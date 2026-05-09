#pragma once

#include "BombermanTypes.h"

#include <SFML/System/Vector2.hpp>
#include <algorithm>
#include <cmath>

class BombermanBomb
{
public:
	BombermanBomb() = default;

	BombermanBomb(BombermanGridPosition gridPosition, float fuseTime, int explosionRange)
		: m_gridPosition(gridPosition),
		m_drawPosition(gridToWorldTopLeft(gridPosition)),
		m_fuseTimer(fuseTime),
		m_explosionRange(explosionRange)
	{
	}

	void update(float deltaTime)
	{
		if (m_exploded)
			return;

		m_fuseTimer -= deltaTime;
		updateSlide(deltaTime);
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
		m_isSliding = false;
	}

	bool hasExploded() const
	{
		return m_exploded;
	}

	BombermanGridPosition getGridPosition() const
	{
		return m_gridPosition;
	}

	BombermanGridPosition getTargetGridPosition() const
	{
		return m_slideTargetGridPosition;
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

	bool isSliding() const
	{
		return m_isSliding;
	}

	sf::Vector2f getDrawPosition() const
	{
		return m_drawPosition;
	}

	void startSlide(BombermanGridPosition targetGridPosition, float slideSpeedPixelsPerSecond)
	{
		if (m_exploded)
			return;

		if (targetGridPosition == m_gridPosition)
			return;

		m_slideTargetGridPosition = targetGridPosition;
		m_slideTargetPosition = gridToWorldTopLeft(targetGridPosition);
		m_slideSpeed = std::max(1.f, slideSpeedPixelsPerSecond);
		m_isSliding = true;

		// Once the bomb is punched, the player should no longer be allowed
		// to overlap it as their own newly placed bomb.
		m_playerCanPassThrough = false;
	}

	bool occupiesGridPosition(BombermanGridPosition gridPosition) const
	{
		if (m_exploded)
			return false;

		if (m_gridPosition == gridPosition)
			return true;

		if (m_isSliding && m_slideTargetGridPosition == gridPosition)
			return true;

		return false;
	}

private:
	static constexpr float TileSize = 48.f;

private:
	static sf::Vector2f gridToWorldTopLeft(BombermanGridPosition gridPosition)
	{
		return {
			static_cast<float>(gridPosition.col) * TileSize,
			static_cast<float>(gridPosition.row) * TileSize
		};
	}

	void updateSlide(float deltaTime)
	{
		if (!m_isSliding)
			return;

		const sf::Vector2f toTarget = m_slideTargetPosition - m_drawPosition;
		const float distance = std::sqrt(toTarget.x * toTarget.x + toTarget.y * toTarget.y);

		if (distance <= 0.001f)
		{
			finishSlide();
			return;
		}

		const float step = m_slideSpeed * deltaTime;

		if (step >= distance)
		{
			finishSlide();
			return;
		}

		const sf::Vector2f direction = toTarget / distance;
		m_drawPosition += direction * step;
	}

	void finishSlide()
	{
		m_gridPosition = m_slideTargetGridPosition;
		m_drawPosition = m_slideTargetPosition;
		m_isSliding = false;
	}

private:
	BombermanGridPosition m_gridPosition{ 0, 0 };
	sf::Vector2f m_drawPosition{ 0.f, 0.f };

	float m_fuseTimer = 2.f;
	int m_explosionRange = 2;
	bool m_exploded = false;

	// This starts true when the player places a bomb.
	// It becomes false once the player has fully walked off that bomb tile,
	// or immediately when the bomb is punched.
	bool m_playerCanPassThrough = true;

	bool m_isSliding = false;
	BombermanGridPosition m_slideTargetGridPosition{ 0, 0 };
	sf::Vector2f m_slideTargetPosition{ 0.f, 0.f };
	float m_slideSpeed = 480.f;
};