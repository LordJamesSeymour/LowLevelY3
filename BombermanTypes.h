#pragma once

#include <SFML/Graphics.hpp>

struct BombermanGridPosition
{
	int col = 0;
	int row = 0;
};

inline bool operator==(const BombermanGridPosition& a, const BombermanGridPosition& b)
{
	return a.col == b.col && a.row == b.row;
}

inline bool operator!=(const BombermanGridPosition& a, const BombermanGridPosition& b)
{
	return !(a == b);
}

enum class BombermanDirection
{
	Down,
	Up,
	Left,
	Right
};

enum class BombermanTileType
{
	Floor,
	SolidWall,
	BreakableBlock,
	Exit
};

enum class BombermanExplosionTileType
{
	Center,
	Horizontal,
	Vertical
};

struct BombermanExplosionTile
{
	BombermanGridPosition gridPosition;
	BombermanExplosionTileType type = BombermanExplosionTileType::Center;
};