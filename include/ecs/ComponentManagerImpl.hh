#pragma once

#include "ecs/ComponentManager.hh"
#include <type_traits>

namespace ecs
{
	inline size_t ComponentManager::ComponentTypeCount() const
	{
		return componentPools.size();
	}

	template <typename CompType, typename ...T>
	const CompType &ComponentManager::Set(Entity::Id e, T&&... args)
	{
		std::type_index compType = typeid(CompType);

		uint32 compIndex;

		try
		{
			compIndex = compTypeToCompIndex.at(compType);
		}
		// component never seen before, add it to the collection
		catch (const std::out_of_range &e)
		{
			RegisterComponentType<CompType>();
			compIndex = compTypeToCompIndex.at(compType);
		}

		Assert(entCompMasks.size() > e.Index(), "entity does not have a component mask");

		auto &compMask = entCompMasks.at(e.Index());
		compMask.set(compIndex);

		auto componentPool = static_cast<ComponentPool<CompType>*>(componentPools.at(compIndex));
		return componentPool->Set(e, args...);
	}

	template <typename KeyType, typename ...T>
	const KeyType &ComponentManager::SetKey(Entity::Id e, T&&... args)
	{
		std::type_index compType = typeid(KeyType);

		uint32 compIndex;

		try
		{
			compIndex = compTypeToCompIndex.at(compType);
		}
		// component never seen before, add it to the collection
		catch (const std::out_of_range &e)
		{
			RegisterKeyedComponentType<KeyType>();
			compIndex = compTypeToCompIndex.at(compType);
		}

		Assert(entCompMasks.size() > e.Index(), "entity does not have a component mask");

		auto &compMask = entCompMasks.at(e.Index());
		compMask.set(compIndex);

		auto componentPool = dynamic_cast<KeyedComponentPool<KeyType>*>(componentPools.at(compIndex));
		return componentPool->Set(e, args...);
	}

	template <typename CompType>
	void ComponentManager::Remove(Entity::Id e)
	{
		std::type_index tIndex = typeid(CompType);
		if (compTypeToCompIndex.count(tIndex) == 0)
		{
			throw UnrecognizedComponentType(tIndex);
		}

		uint32 compIndex = compTypeToCompIndex.at(tIndex);

		auto &compMask = entCompMasks.at(e.Index());
		if (compMask[compIndex] == false)
		{
			throw runtime_error("entity does not have a component of type "
				+ string(tIndex.name()));
		}

		componentPools.at(compIndex)->Remove(e);
		compMask.reset(compIndex);
	}

	template <typename CompType>
	bool ComponentManager::Has(Entity::Id e) const
	{
		std::type_index compType = typeid(CompType);
		if (compTypeToCompIndex.count(compType) == 0)
		{
			throw UnrecognizedComponentType(compType);
		}

		auto compIndex = compTypeToCompIndex.at(compType);
		return entCompMasks.at(e.Index())[compIndex];
	}

	template <typename KeyType>
	bool ComponentManager::Has(Entity::Id e, const KeyType &key) const
	{
		std::type_index compType = typeid(KeyType);
		if (compTypeToCompIndex.count(compType) == 0)
		{
			throw UnrecognizedComponentType(compType);
		}

		auto compIndex = compTypeToCompIndex.at(compType);
		if (entCompMasks.at(e.Index())[compIndex])
		{
			auto *compPool = dynamic_cast<KeyedComponentPool<KeyType>*>(componentPools.at(compIndex));
			return key == compPool->Get(e);
		}
		return false;
	}

	template <typename CompType>
	const CompType &ComponentManager::Get(Entity::Id e) const
	{
		if (!Has<CompType>(e))
		{
			throw runtime_error("entity does not have a component of type "
				+ string(typeid(CompType).name()));
		}

		auto compIndex = compTypeToCompIndex.at(typeid(CompType));
		auto *compPool = static_cast<ComponentPool<CompType>*>(componentPools.at(compIndex));
		return compPool->Get(e);
	}

	template <typename CompType>
	void ComponentManager::RegisterComponentType()
	{
		std::type_index compType = typeid(CompType);

		if (compTypeToCompIndex.count(compType) != 0)
		{
			std::stringstream ss;
			ss << "component type " << string(compType.name()) << " is already registered";
			throw std::runtime_error(ss.str());
		}

		uint32 compIndex = componentPools.size();
		compTypeToCompIndex[compType] = compIndex;
		componentPools.push_back(new ComponentPool<CompType>());
	}

	template <typename KeyType>
	void ComponentManager::RegisterKeyedComponentType()
	{
		std::type_index compType = typeid(KeyType);

		if (compTypeToCompIndex.count(compType) != 0)
		{
			std::stringstream ss;
			ss << "component type " << string(compType.name()) << " is already registered";
			throw std::runtime_error(ss.str());
		}

		uint32 compIndex = componentPools.size();
		compTypeToCompIndex[compType] = compIndex;
		componentPools.push_back(new KeyedComponentPool<KeyType>());
	}

	template <typename ...CompTypes>
	ComponentManager::ComponentMask ComponentManager::CreateMask()
	{
		ComponentManager::ComponentMask mask;
		if (sizeof...(CompTypes) == 0)
		{
			return mask;
		}
		return SetMask<CompTypes...>(mask);
	}

	template <typename ...CompTypes>
	ComponentManager::ComponentMask &ComponentManager::SetMask(ComponentMask &mask)
	{
		return setMask(mask, std::type_index(typeid(CompTypes))...);
	}

	template <typename TypeId>
	ComponentManager::ComponentMask &ComponentManager::setMask(ComponentMask &mask, const TypeId &stdTypeId)
	{
		if (compTypeToCompIndex.count(stdTypeId) == 0)
		{
			throw invalid_argument(string(stdTypeId.name()) + " is an invalid component type, it is unknown to the system.");
		}
		auto compIndex = compTypeToCompIndex.at(stdTypeId);
		mask.set(compIndex);
		return mask;
	}

	template <typename TypeId, typename ...OtherTypeIds>
	ComponentManager::ComponentMask &ComponentManager::setMask(ComponentManager::ComponentMask &mask,
			const TypeId &stdTypeId, const OtherTypeIds &... stdTypeIds)
	{
		setMask(mask, stdTypeId);
		return setMask(mask, stdTypeIds...);
	}

	inline void ComponentManager::RemoveAll(Entity::Id e)
	{
		Assert(entCompMasks.size() > e.Index(), "entity does not have a component mask");

		auto &compMask = entCompMasks.at(e.Index());
		for (size_t i = 0; i < componentPools.size(); ++i)
		{
			if (compMask[i])
			{
				componentPools.at(i)->Remove(e);
				compMask.reset(i);
			}
		}

		Assert(compMask == ComponentMask(),
			"component mask not blank after removing all components");
	}
}
