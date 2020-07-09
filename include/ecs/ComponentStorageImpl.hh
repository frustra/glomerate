#pragma once

#include "ecs/ComponentStorage.hh"
#include <type_traits>

// BaseComponentPool::IterateLock
namespace ecs
{
	inline BaseComponentPool::IterateLock::IterateLock(BaseComponentPool &pool): pool(pool)
	{
		pool.toggleSoftRemove(true);
	}

	inline BaseComponentPool::IterateLock::~IterateLock()
	{
		pool.toggleSoftRemove(false);
	}
}

// ComponentPoolEntityCollection
namespace ecs
{
	inline ComponentPoolEntityCollection::ComponentPoolEntityCollection()
		: pool(nullptr), indexes(nullptr), lastCompIndex(0)
	{
	}

	inline ComponentPoolEntityCollection::ComponentPoolEntityCollection(BaseComponentPool &pool)
		: pool(&pool), indexes(nullptr)
	{
		// keep track of the last component at creation time.  This way, if new components
		// are created during iteration they will be added at the end and we will not iterate over them
		lastCompIndex = pool.Size() - 1;
	}

	inline ComponentPoolEntityCollection::ComponentPoolEntityCollection(BaseComponentPool &pool, std::list<size_t> *indexes)
		: pool(&pool), indexes(indexes), lastCompIndex(0)
	{
		// keep track of the last entity at creation time.  This way, if new entities
		// are created during iteration they will be added at the end and we will not iterate over them
		const size_t tmpIndex = BaseComponentPool::INVALID_COMP_INDEX;
		endIter = indexes->emplace(indexes->end(), tmpIndex);
	}

	inline ComponentPoolEntityCollection::ComponentPoolEntityCollection(ComponentPoolEntityCollection &&other)
		: pool(nullptr), indexes(nullptr), lastCompIndex(0)
	{
		pool = other.pool;
		indexes = other.indexes;
		endIter = other.endIter;
		lastCompIndex = other.lastCompIndex;
		other.pool = nullptr;
		other.indexes = nullptr;
		other.lastCompIndex = 0;
	}

	inline ComponentPoolEntityCollection::~ComponentPoolEntityCollection()
	{
		if (indexes != nullptr)
		{
			indexes->erase(endIter);
		}
	}

	inline ComponentPoolEntityCollection::Iterator ComponentPoolEntityCollection::begin()
	{
		if (indexes == nullptr)
		{
			return ComponentPoolEntityCollection::Iterator(pool, 0);
		}
		else
		{
			return ComponentPoolEntityCollection::Iterator(pool, indexes, indexes->begin());
		}
	}

	inline ComponentPoolEntityCollection::Iterator ComponentPoolEntityCollection::end()
	{
		if (indexes == nullptr)
		{
			if (pool == nullptr)
			{
				return ComponentPoolEntityCollection::Iterator(nullptr, 0);
			}
			else
			{
				return ComponentPoolEntityCollection::Iterator(pool, lastCompIndex + 1);
			}
		}
		else
		{
			return ComponentPoolEntityCollection::Iterator(pool, indexes, endIter);
		}
	}
}

// ComponentPoolEntityCollection::Iterator
namespace ecs
{
	inline ComponentPoolEntityCollection::Iterator::Iterator(BaseComponentPool *pool, size_t compIndex)
		: pool(pool), indexes(nullptr), compIndex(compIndex)
	{}

	inline ComponentPoolEntityCollection::Iterator::Iterator(BaseComponentPool *pool, std::list<size_t> *indexes, std::list<size_t>::iterator iter)
		: pool(pool), indexes(indexes), iter(iter), compIndex(0)
	{}

	inline ComponentPoolEntityCollection::Iterator &ComponentPoolEntityCollection::Iterator::operator++()
	{
		Assert(pool != nullptr,
			"Cannot increment entity component iterator for empty collection");

		if (indexes == nullptr)
		{
			compIndex++;
			if (compIndex > pool->Size())
			{
				throw std::runtime_error(
					"Cannot increment entity component iterator more than 1 time past the end of its pool. "
					"You are likely calling operator++ on an EntityCollection::Iterator that is at the end.  "
					"Try comparing it to <your EntityCollection>::end()");
			}
		}
		else
		{
			if (iter == indexes->end())
			{
				throw std::runtime_error(
					"Cannot increment entity component iterator more than 1 time past the end of its pool. "
					"You are likely calling operator++ on an EntityCollection::Iterator that is at the end.  "
					"Try comparing it to <your EntityCollection>::end()");
			}
			iter++;
		}
		return *this;
	}

