#pragma once

#include "BombermanTypes.h"

class BombermanBomb
{
public:
	BombermanBomb() = default;

	BombermanBomb(BombermanGridPosition gridPosition, float fuseTime, int explosionRange)
		: m_gridPosition(gridPosition),
		m_fuseTimer(fuseTime),
		m_explosionRange(explosionRange)
	{
	}

	void update(float deltaTime)
	{
		if (m_exploded)
			return;

		m_fuseTimer -= deltaTime;
	}

	void triggerNow()
	{
		m_fuseTimer = 0.f;
	}

	bool shouldExplode() const
	{
		return !m_exploded && m_fuseTimer <= 0.f;
	}

	void markExploded()
	{
		m_exploded = true;
	}

	bool hasExploded() const
	{
		return m_exploded;
	}

	BombermanGridPosition getGridPosition() const
	{
		return m_gridPosition;
	}

	int getExplosionRange() const
	{
		return m_explosionRange;
	}

	bool canPlayerPassThrough() const
	{
		return m_playerCanPassThrough;
	}

	void setPlayerCanPassThrough(bool canPassThrough)
	{
		m_playerCanPassThrough = canPassThrough;
	}

private:
	BombermanGridPosition m_gridPosition{ 0, 0 };
	float m_fuseTimer = 2.f;
	int m_explosionRange = 2;
	bool m_exploded = false;

	// This starts true when the player places a bomb.
	// It becomes false once the player has fully walked off that bomb tile.
	// This prevents the player from getting stuck inside their own bomb.
	bool m_playerCanPassThrough = true;
};