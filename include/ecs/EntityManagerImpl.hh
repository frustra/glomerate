#pragma once

#include "ecs/EntityManager.hh"
#include "ecs/Entity.hh"
#include "ecs/Handle.hh"
#include "ecs/EntityDestruction.hh"
#include <type_traits>

// EntityManager
namespace ecs
{
	template <typename CompType, typename ...T>
	Handle<CompType> EntityManager::Assign(Entity::Id e, T... args)
	{
		return compMgr.Assign<CompType>(e, args...);
	}

	template <typename KeyType, typename ...T>
	Handle<KeyType> EntityManager::AssignKey(Entity::Id e, T... args)
	{
		return compMgr.AssignKey<KeyType>(e, args...);
	}

	template <typename CompType>
	void EntityManager::Remove(Entity::Id e)
	{
		compMgr.Remove<CompType>(e);
	}

	template <typename CompType>
	bool EntityManager::Has(Entity::Id e) const
	{
		return compMgr.Has<CompType>(e);
	}

	template <typename KeyType>
	bool EntityManager::Has(Entity::Id e, const KeyType &key) const
	{
		return compMgr.Has<KeyType>(e, key);
	}

	template <typename CompType>
	Handle<CompType> EntityManager::Get(Entity::Id e)
	{
		return compMgr.Get<CompType>(e);
	}

	template <typename ...CompTypes>
	EntityManager::EntityCollection EntityManager::EntitiesWith()
	{
		return EntitiesWith(compMgr.CreateMask<CompTypes...>());
	}

	template <typename KeyType, typename ...CompTypes>
	EntityManager::EntityCollection EntityManager::EntitiesWith(const KeyType &key)
	{
		std::type_index keyType = typeid(KeyType);
		if (compMgr.compTypeToCompIndex.count(keyType) == 0)
		{
			throw UnrecognizedComponentType(keyType);
		}

		auto compIndex = compMgr.compTypeToCompIndex.at(keyType);
		auto compPool = dynamic_cast<KeyedComponentPool<KeyType>*>(compMgr.componentPools.at(compIndex));
		if (compPool)
		{
			return EntityManager::EntityCollection(
				*this,
				compMgr.CreateMask<KeyType, CompTypes...>(),
				compPool->KeyedEntities(key),
				compPool->CreateIterateLock()
			);
		}
		else
		{
			// Return empty collection
			return EntityManager::EntityCollection(*this);
		}
	}

	template <typename KeyType>
	Entity EntityManager::EntityWith(const KeyType &key)
	{
		std::type_index keyType = typeid(KeyType);
		if (compMgr.compTypeToCompIndex.count(keyType) == 0)
		{
			throw UnrecognizedComponentType(keyType);
		}

		auto compIndex = compMgr.compTypeToCompIndex.at(keyType);
		auto compPool = dynamic_cast<KeyedComponentPool<KeyType>*>(compMgr.componentPools.at(compIndex));
		if (compPool)
		{
			return Entity(this, compPool->KeyedEntity(key));
		}
		else
		{
			// Return invalid entity
			return Entity();
		}
	}

	template<typename CompType>
	void EntityManager::RegisterComponentType()
	{
		compMgr.RegisterComponentType<CompType>();
	}

	template<typename KeyType>
	void EntityManager::RegisterKeyedComponentType()
	{
		compMgr.RegisterKeyedComponentType<KeyType>();
	}

	template <typename ...CompTypes>
	ComponentManager::ComponentMask EntityManager::CreateComponentMask()
	{
		return compMgr.CreateMask<CompTypes...>();
	}

	template <typename ...CompTypes>
	ComponentManager::ComponentMask &EntityManager::SetComponentMask(ComponentManager::ComponentMask &mask)
	{
		return compMgr.SetMask<CompTypes...>(mask);
	}

