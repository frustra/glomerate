#include <iostream>
#include <unordered_map>
#include <stdexcept>

#include <gtest/gtest.h>

#include "Ecs.hh"
#include "Test.hh"

namespace test
{
	int positionsDestroyed = 0;

	typedef struct Eater
	{
		bool hungry;
		uint thingsEaten;
	} Eater;

	class EcsBasicIterateWithComponents : public ::testing::Test
	{
	protected:
		std::unordered_map<ecs::Entity, bool> entsFound;

		ecs::EntityManager em;
		ecs::Entity ePos1;
		ecs::Entity ePos2;
		ecs::Entity ePosEat;
		ecs::Entity eEat;
		ecs::Entity eNoComps;

		virtual void SetUp()
		{
			ePos1 = em.NewEntity();
			ePos2 = em.NewEntity();
			ePosEat = em.NewEntity();
			eEat = em.NewEntity();
			eNoComps = em.NewEntity();

			ePos1.Set<Position>();
			ePos2.Set<Position>();
			ePosEat.Set<Position>();

			ePosEat.Set<Eater>();
			eEat.Set<Eater>();
		}

		void ExpectPositionEntitiesFound()
		{
			// found entities with the component
			EXPECT_TRUE(entsFound.count(ePos1) == 1 && entsFound[ePos1] == true);
			EXPECT_TRUE(entsFound.count(ePos2) == 1 && entsFound[ePos2] == true);
			EXPECT_TRUE(entsFound.count(ePosEat) == 1 && entsFound[ePosEat] == true);

			// did not find entities without the component
			EXPECT_FALSE(entsFound.count(eEat) == 1 && entsFound[eEat] == true);
			EXPECT_FALSE(entsFound.count(eNoComps) == 1 && entsFound[eNoComps] == true);
		}
	};

	TEST(EcsBasic, NullEntity)
	{
		ecs::EntityManager em;
		ecs::Entity e1;
		ecs::Entity e2(&em, ecs::Entity::Id());

		EXPECT_FALSE(e1.Valid());
		EXPECT_FALSE(e2.Valid());
	}

	TEST(EcsBasic, CreateDestroyEntity)
	{
		ecs::EntityManager em;
		ecs::Entity e = em.NewEntity();

		EXPECT_TRUE(e.Valid());
		e.Destroy();

		EXPECT_FALSE(e.Valid());
	}

	TEST(EcsBasic, AddRemoveComponent)
	{
		ecs::EntityManager em;
		ecs::Entity e = em.NewEntity();

		e.Set<Position>();

		ASSERT_TRUE(e.Has<Position>());

		positionsDestroyed = 0;
		e.Remove<Position>();

		ASSERT_FALSE(e.Has<Position>());
		ASSERT_ANY_THROW(e.Get<Position>());
		ASSERT_EQ(positionsDestroyed, 1);
	}

	TEST(EcsBasic, ConstructComponent)
	{
		ecs::EntityManager em;
		ecs::Entity e = em.NewEntity();

		e.Set<Position>(1, 2);
		const Position &pos = e.Get<Position>();

		ASSERT_EQ(pos.x, 1);
		ASSERT_EQ(pos.y, 2);
	}

	TEST(EcsBasic, RemoveAllComponents)
	{
		ecs::EntityManager em;
		ecs::Entity e = em.NewEntity();

		e.Set<Position>();
		e.Set<Eater>();

		ASSERT_TRUE(e.Has<Position>());
		ASSERT_TRUE(e.Has<Eater>());

		positionsDestroyed = 0;
		e.RemoveAllComponents();

		ASSERT_FALSE(e.Has<Position>());
		ASSERT_FALSE(e.Has<Eater>());
		ASSERT_EQ(positionsDestroyed, 1);
	}

