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
		BombermanGridPosition targetGridPosition,
		const TileBlockedCallback& isTileBlocked);

	void draw(sf::RenderTarget& target) const;

	bool isAlive() const;
	void kill();
	void takeHit();

	int getHitPoints() const;

	bool canPassThroughBreakableBlocks() const;
	bool canBeKilledByExplosion() const;
	bool wantsToEatBombs() const;

	BombermanEnemyType getType() const;
	BombermanGridPosition getGridPosition(const BombermanLevel& level) const;
	BombermanGridPosition getFacingDirectionDelta() const;

	sf::Vector2f getDrawPosition() const;
	sf::FloatRect getBounds() const;

	bool shouldDrawAsBomb() const;
	bool isPreparingBombExplosion() const;
	int getBomberExplosionRange() const;
	bool consumePendingBomberExplosion();

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
	bool loadCopterAnimations(const std::string& enemyDirectory);
	bool loadTreeAnimations(const std::string& enemyDirectory);
	bool loadBomberAnimations(const std::string& enemyDirectory);
	bool loadChomperAnimations(const std::string& enemyDirectory);

	bool loadDirectionalAnimations(const std::string& enemyDirectory,
		const std::string& readableName);

	bool loadLampAnimation(const std::string& enemyDirectory);

	bool loadAnimationFramesFromDirectory(AnimationSet& animation,
		const std::string& directoryPath,
		const std::string& readableName);

	void updateBomberDetonation(float deltaTime);

	void chooseNextMove(BombermanGridPosition targetGridPosition,
		const TileBlockedCallback& isTileBlocked);

	void chooseCopterMove(const TileBlockedCallback& isTileBlocked);
	void chooseLampMove(BombermanGridPosition targetGridPosition,
		const TileBlockedCallback& isTileBlocked);
	void chooseStraightLineMove(const TileBlockedCallback& isTileBlocked);
	void chooseChomperMove(BombermanGridPosition targetGridPosition,
		const TileBlockedCallback& isTileBlocked);

	bool tryStartMove(int colDelta,
		int rowDelta,
		const TileBlockedCallback& isTileBlocked);

	void reverseDirection();

	void updateMovement(float deltaTime);
	void updateAnimation(float deltaTime);

	const AnimationSet& getCurrentAnimation() const;
	std::size_t getCurrentFrameIndex() const;
	bool shouldUsePingPongAnimation() const;

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

	int m_hitPoints = 1;

	float m_bomberPreparationTimer = 0.f;
	float m_bomberPreparationCooldown = 5.0f;
	float m_bomberExplosionTimer = 0.f;
	float m_bomberExplosionDelay = 3.4f;
	int m_bomberExplosionRange = 3;

	bool m_bomberPreparingExplosion = false;
	bool m_pendingBomberExplosion = false;

	std::mt19937 m_rng{ std::random_device{}() };

	std::string m_lastError;
};