	inline Entity::Id ComponentPoolEntityCollection::Iterator::operator*()
	{
		Assert(pool != nullptr,
			"trying to access entity that is part of an empty component pool");
		if (indexes == nullptr)
		{
			Assert(compIndex < pool->Size(),
				"trying to access entity that is past the end of this component pool");
			return pool->entityAt(compIndex);
		}
		else
		{
			if (iter == indexes->end())
			{
				throw std::runtime_error(
					"trying to access entity that is past the end of this component pool");
			}
			return *iter == BaseComponentPool::INVALID_COMP_INDEX ? Entity::Id() : pool->entityAt(*iter);
		}
	}

	inline bool ComponentPoolEntityCollection::Iterator::operator==(
		const ComponentPoolEntityCollection::Iterator &other)
	{
		return pool == other.pool && indexes == other.indexes && compIndex == other.compIndex && (indexes == nullptr || iter == other.iter);
	}

	inline bool ComponentPoolEntityCollection::Iterator::operator!=(
		const ComponentPoolEntityCollection::Iterator &other)
	{
		return !(*this == other);
	}
}

// ComponentPool
namespace ecs
{
	template <typename CompType>
	ComponentPool<CompType>::ComponentPool()
	{
		softRemoveMode = false;
	}

	template <typename CompType>
	unique_ptr<BaseComponentPool::IterateLock> ComponentPool<CompType>::CreateIterateLock()
	{
		return unique_ptr<BaseComponentPool::IterateLock>(new BaseComponentPool::IterateLock(*static_cast<BaseComponentPool *>(this)));
	}

	template <typename CompType>
	template <typename ...T>
	const CompType &ComponentPool<CompType>::Set(Entity::Id e, T&&... args)
	{
		size_t newCompIndex = components.size();

		components.emplace_back(e, args...);
		entIndexToCompIndex[e.Index()] = newCompIndex;

		return components.at(newCompIndex).value;
	}

	template <typename CompType>
	void ComponentPool<CompType>::Set(Entity::Id e, CompType &&value)
	{
		size_t newCompIndex = components.size();

		components.emplace_back(e, std::move(value));
		entIndexToCompIndex[e.Index()] = newCompIndex;

		return components.at(newCompIndex).value;
	}

	template <typename CompType>
	const CompType &ComponentPool<CompType>::Get(Entity::Id e) const
	{
		if (!HasComponent(e))
		{
			throw runtime_error("entity does not have a component of type "
				+ string(typeid(CompType).name()));
		}

		size_t compIndex = entIndexToCompIndex.at(e.Index());
		return components.at(compIndex).value;
	}

	template <typename CompType>
	void ComponentPool<CompType>::Remove(Entity::Id e)
	{
		if (!HasComponent(e))
		{
			throw std::runtime_error("cannot remove component because the entity does not have one");
		}

		size_t removeIndex = entIndexToCompIndex.at(e.Index());
		entIndexToCompIndex.at(e.Index()) = BaseComponentPool::INVALID_COMP_INDEX;

		if (softRemoveMode)
		{
			softRemove(removeIndex);
		}
		else
		{
			remove(removeIndex);
		}
	}

	template <typename CompType>
	void ComponentPool<CompType>::remove(size_t compIndex)
	{
		if (compIndex < components.size() - 1)
		{
			// Swap this component to the end
			auto &validComponent = (components.at(compIndex) = std::move(components.back()));

			// update the entity -> component index mapping of swapped component if it's entity still exists
			// (Entity could have been deleted while iterating over entities so the component was only soft-deleted till now)
			auto it = entIndexToCompIndex.find(validComponent.eid.Index());
			if (it != entIndexToCompIndex.end())
			{
				it->second = compIndex;
			}
		}
		components.pop_back();
	}

	template <typename CompType>
	void ComponentPool<CompType>::softRemove(size_t compIndex)
	{
		// mark the component as the "Null" Entity and add this component index to queue of
		// components to be deleted when "soft remove" mode is disabled.
		// "Null" Entities will never be iterated over
		Assert(compIndex < components.size());

		components.at(compIndex).eid = Entity::Id();
		softRemoveCompIndexes.push(compIndex);
	}

	template <typename CompType>
	bool ComponentPool<CompType>::HasComponent(Entity::Id e) const
	{
		auto compIndex = entIndexToCompIndex.find(e.Index());
		return compIndex != entIndexToCompIndex.end() && compIndex->second != BaseComponentPool::INVALID_COMP_INDEX;
	}

