#pragma once

#include "BombermanTypes.h"

#include <SFML/Graphics.hpp>
#include <functional>
#include <optional>
#include <random>
#include <string>
#include <vector>

class BombermanLevel;

class BombermanEnemy
{
public:
	using TileBlockedCallback = std::function<bool(int col, int row)>;

public:
	bool load(const std::string& enemyDirectory,
		BombermanEnemyType type,
		BombermanGridPosition spawnPosition,
		const BombermanLevel& level);

	void update(float deltaTime,
		BombermanGridPosition playerGridPosition,
		const TileBlockedCallback& isTileBlocked);

	void draw(sf::RenderTarget& target) const;

	bool isAlive() const;
	void kill();

	bool canPassThroughBreakableBlocks() const;
	bool canBeKilledByExplosion() const;

	bool shouldDrawAsBomb() const;
	bool hasPendingBomberExplosion() const;
	int getBomberExplosionRange() const;
	void consumePendingBomberExplosion();

	BombermanEnemyType getType() const;
	BombermanGridPosition getGridPosition(const BombermanLevel& level) const;
	sf::Vector2f getDrawPosition() const;
	sf::FloatRect getBounds() const;

	const std::string& getLastError() const;

private:
	enum class FacingDirection
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

private:
	bool loadDirectionalAnimations(const std::string& enemyDirectory, const std::string& readableName);
	bool loadCopterAnimations(const std::string& enemyDirectory);
	bool loadLampAnimation(const std::string& enemyDirectory);
	bool loadTreeAnimations(const std::string& enemyDirectory);
	bool loadBomberAnimations(const std::string& enemyDirectory);

	bool loadAnimationFramesFromDirectory(AnimationSet& animation,
		const std::string& directoryPath,
		const std::string& readableName);

	void chooseNextMove(BombermanGridPosition playerGridPosition,
		const TileBlockedCallback& isTileBlocked);

	void chooseCopterMove(const TileBlockedCallback& isTileBlocked);
	void chooseLampMove(BombermanGridPosition playerGridPosition,
		const TileBlockedCallback& isTileBlocked);
	void chooseStraightLineMove(const TileBlockedCallback& isTileBlocked);
	void chooseBomberMove(const TileBlockedCallback& isTileBlocked);

	bool tryStartMove(int colDelta,
		int rowDelta,
		const TileBlockedCallback& isTileBlocked);

	bool canMoveInDirection(int colDelta,
		int rowDelta,
		const TileBlockedCallback& isTileBlocked) const;

	void updateMovement(float deltaTime);
	void updateAnimation(float deltaTime);
	void updateBomberDetonation(float deltaTime);

	void startBomberDetonation();
	void finishBomberDetonation();

	void setRandomStraightLineDirection();
	void reverseFacingDirection();
	BombermanGridPosition getFacingDirectionDelta() const;

	const AnimationSet& getCurrentAnimation() const;
	std::size_t getCurrentFrameIndex() const;

	sf::Vector2f gridToWorldTopLeft(BombermanGridPosition gridPosition) const;
	sf::Vector2f gridToWorldCenter(BombermanGridPosition gridPosition) const;

private:
	BombermanEnemyType m_type = BombermanEnemyType::Copter;

	AnimationSet m_frontAnimation;
	AnimationSet m_backAnimation;
	AnimationSet m_sideAnimation;
	AnimationSet m_lampAnimation;

	BombermanGridPosition m_gridPosition{ 0, 0 };
	BombermanGridPosition m_targetGridPosition{ 0, 0 };

	sf::Vector2f m_position{ 0.f, 0.f };
	sf::Vector2f m_targetPosition{ 0.f, 0.f };

	bool m_isMoving = false;
	bool m_alive = true;

	FacingDirection m_facing = FacingDirection::Front;

	float m_moveSpeed = 95.f;

	float m_animationTimer = 0.f;
	float m_animationFrameDuration = 0.14f;
	std::size_t m_animationFrameIndex = 0;

	bool m_hasInitialStraightLineDirection = false;

	bool m_isBomberDetonating = false;
	bool m_bomberExplosionPending = false;
	float m_bomberCooldownTimer = 4.0f;
	float m_bomberCooldownMin = 4.0f;
	float m_bomberCooldownMax = 7.0f;
	float m_bomberDetonationTimer = 0.f;
	float m_bomberFuseTime = 3.5f;
	int m_bomberExplosionRange = 3;

	std::mt19937 m_rng{ std::random_device{}() };

	std::string m_lastError;
};