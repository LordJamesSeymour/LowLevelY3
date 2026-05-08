#pragma once

struct BombermanGridPosition
{
	int col = 0;
	int row = 0;

	bool operator==(const BombermanGridPosition& other) const
	{
		return col == other.col && row == other.row;
	}

	bool operator!=(const BombermanGridPosition& other) const
	{
		return !(*this == other);
	}
};

enum class BombermanDirection
{
	Up,
	Down,
	Left,
	Right
};

enum class BombermanExplosionTileType
{
	Center,
	Horizontal,
	HorizontalEnd,
	Vertical,
	VerticalEnd
};

struct BombermanExplosionTile
{
	BombermanGridPosition gridPosition{ 0, 0 };
	BombermanExplosionTileType type = BombermanExplosionTileType::Center;

	bool flipX = false;
	bool flipY = false;
};