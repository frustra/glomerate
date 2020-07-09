#pragma once

namespace test
{
	extern int positionsDestroyed;

	typedef struct Position
	{
		Position() {}
		Position(int x, int y) : x(x), y(y) {}
		~Position() { positionsDestroyed++; }
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
