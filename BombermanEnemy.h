#pragma once

#include "BombermanLevel.h"
#include "BombermanTypes.h"

#include <SFML/Graphics.hpp>
#include <functional>
#include <optional>
#include <random>
#include <string>
#include <vector>

class BombermanEnemy
{
public:
	bool load(const std::string& enemyAssetPath,
		BombermanGridPosition spawnPosition,
		const BombermanLevel& level);

	void update(float deltaTime,
		const std::function<bool(int col, int row)>& isTileBlocked);

	void draw(sf::RenderTarget& target) const;

	bool isAlive() const;
	void kill();

	BombermanGridPosition getGridPosition(const BombermanLevel& level) const;

	sf::FloatRect getBounds() const;

	const std::string& getLastError() const;

private:
	struct AnimationSet
	{
		std::vector<sf::Texture> frames;
		std::size_t currentFrame = 0;
		float timer = 0.f;
	};

private:
	bool loadAnimationFolder(AnimationSet& animationSet,
		const std::string& directoryPath,
		const std::string& readableName);

	void updateAnimation(float deltaTime);
	void applyCurrentAnimationFrame();

	const sf::Texture* getCurrentTexture() const;
	AnimationSet& getActiveAnimationSet();
	const AnimationSet& getActiveAnimationSet() const;

	void chooseNewDirection(const std::function<bool(int col, int row)>& isTileBlocked);
	bool tryStartMove(BombermanDirection direction,
		const std::function<bool(int col, int row)>& isTileBlocked);

	BombermanGridPosition getNeighbourPosition(BombermanDirection direction) const;

	sf::Vector2f directionToVector(BombermanDirection direction) const;
	BombermanDirection getOppositeDirection(BombermanDirection direction) const;

	void snapSpriteToPosition();

private:
	AnimationSet m_frontAnimation;
	AnimationSet m_backAnimation;
	AnimationSet m_sideAnimation;

	std::optional<sf::Sprite> m_sprite;

	BombermanGridPosition m_gridPosition{ 0, 0 };
	BombermanGridPosition m_targetGridPosition{ 0, 0 };

	sf::Vector2f m_position{ 0.f, 0.f };
	sf::Vector2f m_targetPosition{ 0.f, 0.f };

	BombermanDirection m_facingDirection = BombermanDirection::Down;
	BombermanDirection m_lastMoveDirection = BombermanDirection::Down;

	bool m_alive = true;
	bool m_isMoving = false;

	float m_moveSpeed = 72.f;
	float m_frameDuration = 0.14f;

	std::mt19937 m_rng{ std::random_device{}() };

	std::string m_lastError;
};