	TEST_F(EcsBasicIterateWithComponents, MultiComponentTemplateIteration)
	{
		for (ecs::Entity ent : em.EntitiesWith<Eater, Position>())
		{
			// ensure we can retrieve these components
			ent.Get<Eater>();
			ent.Get<Position>();

			entsFound[ent] = true;
		}

		EXPECT_TRUE(entsFound.count(ePosEat) == 1 && entsFound[ePosEat] == true);
		EXPECT_EQ(1u, entsFound.size()) << "should have only found one entity";
	}

	TEST_F(EcsBasicIterateWithComponents, TemplateIteration)
	{
		for (ecs::Entity ent : em.EntitiesWith<Position>())
		{
			entsFound[ent] = true;
		}

		ExpectPositionEntitiesFound();
	}

	TEST_F(EcsBasicIterateWithComponents, MaskIteration)
	{
		auto compMask = em.CreateComponentMask<Position>();
		for (ecs::Entity ent : em.EntitiesWith(compMask))
		{
			entsFound[ent] = true;
		}

		ExpectPositionEntitiesFound();
	}

	/**
	 * This is a test for the fix in commit ea35fb59156261ff16f9993dd5c40410aefd335e
	 * The bug was that when iterating over multiple component types the
	 * "begin" iterator under the hood would start at the beginning of a component
	 * pool instead of advancing to the first component that belonged to an entity
	 * that had all the components specified.
	 */
	TEST(EcsBugFix, IterateOverComponentsSkipsFirstInvalidComponents)
	{
		ecs::EntityManager em;
		ecs::Entity ePos1 = em.NewEntity();
		ecs::Entity ePos2 = em.NewEntity();
		ecs::Entity ePosEater = em.NewEntity();
		ecs::Entity eEater1 = em.NewEntity();
		ecs::Entity eEater2 = em.NewEntity();
		ecs::Entity eEater3 = em.NewEntity();

		// ensure that first 2 components in the Position pool don't have Eater components
		ePos1.Set<Position>();
		ePos2.Set<Position>();

		// create the combination entity we will query for
		ePosEater.Set<Position>();
		ePosEater.Set<Eater>();

		// create more Eater components than Position components so that
		// when we iterate over Position, Eater, we will iterate through
		// the Position pool instead of the Eater pool
		eEater1.Set<Eater>();
		eEater2.Set<Eater>();
		eEater3.Set<Eater>();

		for (auto e : em.EntitiesWith<Position, Eater>())
		{
			// without bugfix, the Eater assertion will fail
			ASSERT_TRUE(e.Has<Eater>()) << " bug has regressed";
			ASSERT_TRUE(e.Has<Position>());
		}
	}

	/**
	 * Tests that there is no error when deleting the current entity while
	 * iterating over a set of entities
	 */
	TEST(EcsBugFix, DeleteEntityWhileIterating)
	{
		ecs::EntityManager em;
		ecs::Entity ePos1 = em.NewEntity();
		ecs::Entity ePos2 = em.NewEntity();

		ePos1.Set<Position>();
		ePos2.Set<Position>();

		// exception is normally raised after exiting the loop
		// (EntityCollection / iterate lock is destroyed which triggers a list of "soft deleted"
		// components to be deleted for real)
		try {
			positionsDestroyed = 0;
			for (auto e : em.EntitiesWith<Position>())
			{
				e.Destroy();
			}
			ASSERT_EQ(positionsDestroyed, 2);
		}
		catch (const std::out_of_range & ex)
		{
			// for some reason the std::out_of_range doesn't get caught and this block never runs,
			// so if you come here from seeing the test receive an std::out_of_range it means the test failed
			std::cerr << "Err: " << ex.what() << std::endl;
			ASSERT_TRUE(false) << "bug has regressed";
		}
	}

