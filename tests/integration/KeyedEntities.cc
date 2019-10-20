#include <gtest/gtest.h>

#include <functional>

#include "Ecs.hh"

namespace test
{

	typedef struct Position
	{
		Position() {}
		Position(int x, int y) : x(x), y(y) {}
		bool operator==(const Position & other) const { return x == other.x && y == other.y; }
		int x;
		int y;
	} Position;
}

namespace std
{
    template<>
    struct hash<test::Position>
    {
        size_t operator()(const test::Position & p) const
        {
            return std::hash<int>{}(p.x) ^ (std::hash<int>{}(p.y) << 1);
        }
    };
}

namespace test
{

	class EcsKeyedIterateWithComponents : public ::testing::Test
	{
	protected:
		std::unordered_map<ecs::Entity, bool> entsFound;

		ecs::EntityManager em;
		ecs::Entity ePos1;
		ecs::Entity ePos2;
		ecs::Entity ePos2Dup;
		ecs::Entity ePos3;
		ecs::Entity ePosNoKey;

		virtual void SetUp()
		{
			ePos1 = em.NewEntity();
			ePos2 = em.NewEntity();
			ePos2Dup = em.NewEntity();
			ePos3 = em.NewEntity();
			ePosNoKey = em.NewEntity();

			ePos1.AssignKey<Position>(1, 2);
			ePos2.AssignKey<Position>(2, 2);
			ePos2Dup.AssignKey<Position>(2, 2);
			ePos2Dup.AssignKey<std::string>("hello");
			ePos3.AssignKey<Position>(2, 3);
			ePosNoKey.Assign<Position>(2, 3);
		}

		void ExpectEntityFound(ecs::Entity ent, bool found = true)
		{
            if (found)
            {
			    EXPECT_TRUE(entsFound.count(ent) == 1 && entsFound[ent] == true);
            }
            else
            {
			    EXPECT_TRUE(entsFound.count(ent) == 0);
            }
		}

		void ExpectPositionEntitiesFound()
		{
			// found entities with the component
            ExpectEntityFound(ePos1);
            ExpectEntityFound(ePos2);
            ExpectEntityFound(ePos2Dup);
            ExpectEntityFound(ePos3);
            ExpectEntityFound(ePosNoKey);
		    EXPECT_EQ(5u, entsFound.size());
		}

		void ExpectPositionEntitiesFound(const Position &key)
		{
			// found entities with the component
            ExpectEntityFound(ePos1, key == Position(1, 2));
            ExpectEntityFound(ePos2, key == Position(2, 2));
            ExpectEntityFound(ePos2Dup, key == Position(2, 2));
            ExpectEntityFound(ePos3, key == Position(2, 3));
            ExpectEntityFound(ePosNoKey, false);
		}
	};

	TEST(EcsKeyed, AddRemoveComponent)
	{
		ecs::EntityManager em;
		ecs::Entity e = em.NewEntity();

		e.AssignKey<std::string>("hello");

		ASSERT_TRUE(e.Has<std::string>());
		ASSERT_TRUE(e.Has<std::string>("hello"));
		ASSERT_FALSE(e.Has<std::string>("world"));

		e.Remove<std::string>();

		ASSERT_FALSE(e.Has<std::string>());
		ASSERT_FALSE(e.Has<std::string>("hello"));
		ASSERT_ANY_THROW(e.Get<std::string>());
	}

	TEST(EcsKeyed, ConstructComponent)
	{
		ecs::EntityManager em;
		ecs::Entity e = em.NewEntity();

		e.AssignKey<std::string>("hello");
		ecs::Handle<std::string> name = e.Get<std::string>();

		ASSERT_EQ(*name, "hello");
	}

	TEST(EcsKeyed, RemoveAllComponents)
	{
		ecs::EntityManager em;
		ecs::Entity e = em.NewEntity();

		e.AssignKey<std::string>("hello");
		e.AssignKey<Position>(1, 2);

		ASSERT_TRUE(e.Has<std::string>());
		ASSERT_TRUE(e.Has<Position>());

		e.RemoveAllComponents();

		ASSERT_FALSE(e.Has<std::string>());
		ASSERT_FALSE(e.Has<Position>());
	}

	TEST_F(EcsKeyedIterateWithComponents, MultiComponentTemplateIteration)
	{
		for (ecs::Entity ent : em.EntitiesWith<Position, std::string>(Position(2, 2)))
		{
			// ensure we can retrieve these components
			auto name = ent.Get<std::string>();
			auto position = ent.Get<Position>();

            ASSERT_EQ(*name, "hello");
            ASSERT_EQ(position->x, 2);
            ASSERT_EQ(position->y, 2);

			entsFound[ent] = true;
		}

		EXPECT_TRUE(entsFound.count(ePos2Dup) == 1 && entsFound[ePos2Dup] == true);
		EXPECT_EQ(1u, entsFound.size()) << "should have only found one entity";
	}

