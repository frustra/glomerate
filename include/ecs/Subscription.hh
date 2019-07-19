#pragma once

#include <list>

namespace ecs {

	class Entity;
	class EntityManager;

	typedef std::function<void(Entity, void *)> GenericEntityCallback;
	typedef std::function<void(void *)> GenericCallback;

	/**
	 * Lightweight class that represents a subscription to a type of event
	 */
	class Subscription {
	public:
		Subscription();
		Subscription(EntityManager *manaager, std::list<GenericEntityCallback> *list, std::list<GenericEntityCallback>::iterator &c);
		Subscription(EntityManager *manaager, std::list<GenericCallback> *list, std::list<GenericCallback>::iterator &c);
		Subscription(const Subscription &other) = default;

		/**
		 * Returns true if the registered callback will still be called
		 * when its subscribed event occurs.
		 * Always safe to call.
		 */
		bool IsActive() const;

		/**
		 * Terminates this subscription so that the registered callback will
		 * stop being called when new events are generated. It is safe to
		 * deallocate the associated callback after calling this function.
		 * Always safe to call, even if the subscription is not active.
		 */
		void Unsubscribe();
	private:
		EntityManager *manager = nullptr;
		std::list<GenericEntityCallback> *entityConnectionList = nullptr;
		std::list<GenericEntityCallback>::iterator entityConnection;
		std::list<GenericCallback> *connectionList = nullptr;
		std::list<GenericCallback>::iterator connection;
	};
}