	inline EntityManager::EntityManager()
	{
		// update data structures for the NULL Entity
		compMgr.entCompMasks.resize(1);
		entIndexToGen.push_back(0);

		// NULL entity is special because it isn't considered alive
		// so we don't try to delete it when deleting all entities
		indexIsAlive.push_back(false);
	}

	inline Entity EntityManager::NewEntity()
	{
		eid_t i;
		gen_t gen;
		if (freeEntityIndexes.size() >= ECS_ENTITY_RECYCLE_COUNT)
		{
			i = freeEntityIndexes.front();
			freeEntityIndexes.pop();
			gen = entIndexToGen.at(i);  // incremented at Entity destruction
			Assert(compMgr.entCompMasks[i] == ComponentManager::ComponentMask(),
				"expected comp mask to be reset at destruction but it wasn't");
			indexIsAlive[i] = true;
		}
		else
		{
			i = entIndexToGen.size();
			gen = 0;
			entIndexToGen.push_back(gen);
			indexIsAlive.push_back(true);

			// add a blank comp mask without copying one in
			compMgr.entCompMasks.resize(compMgr.entCompMasks.size() + 1);
		}

		// all 3 of these data structures have 1 entry per Entity index
		Assert(entIndexToGen.size() == indexIsAlive.size());
		Assert(entIndexToGen.size() == compMgr.entCompMasks.size());

		return Entity(this, Entity::Id(i, gen));
	}

	inline void EntityManager::Destroy(Entity::Id e)
	{
		if (!Valid(e))
		{
			std::stringstream ss;
			ss << "entity " << e
			   << " is not valid; it may have already been destroyed.";
			throw std::invalid_argument(ss.str());
		}

		Assert(indexIsAlive[e.Index()] == true);

		// notify any subscribers of this entity's death before killing it
		this->Emit(e, EntityDestruction());

		// detach any subscribers listening for events on this entity
		if (entityEventSignals.count(e) > 0) {
			for (auto &kv : entityEventSignals[e]) {
				kv.second.clear();
			}

			entityEventSignals.erase(e);
		}

		RemoveAllComponents(e);
		entIndexToGen.at(e.Index())++;
		freeEntityIndexes.push(e.Index());
		indexIsAlive[e.Index()] = false;
	}

	inline void EntityManager::DestroyAll()
	{
		for (eid_t i = 1; i < indexIsAlive.size(); ++i) {
			if (indexIsAlive.at(i)) {
				Destroy(Entity::Id(i, entIndexToGen.at(i)));
			}
		}
	}

	template<typename KeyType, typename... CompTypes>
	inline void EntityManager::DestroyAllWith(const KeyType &key) 
	{
		auto entityCollection = EntitiesWith<KeyType, CompTypes...>(key);

		for (auto ent : entityCollection) 
		{
			ent.Destroy();
		}
	}

	inline bool EntityManager::Valid(Entity::Id e) const
	{
		return e && e.Generation() == entIndexToGen.at(e.Index());
	}

	inline void EntityManager::RemoveAllComponents(Entity::Id e)
	{
		compMgr.RemoveAll(e);
	}

	inline EntityManager::EntityCollection EntityManager::EntitiesWith(ComponentManager::ComponentMask compMask)
	{
		// find the smallest size component pool to iterate over
		size_t minSize = ~0;
		int minSizeCompIndex = -1;

		for (size_t i = 0; i < compMgr.ComponentTypeCount(); ++i)
		{
			if (!compMask.test(i))
			{
				continue;
			}

			size_t compSize = compMgr.componentPools.at(i)->Size();

			if (minSizeCompIndex == -1 || compSize < minSize)
			{
				minSize = compSize;
				minSizeCompIndex = i;
			}
		}

		auto smallestCompPool = compMgr.componentPools.at(minSizeCompIndex);

		return EntityManager::EntityCollection(
			*this,
			compMask,
			smallestCompPool->Entities(),
			smallestCompPool->CreateIterateLock()
		);
	}

