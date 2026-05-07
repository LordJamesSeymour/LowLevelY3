#pragma once

#include "BombermanLevel.h"
#include "BombermanTypes.h"

#include <SFML/Graphics.hpp>
#include <functional>
#include <optional>
#include <random>
#include <string>

class BombermanEnemy
{
public:
	BombermanEnemy() = default;
	~BombermanEnemy() = default;

	BombermanEnemy(const BombermanEnemy&) = delete;
	BombermanEnemy& operator=(const BombermanEnemy&) = delete;

	BombermanEnemy(BombermanEnemy&& other) noexcept;
	BombermanEnemy& operator=(BombermanEnemy&& other) noexcept;

	bool load(const std::string& texturePath, BombermanGridPosition spawnPosition, const BombermanLevel& level);

	void update(float deltaTime,
		const std::function<bool(int col, int row)>& isTileBlocked);

	void draw(sf::RenderTarget& target) const;

	void kill();
	bool isAlive() const;

	sf::FloatRect getBounds() const;
	BombermanGridPosition getGridPosition(const BombermanLevel& level) const;

	const std::string& getLastError() const;

private:
	void chooseRandomDirection();
	bool canFitAt(sf::Vector2f topLeftPosition,
		const std::function<bool(int col, int row)>& isTileBlocked) const;

	void rebindSpriteToOwnTexture();

private:
	sf::Texture m_texture;
	std::optional<sf::Sprite> m_sprite;

	sf::Vector2f m_position{ 0.f, 0.f };
	sf::Vector2f m_moveDirection{ 0.f, 0.f };

	float m_moveSpeed = 82.f;
	float m_directionTimer = 0.f;
	float m_directionChangeInterval = 0.8f;

	bool m_alive = true;

	std::mt19937 m_rng{ std::random_device{}() };

	std::string m_lastError;
};