	TEST_F(EcsKeyedIterateWithComponents, MultiKeyTemplateIteration)
	{
		for (ecs::Entity ent : em.EntitiesWith(Position(2, 2)))
		{
			// ensure we can retrieve these components
			auto position = ent.Get<Position>();

            ASSERT_EQ(position->x, 2);
            ASSERT_EQ(position->y, 2);

			entsFound[ent] = true;
		}

        ExpectPositionEntitiesFound(Position(2, 2));
	}

	TEST_F(EcsKeyedIterateWithComponents, MultiTemplateIterationNoKey)
	{
		for (ecs::Entity ent : em.EntitiesWith(Position(2, 3)))
		{
			// ensure we can retrieve these components
			auto position = ent.Get<Position>();

            ASSERT_EQ(position->x, 2);
            ASSERT_EQ(position->y, 3);

			entsFound[ent] = true;
		}

        ExpectPositionEntitiesFound(Position(2, 3));
	}

	TEST_F(EcsKeyedIterateWithComponents, TemplateIteration)
	{
		for (ecs::Entity ent : em.EntitiesWith<Position>())
		{
			entsFound[ent] = true;
		}

		ExpectPositionEntitiesFound();
	}

	TEST_F(EcsKeyedIterateWithComponents, MaskIteration)
	{
		auto compMask = em.CreateComponentMask<Position>();
		for (ecs::Entity ent : em.EntitiesWith(compMask))
		{
			entsFound[ent] = true;
		}

		ExpectPositionEntitiesFound();
	}

	TEST(EcsKeyed, AddEntitiesWhileIterating)
	{
		ecs::EntityManager em;
		ecs::Entity e1 = em.NewEntity();
		e1.AssignKey<Position>(1, 2);

		int entitiesFound = 0;
		for (ecs::Entity ent : em.EntitiesWith(Position(1, 2)))
		{
			ent.Valid(); // prevent -Wunused-but-set-variable
			entitiesFound++;
			if (entitiesFound == 1)
			{
				ecs::Entity e2 = em.NewEntity();
				e2.AssignKey<Position>(Position(1, 2));
			}
		}

		ASSERT_EQ(1, entitiesFound) << "Should have only found the entity created before started iterating";
	}

	// test to verify that an entity is not iterated over if it is destroyed
	// before we get to it during iteration
	TEST(EcsKeyed, RemoveEntityWhileIterating)
	{
		ecs::EntityManager em;

		ecs::Entity e1 = em.NewEntity();
		e1.AssignKey<Position>(1, 2);

		ecs::Entity e2 = em.NewEntity();
		e2.AssignKey<Position>(1, 2);

		int entitiesFound = 0;
		for (ecs::Entity ent : em.EntitiesWith(Position(1, 2)))
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
	}

	// test to verify that an entity is not iterated over if it's component we are interested in
	// is removed before we get to it during iteration
	TEST(EcsKeyed, RemoveComponentWhileIterating)
	{
		ecs::EntityManager em;

		ecs::Entity e1 = em.NewEntity();
		e1.AssignKey<Position>(1, 2);

		ecs::Entity e2 = em.NewEntity();
		e2.AssignKey<Position>(1, 2);

		int entitiesFound = 0;
		for (ecs::Entity ent : em.EntitiesWith(Position(1, 2)))
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
	}

	TEST(EcsKeyed, RegisterComponentPreventsExceptions)
	{
		ecs::EntityManager em;

		ecs::Entity e = em.NewEntity();

		// assert that exceptions are raised before registering
		ASSERT_THROW(e.Has<Position>(Position(1, 2)), std::invalid_argument);
		ASSERT_THROW(
			for (auto e : em.EntitiesWith<Position>()) {
				e.Valid();
			},
			std::invalid_argument
		);

		em.RegisterKeyedComponentType<Position>();

		// assert that exceptions no longer occur
		ASSERT_NO_THROW(e.Has(Position(1, 2)));

		ASSERT_NO_THROW(
			for (auto e : em.EntitiesWith<Position>(Position(1, 2))) {
				e.Valid();
			}
		);
	}

	TEST(EcsKeyedRecycle, RecycledEntitiesDontHaveOldComponents)
	{
		ecs::EntityManager em;
		ecs::Entity e = em.NewEntity();
		e.AssignKey<Position>(1, 1);

		uint64 entitiesMade = 1;
		const uint64 tooMany = 1000000;

		while (e.Generation() <= 0 && entitiesMade < tooMany) {
			e.Destroy();
			e = em.NewEntity();
			e.AssignKey<Position>(1, 1);
			entitiesMade += 1;
		}

		ASSERT_LT(entitiesMade, tooMany)
			<< "failed to trigger recycling of entities after "
			<< entitiesMade << " were created and destroyed";

		e.Destroy();
		e = em.NewEntity();
		entitiesMade += 1;

		ASSERT_GE(e.Generation(), 1u)
			<< "failed to trigger recycling of entities after "
			<< entitiesMade << " were created and destroyed";

		ASSERT_FALSE(e.Has<Position>());
	}
}
