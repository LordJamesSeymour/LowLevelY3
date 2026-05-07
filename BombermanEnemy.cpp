#include "BombermanEnemy.h"

#include <cmath>
#include <utility>

BombermanEnemy::BombermanEnemy(BombermanEnemy&& other) noexcept
	:
	m_texture(std::move(other.m_texture)),
	m_sprite(std::move(other.m_sprite)),
	m_position(other.m_position),
	m_moveDirection(other.m_moveDirection),
	m_moveSpeed(other.m_moveSpeed),
	m_directionTimer(other.m_directionTimer),
	m_directionChangeInterval(other.m_directionChangeInterval),
	m_alive(other.m_alive),
	m_rng(std::move(other.m_rng)),
	m_lastError(std::move(other.m_lastError))
{
	rebindSpriteToOwnTexture();
}

BombermanEnemy& BombermanEnemy::operator=(BombermanEnemy&& other) noexcept
{
	if (this == &other)
		return *this;

	m_texture = std::move(other.m_texture);
	m_sprite = std::move(other.m_sprite);

	m_position = other.m_position;
	m_moveDirection = other.m_moveDirection;
	m_moveSpeed = other.m_moveSpeed;
	m_directionTimer = other.m_directionTimer;
	m_directionChangeInterval = other.m_directionChangeInterval;
	m_alive = other.m_alive;
	m_rng = std::move(other.m_rng);
	m_lastError = std::move(other.m_lastError);

	rebindSpriteToOwnTexture();

	return *this;
}

bool BombermanEnemy::load(const std::string& texturePath, BombermanGridPosition spawnPosition, const BombermanLevel& level)
{
	m_lastError.clear();

	if (!m_texture.loadFromFile(texturePath))
	{
		m_lastError = "Failed to load Bomberman enemy texture: " + texturePath;
		return false;
	}

	m_sprite.emplace(m_texture);

	const sf::FloatRect localBounds = m_sprite->getLocalBounds();

	if (localBounds.size.x <= 0.f || localBounds.size.y <= 0.f)
	{
		m_lastError = "Bomberman enemy texture has invalid size.";
		return false;
	}

	m_sprite->setScale({
		static_cast<float>(BombermanLevel::TileSize) / localBounds.size.x,
		static_cast<float>(BombermanLevel::TileSize) / localBounds.size.y
		});

	m_position = level.gridToWorldTopLeft(spawnPosition);
	m_sprite->setPosition(m_position);

	m_alive = true;
	chooseRandomDirection();

	return true;
}

void BombermanEnemy::update(float deltaTime,
	const std::function<bool(int col, int row)>& isTileBlocked)
{
	if (!m_sprite || !m_alive)
		return;

	m_directionTimer -= deltaTime;

	if (m_directionTimer <= 0.f)
	{
		chooseRandomDirection();
	}

	const sf::Vector2f nextPosition = m_position + m_moveDirection * m_moveSpeed * deltaTime;

	if (canFitAt(nextPosition, isTileBlocked))
	{
		m_position = nextPosition;
	}
	else
	{
		chooseRandomDirection();
	}

	m_sprite->setPosition(m_position);
}

void BombermanEnemy::chooseRandomDirection()
{
	std::uniform_int_distribution<int> directionDistribution(0, 3);
	const int value = directionDistribution(m_rng);

	switch (value)
	{
	case 0:
		m_moveDirection = { 1.f, 0.f };
		break;

	case 1:
		m_moveDirection = { -1.f, 0.f };
		break;

	case 2:
		m_moveDirection = { 0.f, 1.f };
		break;

	default:
		m_moveDirection = { 0.f, -1.f };
		break;
	}

	std::uniform_real_distribution<float> timerDistribution(0.45f, 1.25f);
	m_directionTimer = timerDistribution(m_rng);
}

bool BombermanEnemy::canFitAt(sf::Vector2f topLeftPosition,
	const std::function<bool(int col, int row)>& isTileBlocked) const
{
	const float padding = 8.f;
	const float left = topLeftPosition.x + padding;
	const float top = topLeftPosition.y + padding;
	const float right = topLeftPosition.x + static_cast<float>(BombermanLevel::TileSize) - padding;
	const float bottom = topLeftPosition.y + static_cast<float>(BombermanLevel::TileSize) - padding;

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

void BombermanEnemy::rebindSpriteToOwnTexture()
{
	if (!m_sprite)
		return;

	m_sprite->setTexture(m_texture, true);

	const sf::FloatRect localBounds = m_sprite->getLocalBounds();

	if (localBounds.size.x > 0.f && localBounds.size.y > 0.f)
	{
		m_sprite->setScale({
			static_cast<float>(BombermanLevel::TileSize) / localBounds.size.x,
			static_cast<float>(BombermanLevel::TileSize) / localBounds.size.y
			});
	}

	m_sprite->setPosition(m_position);
}

void BombermanEnemy::draw(sf::RenderTarget& target) const
{
	if (m_sprite && m_alive)
	{
		target.draw(*m_sprite);
	}
}

void BombermanEnemy::kill()
{
	m_alive = false;
}

bool BombermanEnemy::isAlive() const
{
	return m_alive;
}

sf::FloatRect BombermanEnemy::getBounds() const
{
	if (m_sprite)
		return m_sprite->getGlobalBounds();

	return sf::FloatRect();
}

BombermanGridPosition BombermanEnemy::getGridPosition(const BombermanLevel& level) const
{
	const sf::Vector2f center{
		m_position.x + static_cast<float>(BombermanLevel::TileSize) * 0.5f,
		m_position.y + static_cast<float>(BombermanLevel::TileSize) * 0.5f
	};

	return level.worldToGrid(center);
}

const std::string& BombermanEnemy::getLastError() const
{
	return m_lastError;
}