	/**
	 * The situation is that a component is created with certain values and then deleted.
	 * When a new component of the same type is created it was being assigned to the spot
	 * of the old component without having its new values copied into place.
	 */
	TEST(EcsBugFix, DeleteThenAddComponentDoesNotHaveOldComponentValues)
	{
		ecs::EntityManager em;

		ecs::Entity ent = em.NewEntity();

		Position positionComp = ent.Set<Position>(1, 2);
		ASSERT_EQ(Position(1, 2), positionComp) << "sanity check failed";

		positionsDestroyed = 0;
		ent.Remove<Position>();
		ASSERT_EQ(positionsDestroyed, 1);

		Position positionComp2 = ent.Set<Position>(3, 4);
		ASSERT_EQ(Position(3, 4), positionComp2) << "component values not properly set on creation";
	}

	TEST(EcsBasic, AddEntitiesWhileIterating)
	{
		ecs::EntityManager em;
		ecs::Entity e1 = em.NewEntity();
		e1.Set<Position>();

		int entitiesFound = 0;
		for (ecs::Entity ent : em.EntitiesWith<Position>())
		{
			ent.Valid(); // prevent -Wunused-but-set-variable
			entitiesFound++;
			if (entitiesFound == 1)
			{
				ecs::Entity e2 = em.NewEntity();
				e2.Set<Position>();
			}
		}

		ASSERT_EQ(1, entitiesFound) << "Should have only found the entity created before started iterating";
	}

	// test to verify that an entity is not iterated over if it is destroyed
	// before we get to it during iteration
	TEST(EcsBasic, RemoveEntityWhileIterating)
	{
		ecs::EntityManager em;

		ecs::Entity e1 = em.NewEntity();
		e1.Set<Position>();

		ecs::Entity e2 = em.NewEntity();
		e2.Set<Position>();

		int entitiesFound = 0;
		positionsDestroyed = 0;
		for (ecs::Entity ent : em.EntitiesWith<Position>())
		{
			entitiesFound++;
			if (ent == e1)
			{
				e2.Destroy();
			}
			else
			{
				e1.Destroy();
			}
		}
		ASSERT_EQ(1, entitiesFound) <<
			"Should have only found one entity because the other was destroyed";
		ASSERT_EQ(positionsDestroyed, 1);
	}

	// test to verify that an entity is not iterated over if it's component we are interested in
	// is removed before we get to it during iteration
	TEST(EcsBasic, RemoveComponentWhileIterating)
	{
		ecs::EntityManager em;

		ecs::Entity e1 = em.NewEntity();
		e1.Set<Position>();

		ecs::Entity e2 = em.NewEntity();
		e2.Set<Position>();

		int entitiesFound = 0;
		positionsDestroyed = 0;
		for (ecs::Entity ent : em.EntitiesWith<Position>())
		{
			entitiesFound++;
			if (ent == e1)
			{
				e2.Remove<Position>();
			}
			else
			{
				e1.Remove<Position>();
			}
		}

		ASSERT_EQ(1, entitiesFound) <<
			"Should have only found one entity because the other's component was removed before"
			" we got to it during iteration";
		ASSERT_EQ(positionsDestroyed, 1);
	}

	TEST(EcsBasic, RegisterComponentPreventsExceptions)
	{
		ecs::EntityManager em;

		ecs::Entity e = em.NewEntity();

		// assert that exceptions are raised before registering
		ASSERT_THROW(e.Has<Position>(), std::invalid_argument);
		ASSERT_THROW(
			for (auto e : em.EntitiesWith<Position>()) {
				e.Valid();
			},
			std::invalid_argument
		);

		em.RegisterComponentType<Position>();

		// assert that exceptions no longer occur
		ASSERT_NO_THROW(e.Has<Position>());

		ASSERT_NO_THROW(
			for (auto e : em.EntitiesWith<Position>()) {
				e.Valid();
			}
		);
	}

	TEST(EcsBasic, DeleteComponentDoesNotInvalidateOtherComponentHandles)
	{
		ecs::EntityManager em;
		auto e1 = em.NewEntity();
		auto e2 = em.NewEntity();

		e1.Set<Position>(1, 1);
		e2.Set<Position>(2, 2);

		const Position &p2Handle = e2.Get<Position>();

		Position p2before = p2Handle;
		e1.Remove<Position>();
		Position p2now = p2Handle;

		ASSERT_EQ(p2before, p2now);
	}

