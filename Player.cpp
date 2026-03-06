#include "Player.h"
#include "Level.h"

#include <algorithm>
#include <cmath>

namespace
{
	int GetLeftTile(const sf::FloatRect& bounds)
	{
		return static_cast<int>(std::floor(bounds.position.x / Level::TileSize));
	}

	int GetRightTile(const sf::FloatRect& bounds)
	{
		return static_cast<int>(std::floor((bounds.position.x + bounds.size.x - 0.1f) / Level::TileSize));
	}

	int GetTopTile(const sf::FloatRect& bounds)
	{
		return static_cast<int>(std::floor(bounds.position.y / Level::TileSize));
	}

	int GetBottomTile(const sf::FloatRect& bounds)
	{
		return static_cast<int>(std::floor((bounds.position.y + bounds.size.y - 0.1f) / Level::TileSize));
	}

	int GetSupportRow(const sf::FloatRect& bounds)
	{
		return static_cast<int>(std::floor((bounds.position.y + bounds.size.y + 0.1f) / Level::TileSize));
	}
}

bool Player::load(const std::string& texturePath, sf::Vector2f startPosition)
{
	m_lastError.clear();

	if (!m_texture.loadFromFile(texturePath))
	{
		m_lastError = "Failed to load player texture: " + texturePath;
		return false;
	}

	m_sprite.emplace(m_texture);

	const sf::FloatRect localBounds = m_sprite->getLocalBounds();
	if (localBounds.size.x <= 0.f || localBounds.size.y <= 0.f)
	{
		m_lastError = "Player texture has invalid size.";
		return false;
	}

	m_sprite->setScale({
		48.f / localBounds.size.x,
		48.f / localBounds.size.y
		});

	m_sprite->setPosition(startPosition);

	return true;
}

void Player::update(float deltaTime, Level& level, unsigned int windowWidth)
{
	if (!m_sprite.has_value())
		return;

	handleInput(deltaTime);

	m_velocity.y += m_gravity * deltaTime;

	moveHorizontal(deltaTime, level, windowWidth);
	moveVertical(deltaTime, level);
	updateBreakBlockTimer(level, deltaTime);
}

void Player::handleInput(float deltaTime)
{
	m_velocity.x = 0.f;

	if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A) ||
		sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left))
	{
		m_velocity.x -= m_moveSpeed;
	}

	if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D) ||
		sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right))
	{
		m_velocity.x += m_moveSpeed;
	}

	if (m_onGround)
		m_coyoteTimer = m_coyoteTime;
	else
		m_coyoteTimer = std::max(0.f, m_coyoteTimer - deltaTime);

	m_jumpBufferTimer = std::max(0.f, m_jumpBufferTimer - deltaTime);

	const bool jumpHeld = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space);

	if (jumpHeld && !m_jumpHeldLastFrame)
	{
		m_jumpBufferTimer = m_jumpBufferTime;
	}

	if (m_jumpBufferTimer > 0.f && m_coyoteTimer > 0.f)
	{
		m_velocity.y = -m_jumpSpeed;
		m_onGround = false;
		m_coyoteTimer = 0.f;
		m_jumpBufferTimer = 0.f;
	}

	m_jumpHeldLastFrame = jumpHeld;
}

void Player::moveHorizontal(float deltaTime, Level& level, unsigned int windowWidth)
{
	m_sprite->move({ m_velocity.x * deltaTime, 0.f });

	sf::FloatRect bounds = m_sprite->getGlobalBounds();

	const int topTile = GetTopTile(bounds);
	const int bottomTile = GetBottomTile(bounds);

	if (m_velocity.x > 0.f)
	{
		const int rightTile = GetRightTile(bounds);

		for (int row = topTile; row <= bottomTile; ++row)
		{
			if (level.isSolidTile(rightTile, row))
			{
				m_sprite->setPosition({
					static_cast<float>(rightTile * Level::TileSize) - bounds.size.x,
					m_sprite->getPosition().y
					});

				m_velocity.x = 0.f;
				break;
			}
		}
	}
	else if (m_velocity.x < 0.f)
	{
		const int leftTile = GetLeftTile(bounds);

		for (int row = topTile; row <= bottomTile; ++row)
		{
			if (level.isSolidTile(leftTile, row))
			{
				m_sprite->setPosition({
					static_cast<float>((leftTile + 1) * Level::TileSize),
					m_sprite->getPosition().y
					});

				m_velocity.x = 0.f;
				break;
			}
		}
	}

	bounds = m_sprite->getGlobalBounds();

	if (bounds.position.x < 0.f)
	{
		m_sprite->setPosition({
			static_cast<float>(windowWidth) - bounds.size.x,
			m_sprite->getPosition().y
			});
	}
	else if (bounds.position.x + bounds.size.x > static_cast<float>(windowWidth))
	{
		m_sprite->setPosition({
			0.f,
			m_sprite->getPosition().y
			});
	}
}

