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

	BombermanGridPosition getGridPosition(const BombermanLevel& level) const;
	BombermanDirection getFacingDirection() const;

	sf::FloatRect getBounds() const;
	sf::FloatRect getCollisionBounds() const;

	const std::string& getLastError() const;

private:
	bool loadTextureOrFallback(sf::Texture& texture,
		const std::string& preferredPath,
		const std::string& fallbackPath);

	bool canFitAt(sf::Vector2f topLeftPosition,
		const std::function<bool(int col, int row)>& isTileBlocked) const;

	void applyTextureForFacingDirection();

private:
	sf::Texture m_downTexture;
	sf::Texture m_upTexture;
	sf::Texture m_leftTexture;
	sf::Texture m_rightTexture;

	std::optional<sf::Sprite> m_sprite;

	sf::Vector2f m_position{ 0.f, 0.f };

	float m_moveSpeed = 150.f;

	// Uniform collision body. This does NOT change based on sprite direction.
	float m_collisionInset = 13.f;

	bool m_alive = true;

	BombermanDirection m_facingDirection = BombermanDirection::Down;

	std::string m_lastError;
};