	TEST(EcsBasic, AddComponentsDoesNotInvalidateOtherComponentHandles)
	{
		ecs::EntityManager em;
		auto e1 = em.NewEntity();
		e1.Set<Position>(1, 1);

		const Position &p2Handle = e1.Get<Position>();
		Position positionBefore = p2Handle;

		for (int i = 0; i < 1000; ++i)
		{
			auto e = em.NewEntity();
			e.Set<Position>(2, 2);
		}

		Position positionAfter = p2Handle;

		ASSERT_EQ(positionBefore, positionAfter);
	}

	TEST(EcsDestroyAll, DestroysMultipleEntities)
	{
		ecs::EntityManager em;

		vector<ecs::Entity> entities;
		for (int i = 0; i < 10; ++i) {
			entities.push_back(em.NewEntity());
		}

		for (auto e : entities) {
			ASSERT_TRUE(e.Valid());
		}

		em.DestroyAll();

		for (auto e : entities) {
			ASSERT_FALSE(e.Valid());
		}
	}

	TEST(EcsDestroyAll, DestroysMultipleEntitiesTwice)
	{
		ecs::EntityManager em;

		vector<ecs::Entity> entities;
		for (int i = 0; i < 2; ++i) {
			entities.clear();

			for (int j = 0; j < 10; ++j) {
				entities.push_back(em.NewEntity());
			}

			for (auto e : entities) {
				ASSERT_TRUE(e.Valid());
			}

			em.DestroyAll();

			for (auto e : entities) {
				ASSERT_FALSE(e.Valid());
			}
		}
	}

	TEST(EcsDestroyAll, NoExceptionsThrownWhenNoEntitiesEverAlive)
	{
		ecs::EntityManager em;
		em.DestroyAll();
	}

	TEST(EcsDestroyAll, NoExceptionsThrownWhenNoEntitiesStillAlive)
	{
		ecs::EntityManager em;

		for (uint i = 0; i < 10; ++i) {
			em.NewEntity();
		}

		em.DestroyAll(); // clear all entities
		em.DestroyAll(); // ensure no exceptions raised
	}

	TEST(EcsRecycle, EntitiesGetRecycledAfterManyAreDestroyed)
	{
		ecs::EntityManager em;
		ecs::Entity e = em.NewEntity();

		uint64 entitiesMade = 0;
		const uint64 tooMany = 1000000;

		while (e.Generation() <= 0 && entitiesMade < tooMany) {
			e.Destroy();
			e = em.NewEntity();
			entitiesMade += 1;
		}

		ASSERT_LT(entitiesMade, tooMany)
			<< "entities were never recycled";
	}

	TEST(EcsRecycle, RecycledEntitiesDontHaveOldComponents)
	{
		ecs::EntityManager em;
		ecs::Entity e = em.NewEntity();
		e.Set<Position>(1, 1);

		uint64 entitiesMade = 1;
		const uint64 tooMany = 1000000;

		while (e.Generation() <= 0 && entitiesMade < tooMany) {
			positionsDestroyed = 0;
			e.Destroy();
			ASSERT_EQ(positionsDestroyed, 1);

			e = em.NewEntity();
			e.Set<Position>(1, 1);
			entitiesMade += 1;
		}

		ASSERT_LT(entitiesMade, tooMany)
			<< "failed to trigger recycling of entities after "
			<< entitiesMade << " were created and destroyed";

		positionsDestroyed = 0;
		e.Destroy();
		ASSERT_EQ(positionsDestroyed, 1);
		e = em.NewEntity();
		entitiesMade += 1;

		ASSERT_GE(e.Generation(), 1u)
			<< "failed to trigger recycling of entities after "
			<< entitiesMade << " were created and destroyed";

		ASSERT_FALSE(e.Has<Position>());
	}
}
