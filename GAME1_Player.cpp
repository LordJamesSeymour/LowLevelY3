#include "GAME1_Player.h"
#include "GAME1_Level.h"

#include <algorithm>
#include <cmath>

namespace
{
	// These helper functions convert the player's world-space rectangle
	// into tile indices. That keeps the collision code cleaner.
	int GetLeftTile(const sf::FloatRect& bounds)
	{
		return static_cast<int>(std::floor(bounds.position.x / GAME1_Level::TileSize));
	}

	int GetRightTile(const sf::FloatRect& bounds)
	{
		return static_cast<int>(std::floor((bounds.position.x + bounds.size.x - 0.1f) / GAME1_Level::TileSize));
	}

	int GetTopTile(const sf::FloatRect& bounds)
	{
		return static_cast<int>(std::floor(bounds.position.y / GAME1_Level::TileSize));
	}

	int GetBottomTile(const sf::FloatRect& bounds)
	{
		return static_cast<int>(std::floor((bounds.position.y + bounds.size.y - 0.1f) / GAME1_Level::TileSize));
	}

	int GetSupportRow(const sf::FloatRect& bounds)
	{
		return static_cast<int>(std::floor((bounds.position.y + bounds.size.y + 0.1f) / GAME1_Level::TileSize));
	}
}

bool GAME1_Player::load(const std::string& texturePath, sf::Vector2f startPosition)
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

	// Scale the player sprite to a fixed 48x48 footprint.
	m_sprite->setScale({
		48.f / localBounds.size.x,
		48.f / localBounds.size.y
		});

	m_sprite->setPosition(startPosition);

	return true;
}

void GAME1_Player::update(float deltaTime, GAME1_Level& level, unsigned int windowWidth)
{
	if (!m_sprite.has_value())
		return;

	// Read input first so the movement state is fresh for this frame.
	handleInput(deltaTime);

	// Apply gravity every update.
	m_velocity.y += m_gravity * deltaTime;

	// Horizontal and vertical collision are separated on purpose.
	// This is a very common platformer technique because it simplifies resolution.
	moveHorizontal(deltaTime, level, windowWidth);
	moveVertical(deltaTime, level);
	updateBreakBlockTimer(level, deltaTime);
}

void GAME1_Player::handleInput(float deltaTime)
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

	// Coyote time lets the player still jump for a tiny moment after leaving ground.
	if (m_onGround)
		m_coyoteTimer = m_coyoteTime;
	else
		m_coyoteTimer = std::max(0.f, m_coyoteTimer - deltaTime);

	// Jump buffer stores a jump press briefly so jumps feel more forgiving.
	m_jumpBufferTimer = std::max(0.f, m_jumpBufferTimer - deltaTime);

	const bool jumpHeld = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space);

	// Only refresh the jump buffer on the press edge, not while held continuously.
	if (jumpHeld && !m_jumpHeldLastFrame)
	{
		m_jumpBufferTimer = m_jumpBufferTime;
	}

	// If both forgiveness systems overlap, perform the jump.
	if (m_jumpBufferTimer > 0.f && m_coyoteTimer > 0.f)
	{
		m_velocity.y = -m_jumpSpeed;
		m_onGround = false;
		m_coyoteTimer = 0.f;
		m_jumpBufferTimer = 0.f;
	}

	m_jumpHeldLastFrame = jumpHeld;
}

void GAME1_Player::moveHorizontal(float deltaTime, GAME1_Level& level, unsigned int windowWidth)
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
					static_cast<float>(rightTile * GAME1_Level::TileSize) - bounds.size.x,
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
					static_cast<float>((leftTile + 1) * GAME1_Level::TileSize),
					m_sprite->getPosition().y
					});

				m_velocity.x = 0.f;
				break;
			}
		}
	}

	bounds = m_sprite->getGlobalBounds();

	// Screen wrap gives the small arcade level a more classic feel.
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

void GAME1_Player::moveVertical(float deltaTime, GAME1_Level& level)
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
					static_cast<float>(bottomTile * GAME1_Level::TileSize) - bounds.size.y
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
					static_cast<float>((topTile + 1) * GAME1_Level::TileSize)
					});

				m_velocity.y = 0.f;

				// Breakable blocks also shatter when hit from underneath.
				if (level.isBreakTile(col, topTile))
				{
					level.breakTile(col, topTile);
				}

				break;
			}
		}
	}
}

void GAME1_Player::updateBreakBlockTimer(GAME1_Level& level, float deltaTime)
{
	if (!m_sprite.has_value())
		return;

	// The stand-to-break mechanic only matters when standing on the ground.
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

	// Search only the row directly under the player's feet.
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

	// If still standing on the same break block, count up the timer.
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
		// New break block under the player, so restart the countdown.
		m_standingBreakCol = foundBreakCol;
		m_standingBreakRow = foundBreakRow;
		m_breakStandTimer = 0.f;
	}
}

void GAME1_Player::draw(sf::RenderWindow& window) const
{
	if (m_sprite.has_value())
	{
		window.draw(*m_sprite);
	}
}

sf::FloatRect GAME1_Player::getBounds() const
{
	if (m_sprite.has_value())
		return m_sprite->getGlobalBounds();

	return sf::FloatRect();
}

const std::string& GAME1_Player::getLastError() const
{
	return m_lastError;
}