	template <typename Event>
	void EntityManager::registerEventType()
	{
		std::type_index eventType = typeid(Event);

		if (eventTypeToEventIndex.count(eventType) != 0)
		{
			std::stringstream ss;
			ss << "event type " << string(eventType.name())
			   << " is already registered";
			throw std::runtime_error(ss.str());
		}

		uint32 eventIndex = eventSignals.size();
		eventTypeToEventIndex[eventType] = eventIndex;
		eventSignals.push_back({});
	}

	template <typename Event>
	void EntityManager::registerNonEntityEventType()
	{
		std::type_index eventType = typeid(Event);

		if (eventTypeToNonEntityEventIndex.count(eventType) != 0)
		{
			std::stringstream ss;
			ss << "event type " << string(eventType.name())
			   << " is already registered";
			throw std::runtime_error(ss.str());
		}

		uint32 nonEntityEventIndex = nonEntityEventSignals.size();
		eventTypeToNonEntityEventIndex[eventType] = nonEntityEventIndex;
		nonEntityEventSignals.push_back({});
	}

	template <typename Event>
	Subscription EntityManager::Subscribe(
		std::function<void(const Event &e)> callback)
	{
		// TODO-cs: this shares a lot of code in common with
		// Subscribe(function<void(Entity, const Event &)>), find a way
		// to eliminate the duplicate code.
		std::type_index eventType = typeid(Event);

		uint32 nonEntityEventIndex;

		try
		{
			nonEntityEventIndex = eventTypeToNonEntityEventIndex.at(eventType);
		}
		// Non-Entity Event never seen before, add it to the collection
		catch (const std::out_of_range &e)
		{
			registerNonEntityEventType<Event>();
			nonEntityEventIndex = eventTypeToNonEntityEventIndex.at(eventType);
		}

		std::lock_guard<std::recursive_mutex> lock(signalLock);
		auto &signal = nonEntityEventSignals.at(nonEntityEventIndex);
		auto connection = signal.insert(signal.end(), *reinterpret_cast<GenericCallback *>(&callback));

		return Subscription(this, &signal, connection);
	}

	template <typename Event>
	Subscription EntityManager::Subscribe(
		std::function<void(Entity, const Event &)> callback)
	{
		std::type_index eventType = typeid(Event);

		uint32 eventIndex;

		try
		{
			eventIndex = eventTypeToEventIndex.at(eventType);
		}
		// Event never seen before, add it to the collection
		catch (const std::out_of_range &e)
		{
			registerEventType<Event>();
			eventIndex = eventTypeToEventIndex.at(eventType);
		}

		std::lock_guard<std::recursive_mutex> lock(signalLock);
		auto &signal = eventSignals.at(eventIndex);
		auto connection = signal.insert(signal.end(), *reinterpret_cast<GenericEntityCallback *>(&callback));

		return Subscription(this, &signal, connection);
	}

	template <typename Event>
	Subscription EntityManager::Subscribe(
		std::function<void(Entity, const Event &e)> callback,
		Entity::Id entity)
	{
		auto &signal = entityEventSignals[entity][typeid(Event)];
		auto connection = signal.insert(signal.end(), *reinterpret_cast<GenericEntityCallback *>(&callback));
		return Subscription(this, &signal, connection);
	}

	template <typename Event>
	void EntityManager::Emit(Entity::Id e, const Event &event)
	{
		std::type_index eventType = typeid(Event);
		Entity entity(this, e);

		std::lock_guard<std::recursive_mutex> lock(signalLock);

		// signal the generic Event subscribers
		if (eventTypeToEventIndex.count(eventType) > 0)
		{
			auto eventIndex = eventTypeToEventIndex.at(eventType);
			auto &signal = eventSignals.at(eventIndex);
			auto connection = signal.begin();
			while (connection != signal.end())
			{
				auto callback = (*reinterpret_cast<std::function<void(Entity, const Event &)> *>(&(*connection)));
				connection++;
				callback(entity, event);
			}
		}

		// now signal the entity-specific Event subscribers
		if (entityEventSignals.count(e) > 0) {
			if (entityEventSignals[e].count(eventType) > 0) {
				auto &signal = entityEventSignals[e][typeid(Event)];
				auto connection = signal.begin();
				while (connection != signal.end())
				{
					auto callback = (*reinterpret_cast<std::function<void(Entity, const Event &)> *>(&(*connection)));
					connection++;
					callback(entity, event);
				}
			}
		}
	}

