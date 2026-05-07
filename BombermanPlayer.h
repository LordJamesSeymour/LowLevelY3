#pragma once

#include "BombermanLevel.h"
#include "BombermanTypes.h"

#include <SFML/Graphics.hpp>
#include <functional>
#include <optional>
#include <string>

class BombermanPlayer
{
public:
	bool load(const std::string& playerDirectory);

	void reset(BombermanGridPosition spawnPosition, const BombermanLevel& level);

	void update(float deltaTime,
		const BombermanLevel& level,
		const std::function<bool(int col, int row)>& isTileBlocked);

	void draw(sf::RenderTarget& target) const;

	void kill();

	bool isAlive() const;

	void beginInvincibility(float duration);
	bool isInvincible() const;

	void setMoveSpeed(float moveSpeed);
	float getMoveSpeed() const;

	BombermanGridPosition getGridPosition(const BombermanLevel& level) const;
	BombermanDirection getFacingDirection() const;

	sf::FloatRect getBounds() const;
	sf::FloatRect getCollisionBounds() const;

	const std::string& getLastError() const;

private:
	bool loadTextureOrFallback(sf::Texture& texture,
		const std::string& preferredPath,
		const std::string& fallbackPath);

	void refreshMovementInput();

	bool canFitAt(sf::Vector2f topLeftPosition,
		const std::function<bool(int col, int row)>& isTileBlocked) const;

	bool tryMoveWithEdgeCorrection(sf::Vector2f movement,
		const std::function<bool(int col, int row)>& isTileBlocked);

	bool tryForwardMoveWithPerpendicularOffset(sf::Vector2f movement,
		sf::Vector2f perpendicularOffset,
		const std::function<bool(int col, int row)>& isTileBlocked);

	sf::Vector2f getCollisionCenterAt(sf::Vector2f topLeftPosition) const;
	sf::Vector2f getCollisionCenter() const;

	float getNearestLaneCenter(float positionOnAxis) const;

	void applyTextureForFacingDirection();

private:
	sf::Texture m_downTexture;
	sf::Texture m_upTexture;
	sf::Texture m_leftTexture;
	sf::Texture m_rightTexture;

	std::optional<sf::Sprite> m_sprite;

	sf::Vector2f m_position{ 0.f, 0.f };
	sf::Vector2f m_currentMoveInput{ 0.f, 0.f };

	float m_moveSpeed = 150.f;

	float m_collisionRadius = 13.0f;

	float m_edgeCorrectionMaxDistance = 23.0f;
	float m_edgeCorrectionStep = 1.0f;
	float m_edgeCorrectionDeadZone = 2.5f;

	bool m_alive = true;

	float m_invincibilityTimer = 0.f;
	float m_flashRate = 18.f;

	BombermanDirection m_facingDirection = BombermanDirection::Down;

	bool m_upHeldLastFrame = false;
	bool m_downHeldLastFrame = false;
	bool m_leftHeldLastFrame = false;
	bool m_rightHeldLastFrame = false;

	std::string m_lastError;
};