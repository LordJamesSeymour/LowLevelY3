#pragma once

#include "BombermanLevel.h"
#include "BombermanTypes.h"

#include <SFML/Graphics.hpp>
#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <vector>

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
	struct AnimationSet
	{
		std::vector<sf::Texture> frames;
		std::vector<std::size_t> movementSequence;

		std::size_t idleFrameIndex = 0;
		std::size_t sequenceIndex = 0;

		float timer = 0.f;
	};

private:
	bool loadAnimationFolder(AnimationSet& animationSet,
		const std::string& directoryPath,
		const std::string& readableName);

	void buildMovementSequence(AnimationSet& animationSet,
		const std::vector<int>& trailingNumbers);

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

	void updateAnimation(float deltaTime);
	void applyCurrentAnimationFrame();

	AnimationSet& getActiveAnimationSet();
	const AnimationSet& getActiveAnimationSet() const;

	const sf::Texture* getCurrentAnimationTexture() const;

	void resetActiveAnimationToIdle();

private:
	AnimationSet m_frontAnimation;
	AnimationSet m_backAnimation;
	AnimationSet m_leftAnimation;
	AnimationSet m_rightAnimation;

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

	float m_animationFrameDuration = 0.11f;
	bool m_movementKeyHeld = false;
	bool m_wasMovementKeyHeld = false;

	BombermanDirection m_facingDirection = BombermanDirection::Down;
	BombermanDirection m_previousAnimationDirection = BombermanDirection::Down;

	bool m_upHeldLastFrame = false;
	bool m_downHeldLastFrame = false;
	bool m_leftHeldLastFrame = false;
	bool m_rightHeldLastFrame = false;

	std::string m_lastError;
};