void Player::moveVertical(float deltaTime, Level& level)
{
	m_onGround = false;

	m_sprite->move({ 0.f, m_velocity.y * deltaTime });

	sf::FloatRect bounds = m_sprite->getGlobalBounds();

	const int leftTile = GetLeftTile(bounds);
	const int rightTile = GetRightTile(bounds);

	if (m_velocity.y > 0.f)
	{
		const int bottomTile = GetBottomTile(bounds);

		for (int col = leftTile; col <= rightTile; ++col)
		{
			if (level.isSolidTile(col, bottomTile))
			{
				m_sprite->setPosition({
					m_sprite->getPosition().x,
					static_cast<float>(bottomTile * Level::TileSize) - bounds.size.y
					});

				m_velocity.y = 0.f;
				m_onGround = true;
				break;
			}
		}
	}
	else if (m_velocity.y < 0.f)
	{
		const int topTile = GetTopTile(bounds);

		for (int col = leftTile; col <= rightTile; ++col)
		{
			if (level.isSolidTile(col, topTile))
			{
				m_sprite->setPosition({
					m_sprite->getPosition().x,
					static_cast<float>((topTile + 1) * Level::TileSize)
					});

				m_velocity.y = 0.f;

				if (level.isBreakTile(col, topTile))
				{
					level.breakTile(col, topTile);
				}

				break;
			}
		}
	}
}

void Player::updateBreakBlockTimer(Level& level, float deltaTime)
{
	if (!m_sprite.has_value())
		return;

	if (!m_onGround)
	{
		m_standingBreakCol = -1;
		m_standingBreakRow = -1;
		m_breakStandTimer = 0.f;
		return;
	}

	const sf::FloatRect bounds = m_sprite->getGlobalBounds();

	const int leftTile = GetLeftTile(bounds);
	const int rightTile = GetRightTile(bounds);
	const int supportRow = GetSupportRow(bounds);

	int foundBreakCol = -1;
	int foundBreakRow = -1;

	for (int col = leftTile; col <= rightTile; ++col)
	{
		if (level.isBreakTile(col, supportRow))
		{
			foundBreakCol = col;
			foundBreakRow = supportRow;
			break;
		}
	}

	if (foundBreakCol == -1)
	{
		m_standingBreakCol = -1;
		m_standingBreakRow = -1;
		m_breakStandTimer = 0.f;
		return;
	}

	if (foundBreakCol == m_standingBreakCol && foundBreakRow == m_standingBreakRow)
	{
		m_breakStandTimer += deltaTime;

		if (m_breakStandTimer >= 1.f)
		{
			level.breakTile(foundBreakCol, foundBreakRow);

			m_standingBreakCol = -1;
			m_standingBreakRow = -1;
			m_breakStandTimer = 0.f;
			m_onGround = false;
		}
	}
	else
	{
		m_standingBreakCol = foundBreakCol;
		m_standingBreakRow = foundBreakRow;
		m_breakStandTimer = 0.f;
	}
}

void Player::draw(sf::RenderWindow& window) const
{
	if (m_sprite.has_value())
	{
		window.draw(*m_sprite);
	}
}

sf::FloatRect Player::getBounds() const
{
	if (m_sprite.has_value())
		return m_sprite->getGlobalBounds();

	return sf::FloatRect();
}

const std::string& Player::getLastError() const
{
	return m_lastError;
}