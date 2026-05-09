#pragma once

#include "BombermanTypes.h"

#include <SFML/Graphics.hpp>
#include <functional>
#include <optional>
#include <string>
#include <vector>

class BombermanLevel;

class BombermanPlayer
{
public:
	using TileBlockedCallback = std::function<bool(int col, int row)>;

public:
	bool load(const std::string& playerDirectory);

	void reset(BombermanGridPosition spawnPosition, const BombermanLevel& level);
	void update(float deltaTime,
		const BombermanLevel& level,
		const TileBlockedCallback& isTileBlocked);

	void draw(sf::RenderTarget& target) const;

	bool isAlive() const;
	void kill();

	void beginInvincibility(float duration);
	bool isInvincible() const;

	void setMoveSpeed(float moveSpeed);
	float getMoveSpeed() const;

	BombermanGridPosition getGridPosition(const BombermanLevel& level) const;

	sf::FloatRect getBounds() const;
	sf::FloatRect getCollisionBounds() const;

	const std::string& getLastError() const;

private:
	enum class Direction
	{
		Front,
		Back,
		Left,
		Right
	};

	struct AnimationSet
	{
		std::vector<sf::Texture> frames;
	};

	struct HeldInputState
	{
		bool up = false;
		bool down = false;
		bool left = false;
		bool right = false;
	};

private:
	bool loadAnimationFramesFromDirectory(AnimationSet& animation,
		const std::string& directoryPath,
		const std::string& readableName);

	HeldInputState readInputState() const;
	sf::Vector2f resolveMovementInput(const HeldInputState& inputState);
	bool isDirectionHeld(Direction direction, const HeldInputState& inputState) const;

	void updateAnimation(float deltaTime, bool isMoving);
	void setFacing(Direction direction);
	const AnimationSet& getCurrentAnimation() const;
	std::size_t getCurrentFrameIndex() const;

	void tryMove(sf::Vector2f movement, const TileBlockedCallback& isTileBlocked);
	bool tryMoveDirect(sf::Vector2f movement, const TileBlockedCallback& isTileBlocked);
	bool tryMoveWithEdgeCorrection(sf::Vector2f movement, const TileBlockedCallback& isTileBlocked);

	bool wouldCollideAt(sf::Vector2f position, const TileBlockedCallback& isTileBlocked) const;
	sf::FloatRect getCollisionBoundsAt(sf::Vector2f position) const;
	bool rectsIntersect(const sf::FloatRect& a, const sf::FloatRect& b) const;

	sf::Vector2f gridToWorldTopLeft(BombermanGridPosition gridPosition) const;

private:
	AnimationSet m_frontAnimation;
	AnimationSet m_backAnimation;
	AnimationSet m_leftAnimation;
	AnimationSet m_rightAnimation;

	sf::Vector2f m_position{ 0.f, 0.f };

	Direction m_facing = Direction::Front;

	float m_moveSpeed = 150.f;

	bool m_alive = true;

	float m_invincibilityTimer = 0.f;

	bool m_isMoving = false;
	bool m_wasMovingLastFrame = false;

	float m_animationTimer = 0.f;
	float m_animationFrameDuration = 0.11f;
	std::size_t m_animationSequenceIndex = 0;

	HeldInputState m_previousInputState;

	// Uniform collision box. This does not change with animation direction.
	float m_collisionSize = 30.f;

	// Small corner assist. It only applies if the corrected movement actually clears the block.
	float m_edgeCorrectionDistance = 10.f;

	std::string m_lastError;
};