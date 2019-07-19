#pragma once

#include "ecs/Subscription.hh"

#include <algorithm>

namespace ecs {

	inline Subscription::Subscription()
	{}

	inline Subscription::Subscription(EntityManager *manager,
		std::list<GenericEntityCallback> *list,
		std::list<GenericEntityCallback>::iterator &c)
		: manager(manager), entityConnectionList(list), entityConnection(c)
	{}

	inline Subscription::Subscription(EntityManager *manager,
		std::list<GenericCallback> *list,
		std::list<GenericCallback>::iterator &c)
		: manager(manager), connectionList(list), connection(c)
	{}

	inline bool Subscription::IsActive() const
	{
		return entityConnectionList != nullptr || connectionList != nullptr;
	}

	inline void Subscription::Unsubscribe()
	{
		if (manager != nullptr)
		{
			std::lock_guard<std::recursive_mutex> lock(manager->signalLock);
			
			if (entityConnectionList != nullptr)
			{
				entityConnectionList->erase(entityConnection);
				entityConnectionList = nullptr;
			}
			else if (connectionList != nullptr)
			{
				connectionList->erase(connection);
				connectionList = nullptr;
			}
		}
	}
}