	template <typename CompType>
	size_t ComponentPool<CompType>::Size() const
	{
		return components.size();
	}

	template <typename CompType>
	void ComponentPool<CompType>::toggleSoftRemove(bool enabled)
	{
		if (enabled)
		{
			if (softRemoveMode)
			{
				throw runtime_error("soft remove mode is already active");
			}
		}
		else
		{
			if (!softRemoveMode)
			{
				throw runtime_error("soft remove mode is already inactive");
			}

			// must perform proper removes for everything that has been "soft removed"
			while (!softRemoveCompIndexes.empty())
			{
				size_t compIndex = softRemoveCompIndexes.front();
				softRemoveCompIndexes.pop();
				remove(compIndex);
			}
		}

		softRemoveMode = enabled;
	}

	template <typename CompType>
	Entity::Id ComponentPool<CompType>::entityAt(size_t compIndex)
	{
		Assert(compIndex < components.size());
		return components[compIndex].eid;
	}

	template <typename CompType>
	ComponentPoolEntityCollection ComponentPool<CompType>::Entities()
	{
		return ComponentPoolEntityCollection(*this);
	}
}

// KeyedComponentPool
namespace ecs
{
	template <typename KeyType>
	KeyedComponentPool<KeyType>::KeyedComponentPool()
	{
	}

	template <typename KeyType>
	template <typename ...T>
	const KeyType &KeyedComponentPool<KeyType>::Set(Entity::Id e, T&&... args)
	{
		// Add the entity to the regular component pool
		size_t newCompIndex = ComponentPool<KeyType>::components.size();

		ComponentPool<KeyType>::components.emplace_back(e, KeyType(args...));
		ComponentPool<KeyType>::entIndexToCompIndex[e.Index()] = newCompIndex;

		// Add the entity to the keyed component pool
		auto &component = ComponentPool<KeyType>::components.at(newCompIndex);
		if (compKeyToCompIndex.count(component.value) > 0)
		{
			component.keyedList = compKeyToCompIndex[component.value];
			component.keyedIterator = component.keyedList->emplace(component.keyedList->end(), newCompIndex);
		}
		else
		{
			component.keyedList = std::make_shared<std::list<size_t>>(std::initializer_list<size_t>({ newCompIndex }));
			component.keyedIterator = component.keyedList->begin();
			compKeyToCompIndex.emplace(component.value, component.keyedList);
		}

		return component.value;
	}

	template <typename KeyType>
	void KeyedComponentPool<KeyType>::remove(size_t compIndex)
	{
		auto &componentToRemove = ComponentPool<KeyType>::components.at(compIndex);
		if (componentToRemove.keyedList != nullptr)
		{
			componentToRemove.keyedList->erase(componentToRemove.keyedIterator);
			if (componentToRemove.keyedList->empty())
			{
				// There are no more entities in this component pool, remove the pool.
				compKeyToCompIndex.erase(componentToRemove.value);
			}
			componentToRemove.keyedList = nullptr;
		}


		if (compIndex < ComponentPool<KeyType>::components.size() - 1)
		{
			// Swap this component to the end
			auto validComponent = (ComponentPool<KeyType>::components.at(compIndex) = std::move(ComponentPool<KeyType>::components.back()));

			// Update the keyed component pool index if necessary
			if (validComponent.keyedList != nullptr)
			{
				*validComponent.keyedIterator = compIndex;
			}

			// update the entity -> component index mapping of swapped component if it's entity still exists
			// (Entity could have been deleted while iterating over entities so the component was only soft-deleted till now)
			auto it = ComponentPool<KeyType>::entIndexToCompIndex.find(validComponent.eid.Index());
			if (it != ComponentPool<KeyType>::entIndexToCompIndex.end())
			{
				it->second = compIndex;
			}
		}
		ComponentPool<KeyType>::components.pop_back();
	}

	template <typename KeyType>
	ComponentPoolEntityCollection KeyedComponentPool<KeyType>::KeyedEntities(const KeyType &key)
	{
		if (compKeyToCompIndex.count(key) > 0)
		{
			return ComponentPoolEntityCollection(*this, compKeyToCompIndex[key].get());
		}
		else
		{
			return ComponentPoolEntityCollection();
		}
	}

	template <typename KeyType>
	Entity::Id KeyedComponentPool<KeyType>::KeyedEntity(const KeyType &key)
	{
		if (compKeyToCompIndex.count(key) == 1)
		{
			return *(ComponentPoolEntityCollection(*this, compKeyToCompIndex[key].get()).begin());
		}
		else
		{
			return Entity::Id();
		}
	}
}
