#pragma once

#include <SFML/Graphics.hpp>
#include <cstddef>
#include <random>
#include <string>
#include <vector>

class GAME1_Level;
class GAME1_Player;

enum class GAME1_EnemyType
{
	Basic
};

class GAME1_Enemy
{
public:
	bool load(const std::string& enemiesDirectory,
		GAME1_EnemyType type,
		sf::Vector2f spawnPosition);

	void update(float deltaTime, const GAME1_Level& level, const GAME1_Player& player);
	// Co-op overload: when player2 is non-null the enemy picks the more
	// interesting target (closest visible, otherwise closest active player).
	// Behaves identically to the single-player overload when player2 is null.
	void update(float deltaTime, const GAME1_Level& level,
		const GAME1_Player& player1, const GAME1_Player* player2);
	void draw(sf::RenderTarget& target) const;

	bool handlePlayerCollision(GAME1_Player& player);
	void killInstantly();

	bool isActive() const;
	bool isAlive() const;

	sf::FloatRect getBounds() const;
	const std::string& getLastError() const;

private:
	enum class BehaviourState
	{
		Idle,
		Patrol,
		Chase,
		Hit
	};

	enum class FacingDirection
	{
		Right,
		Left
	};

	struct AnimationSet
	{
		std::vector<sf::Texture> frames;
		float frameDuration = 0.07f;
	};

private:
	bool loadAnimationFramesFromDirectory(AnimationSet& animation,
		const std::string& directoryPath,
		const std::string& readableName);

	void chooseNextBehaviour();
	void chooseIdleBehaviour();
	void choosePatrolBehaviour();
	void chooseNewPatrolPoint();

	void updateIdle(float deltaTime);
	void updatePatrol(float deltaTime, const GAME1_Level& level);
	void updateChase(float deltaTime, const GAME1_Level& level, const GAME1_Player& player);
	void updateHit(float deltaTime);
	void updateAnimation(float deltaTime);

	bool canSeePlayer(const GAME1_Level& level, const GAME1_Player& player) const;
	bool hasClearSightToPlayer(const GAME1_Level& level, const GAME1_Player& player) const;
	bool canMoveToX(float newX, const GAME1_Level& level) const;
	bool hasGroundAhead(float newX, const GAME1_Level& level) const;
	void snapToGround(const GAME1_Level& level);
	void takeStompDamage();
	void beginDeath();

	const AnimationSet& getCurrentAnimationSet() const;
	const sf::Texture* getCurrentTexture() const;

	float randomFloat(float minValue, float maxValue);
	int randomInt(int minValue, int maxValue);

	static bool rectsIntersect(const sf::FloatRect& a, const sf::FloatRect& b);

private:
	GAME1_EnemyType m_type = GAME1_EnemyType::Basic;
	BehaviourState m_state = BehaviourState::Idle;
	FacingDirection m_facingDirection = FacingDirection::Right;

	AnimationSet m_idleAnimation;
	AnimationSet m_runAnimation;
	AnimationSet m_hitAnimation;

	sf::Vector2f m_spawnPosition{ 0.f, 0.f };
	sf::Vector2f m_position{ 0.f, 0.f };
	sf::Vector2f m_velocity{ 0.f, 0.f };

	float m_drawWidth = 48.f;
	float m_drawHeight = 48.f;
	float m_moveSpeed = 92.f;
	float m_chaseSpeedMultiplier = 1.5f;
	float m_visionRangeTiles = 4.f;
	float m_gravity = 1500.f;

	float m_idleTimer = 0.f;
	float m_turnTimer = 0.f;

	int m_patrolPointsRemaining = 0;
	float m_targetX = 0.f;
	float m_minPatrolDistanceTiles = 1.f;
	float m_maxPatrolDistanceTiles = 5.f;

	int m_health = 1;
	bool m_alive = true;
	bool m_active = true;

	float m_animationTimer = 0.f;
	std::size_t m_currentFrameIndex = 0;

	std::mt19937 m_rng;
	std::string m_lastError;
};
