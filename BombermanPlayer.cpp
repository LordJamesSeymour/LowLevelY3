#include "BombermanPlayer.h"

#include <algorithm>
#include <cmath>
#include <filesystem>

bool BombermanPlayer::load(const std::string& playerDirectory)
{
	namespace fs = std::filesystem;

	m_lastError.clear();

	const fs::path base(playerDirectory);

	const std::string downPath = (base / "player_down.png").string();
	const std::string upPath = (base / "player_up.png").string();
	const std::string leftPath = (base / "player_left.png").string();
	const std::string rightPath = (base / "player_right.png").string();

	if (!m_downTexture.loadFromFile(downPath))
	{
		m_lastError = "Failed to load Bomberman player texture: " + downPath;
		return false;
	}

	if (!loadTextureOrFallback(m_upTexture, upPath, downPath))
		return false;

	if (!loadTextureOrFallback(m_leftTexture, leftPath, downPath))
		return false;

	if (!loadTextureOrFallback(m_rightTexture, rightPath, downPath))
		return false;

	m_sprite.emplace(m_downTexture);

	const sf::FloatRect localBounds = m_sprite->getLocalBounds();

	if (localBounds.size.x <= 0.f || localBounds.size.y <= 0.f)
	{
		m_lastError = "Bomberman player texture has invalid size.";
		return false;
	}

	m_sprite->setScale({
		static_cast<float>(BombermanLevel::TileSize) / localBounds.size.x,
		static_cast<float>(BombermanLevel::TileSize) / localBounds.size.y
		});

	return true;
}

bool BombermanPlayer::loadTextureOrFallback(sf::Texture& texture,
	const std::string& preferredPath,
	const std::string& fallbackPath)
{
	if (texture.loadFromFile(preferredPath))
		return true;

	if (!texture.loadFromFile(fallbackPath))
	{
		m_lastError = "Failed to load Bomberman player texture: " + preferredPath;
		return false;
	}

	return true;
}

void BombermanPlayer::reset(BombermanGridPosition spawnPosition, const BombermanLevel& level)
{
	m_alive = true;
	m_facingDirection = BombermanDirection::Down;
	applyTextureForFacingDirection();

	m_position = level.gridToWorldTopLeft(spawnPosition);

	if (m_sprite)
	{
		m_sprite->setPosition(m_position);
	}
}

void BombermanPlayer::update(float deltaTime,
	const BombermanLevel& level,
	const std::function<bool(int col, int row)>& isTileBlocked)
{
	(void)level;

	if (!m_sprite || !m_alive)
		return;

	sf::Vector2f input{ 0.f, 0.f };

	if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W) ||
		sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up))
	{
		input.y -= 1.f;
	}
	else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S) ||
		sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Down))
	{
		input.y += 1.f;
	}
	else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A) ||
		sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left))
	{
		input.x -= 1.f;
	}
	else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D) ||
		sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right))
	{
		input.x += 1.f;
	}

	if (input.y < 0.f)
		m_facingDirection = BombermanDirection::Up;
	else if (input.y > 0.f)
		m_facingDirection = BombermanDirection::Down;
	else if (input.x < 0.f)
		m_facingDirection = BombermanDirection::Left;
	else if (input.x > 0.f)
		m_facingDirection = BombermanDirection::Right;

	applyTextureForFacingDirection();

	const sf::Vector2f movement = input * m_moveSpeed * deltaTime;

	// No corner-assist here. The player no longer gets pushed sideways when bumping walls.
	sf::Vector2f nextPosition = m_position + movement;

	if (canFitAt(nextPosition, isTileBlocked))
	{
		m_position = nextPosition;
	}

	m_sprite->setPosition(m_position);
}

bool BombermanPlayer::canFitAt(sf::Vector2f topLeftPosition,
	const std::function<bool(int col, int row)>& isTileBlocked) const
{
	const float left = topLeftPosition.x + m_collisionInset;
	const float top = topLeftPosition.y + m_collisionInset;
	const float right = topLeftPosition.x + static_cast<float>(BombermanLevel::TileSize) - m_collisionInset;
	const float bottom = topLeftPosition.y + static_cast<float>(BombermanLevel::TileSize) - m_collisionInset;

	const int leftCol = static_cast<int>(std::floor(left / static_cast<float>(BombermanLevel::TileSize)));
	const int rightCol = static_cast<int>(std::floor((right - 0.1f) / static_cast<float>(BombermanLevel::TileSize)));
	const int topRow = static_cast<int>(std::floor(top / static_cast<float>(BombermanLevel::TileSize)));
	const int bottomRow = static_cast<int>(std::floor((bottom - 0.1f) / static_cast<float>(BombermanLevel::TileSize)));

	for (int row = topRow; row <= bottomRow; ++row)
	{
		for (int col = leftCol; col <= rightCol; ++col)
		{
			if (isTileBlocked(col, row))
				return false;
		}
	}

	return true;
}

void BombermanPlayer::applyTextureForFacingDirection()
{
	if (!m_sprite)
		return;

	switch (m_facingDirection)
	{
	case BombermanDirection::Down:
		m_sprite->setTexture(m_downTexture, true);
		break;

	case BombermanDirection::Up:
		m_sprite->setTexture(m_upTexture, true);
		break;

	case BombermanDirection::Left:
		m_sprite->setTexture(m_leftTexture, true);
		break;

	case BombermanDirection::Right:
		m_sprite->setTexture(m_rightTexture, true);
		break;
	}

	const sf::FloatRect localBounds = m_sprite->getLocalBounds();

	if (localBounds.size.x > 0.f && localBounds.size.y > 0.f)
	{
		m_sprite->setScale({
			static_cast<float>(BombermanLevel::TileSize) / localBounds.size.x,
			static_cast<float>(BombermanLevel::TileSize) / localBounds.size.y
			});
	}
}

void BombermanPlayer::draw(sf::RenderTarget& target) const
{
	if (m_sprite && m_alive)
	{
		target.draw(*m_sprite);
	}
}

void BombermanPlayer::kill()
{
	m_alive = false;
}

bool BombermanPlayer::isAlive() const
{
	return m_alive;
}

BombermanGridPosition BombermanPlayer::getGridPosition(const BombermanLevel& level) const
{
	const sf::Vector2f center{
		m_position.x + static_cast<float>(BombermanLevel::TileSize) * 0.5f,
		m_position.y + static_cast<float>(BombermanLevel::TileSize) * 0.5f
	};

	return level.worldToGrid(center);
}

BombermanDirection BombermanPlayer::getFacingDirection() const
{
	return m_facingDirection;
}

sf::FloatRect BombermanPlayer::getBounds() const
{
	return getCollisionBounds();
}

sf::FloatRect BombermanPlayer::getCollisionBounds() const
{
	const float size = static_cast<float>(BombermanLevel::TileSize);

	return sf::FloatRect(
		{
			m_position.x + m_collisionInset,
			m_position.y + m_collisionInset
		},
		{
			size - m_collisionInset * 2.f,
			size - m_collisionInset * 2.f
		}
	);
}

const std::string& BombermanPlayer::getLastError() const
{
	return m_lastError;
}