	template <typename Event>
	void EntityManager::Emit(const Event &event)
	{
		std::type_index eventType = typeid(Event);

		std::lock_guard<std::recursive_mutex> lock(signalLock);

		if (eventTypeToNonEntityEventIndex.count(eventType) > 0)
		{
			auto eventIndex = eventTypeToNonEntityEventIndex.at(eventType);
			auto &signal = nonEntityEventSignals.at(eventIndex);
			auto connection = signal.begin();
			while (connection != signal.end())
			{
				auto callback = (*reinterpret_cast<std::function<void(const Event &)> *>(&(*connection)));
				connection++;
				callback(event);
			}
		}
	}
}

// EntityManager::EntityCollection
namespace ecs
{
	inline EntityManager::EntityCollection::EntityCollection(EntityManager &em)
		: em(em), compEntColl()
	{}

	inline EntityManager::EntityCollection::EntityCollection(EntityManager &em,
			const ComponentManager::ComponentMask &compMask,
			ComponentPoolEntityCollection &&compEntColl,
			unique_ptr<BaseComponentPool::IterateLock> &&iLock)
		: em(em), compMask(compMask), compEntColl(std::move(compEntColl)), iLock(std::move(iLock))
	{}

	inline EntityManager::EntityCollection::Iterator EntityManager::EntityCollection::begin()
	{
		return EntityManager::EntityCollection::Iterator(em, compMask, &compEntColl, compEntColl.begin());
	}

	inline EntityManager::EntityCollection::Iterator EntityManager::EntityCollection::end()
	{
		return EntityManager::EntityCollection::Iterator(em, compMask, &compEntColl, compEntColl.end());
	}
}

// EntityManager::EntityCollection::Iterator
namespace ecs
{
	inline EntityManager::EntityCollection::Iterator::Iterator(EntityManager &em,
			const ComponentManager::ComponentMask &compMask,
			ComponentPoolEntityCollection *compEntColl,
			ComponentPoolEntityCollection::Iterator compIt)
		: em(em), compMask(compMask), compEntColl(compEntColl), compIt(compIt)
	{
		// might need to advance this iterator to the first entity that satisfies the mask
		// since *compIt is not guarenteed to satisfy the mask right now
		if (compIt != compEntColl->end())
		{
			Entity::Id e = *compIt;
			auto entCompMask = em.compMgr.entCompMasks.at(e.Index());
			if ((entCompMask & compMask) != compMask)
			{
				this->operator++();
			}
		}
	}

	inline EntityManager::EntityCollection::Iterator &EntityManager::EntityCollection::Iterator::operator++()
	{
		// find the next entity that has all the components specified by this->compMask
		while (++compIt != compEntColl->end())
		{
			Entity::Id e = *compIt;
			auto entCompMask = em.compMgr.entCompMasks.at(e.Index());
			if ((entCompMask & compMask) == compMask)
			{
				break;
			}
		}
		return *this;
	}

	inline bool EntityManager::EntityCollection::Iterator::operator==(const Iterator &other)
	{
		return compMask == other.compMask && compIt == other.compIt;
	}

	inline bool EntityManager::EntityCollection::Iterator::operator!=(const Iterator &other)
	{
		return !(*this == other);
	}

	inline Entity EntityManager::EntityCollection::Iterator::operator*()
	{
		return Entity(&this->em, *compIt);
	}
}
