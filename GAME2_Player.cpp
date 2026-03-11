#include "GAME2_Player.h"

#include "GAME2_Bullet.h"

#include <algorithm>
#include <cmath>
#include <filesystem>

bool GAME2_Player::load(const std::string& playerDirectory, const sf::FloatRect& playBounds)
{
	m_lastError.clear();

	if (!loadAnimationSet(playerDirectory))
		return false;

	m_sprite.emplace(m_forwardFrames[0]);

	const sf::FloatRect localBounds = m_sprite->getLocalBounds();
	if (localBounds.size.x <= 0.f || localBounds.size.y <= 0.f)
	{
		m_lastError = "Game 2 player texture has invalid size.";
		return false;
	}

	reset(playBounds);
	return true;
}

bool GAME2_Player::loadAnimationSet(const std::string& directory)
{
	namespace fs = std::filesystem;

	const fs::path base(directory);

	const fs::path forward1 = base / "PlayerForward-1.png";
	const fs::path forward2 = base / "PlayerForward-2.png";

	const fs::path rl1 = base / "PlayerRL-1.png";
	const fs::path rl2 = base / "PlayerRL-2.png";

	const fs::path left1 = base / "PlayerLeft-1.png";
	const fs::path left2 = base / "PlayerLeft-2.png";

	const fs::path rr1 = base / "PlayerRR-1.png";
	const fs::path rr2 = base / "PlayerRR-2.png";

	const fs::path right1 = base / "PlayerRight-1.png";
	const fs::path right2 = base / "PlayerRight-2.png";

	if (!m_forwardFrames[0].loadFromFile(forward1.string()))
	{
		m_lastError = "Failed to load Game 2 player texture: " + forward1.string();
		return false;
	}

	if (!m_forwardFrames[1].loadFromFile(forward2.string()))
	{
		m_lastError = "Failed to load Game 2 player texture: " + forward2.string();
		return false;
	}

	if (!m_transitionLeftFrames[0].loadFromFile(rl1.string()))
	{
		m_lastError = "Failed to load Game 2 player texture: " + rl1.string();
		return false;
	}

	if (!m_transitionLeftFrames[1].loadFromFile(rl2.string()))
	{
		m_lastError = "Failed to load Game 2 player texture: " + rl2.string();
		return false;
	}

	if (!m_leftFrames[0].loadFromFile(left1.string()))
	{
		m_lastError = "Failed to load Game 2 player texture: " + left1.string();
		return false;
	}

	if (!m_leftFrames[1].loadFromFile(left2.string()))
	{
		m_lastError = "Failed to load Game 2 player texture: " + left2.string();
		return false;
	}

	if (!m_transitionRightFrames[0].loadFromFile(rr1.string()))
	{
		m_lastError = "Failed to load Game 2 player texture: " + rr1.string();
		return false;
	}

	if (!m_transitionRightFrames[1].loadFromFile(rr2.string()))
	{
		m_lastError = "Failed to load Game 2 player texture: " + rr2.string();
		return false;
	}

	if (!m_rightFrames[0].loadFromFile(right1.string()))
	{
		m_lastError = "Failed to load Game 2 player texture: " + right1.string();
		return false;
	}

	if (!m_rightFrames[1].loadFromFile(right2.string()))
	{
		m_lastError = "Failed to load Game 2 player texture: " + right2.string();
		return false;
	}

	return true;
}

void GAME2_Player::reset(const sf::FloatRect& playBounds)
{
	if (!m_sprite)
		return;

	m_animationState = AnimationState::ForwardLoop;
	m_frameIndex = 0;
	m_frameTimer = 0.f;

	m_lives = 3;
	m_shotCooldown = 0.f;
	m_invulnerabilityTimer = 0.f;

	applyCurrentTexture();

	const sf::FloatRect bounds = m_sprite->getGlobalBounds();

	m_position.x = playBounds.position.x + (playBounds.size.x - bounds.size.x) * 0.5f;
	m_position.y = playBounds.position.y + playBounds.size.y - bounds.size.y - 30.f;

	m_sprite->setPosition(m_position);
}

void GAME2_Player::update(float deltaTime, const sf::FloatRect& playBounds)
{
	if (!m_sprite)
		return;

	m_shotCooldown = std::max(0.f, m_shotCooldown - deltaTime);
	m_invulnerabilityTimer = std::max(0.f, m_invulnerabilityTimer - deltaTime);

	sf::Vector2f moveInput{ 0.f, 0.f };

	if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W))
		moveInput.y -= 1.f;

	if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S))
		moveInput.y += 1.f;

	if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A))
		moveInput.x -= 1.f;

	if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D))
		moveInput.x += 1.f;

	const float length = std::sqrt(moveInput.x * moveInput.x + moveInput.y * moveInput.y);
	if (length > 0.f)
	{
		moveInput.x /= length;
		moveInput.y /= length;
	}

	m_position += moveInput * m_moveSpeed * deltaTime;

	const sf::FloatRect bounds = m_sprite->getGlobalBounds();

	// Constrain the player to the visible background area only.
	m_position.x = std::clamp(
		m_position.x,
		playBounds.position.x,
		playBounds.position.x + playBounds.size.x - bounds.size.x);

	m_position.y = std::clamp(
		m_position.y,
		playBounds.position.y,
		playBounds.position.y + playBounds.size.y - bounds.size.y);

	HorizontalIntent desiredIntent = HorizontalIntent::Neutral;

	if (moveInput.x < 0.f)
		desiredIntent = HorizontalIntent::Left;
	else if (moveInput.x > 0.f)
		desiredIntent = HorizontalIntent::Right;

	if (desiredIntent == HorizontalIntent::Left)
	{
		if (m_animationState != AnimationState::ToLeft &&
			m_animationState != AnimationState::LeftLoop)
		{
			setAnimationState(AnimationState::ToLeft);
		}
	}
	else if (desiredIntent == HorizontalIntent::Right)
	{
		if (m_animationState != AnimationState::ToRight &&
			m_animationState != AnimationState::RightLoop)
		{
			setAnimationState(AnimationState::ToRight);
		}
	}
	else
	{
		if (m_animationState == AnimationState::ToLeft ||
			m_animationState == AnimationState::LeftLoop)
		{
			setAnimationState(AnimationState::FromLeft);
		}
		else if (m_animationState == AnimationState::ToRight ||
			m_animationState == AnimationState::RightLoop)
		{
			setAnimationState(AnimationState::FromRight);
		}
	}

	m_frameTimer += deltaTime;

	while (m_frameTimer >= m_frameDuration)
	{
		m_frameTimer -= m_frameDuration;
		advanceAnimationState();
	}

	m_sprite->setPosition(m_position);
}

void GAME2_Player::updateShooting(float deltaTime, std::vector<GAME2_Bullet>& bullets)
{
	(void)deltaTime;

	if (!m_sprite || isGameOver())
		return;

	if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space) && m_shotCooldown <= 0.f)
	{
		const sf::FloatRect bounds = m_sprite->getGlobalBounds();

		const sf::Vector2f bulletSpawnPosition{
			bounds.position.x + bounds.size.x * 0.5f,
			bounds.position.y + 6.f
		};

		bullets.emplace_back(bulletSpawnPosition);
		m_shotCooldown = m_shotInterval;
	}
}

void GAME2_Player::setAnimationState(AnimationState newState)
{
	if (m_animationState == newState)
		return;

	m_animationState = newState;
	m_frameIndex = 0;
	m_frameTimer = 0.f;
	applyCurrentTexture();
}

void GAME2_Player::advanceAnimationState()
{
	switch (m_animationState)
	{
	case AnimationState::ForwardLoop:
		m_frameIndex = (m_frameIndex + 1) % 2;
		break;

	case AnimationState::ToLeft:
		if (m_frameIndex == 0)
			m_frameIndex = 1;
		else
		{
			m_animationState = AnimationState::LeftLoop;
			m_frameIndex = 0;
		}
		break;

	case AnimationState::LeftLoop:
		m_frameIndex = (m_frameIndex + 1) % 2;
		break;

	case AnimationState::FromLeft:
		if (m_frameIndex == 0)
			m_frameIndex = 1;
		else
		{
			m_animationState = AnimationState::ForwardLoop;
			m_frameIndex = 0;
		}
		break;

	case AnimationState::ToRight:
		if (m_frameIndex == 0)
			m_frameIndex = 1;
		else
		{
			m_animationState = AnimationState::RightLoop;
			m_frameIndex = 0;
		}
		break;

	case AnimationState::RightLoop:
		m_frameIndex = (m_frameIndex + 1) % 2;
		break;

	case AnimationState::FromRight:
		if (m_frameIndex == 0)
			m_frameIndex = 1;
		else
		{
			m_animationState = AnimationState::ForwardLoop;
			m_frameIndex = 0;
		}
		break;
	}

	applyCurrentTexture();
}

void GAME2_Player::applyCurrentTexture()
{
	if (!m_sprite)
		return;

	m_sprite->setTexture(getTextureForCurrentStateFrame(), true);
}

const sf::Texture& GAME2_Player::getTextureForCurrentStateFrame() const
{
	switch (m_animationState)
	{
	case AnimationState::ForwardLoop:
		return m_forwardFrames[m_frameIndex];

	case AnimationState::ToLeft:
	case AnimationState::FromLeft:
		return m_transitionLeftFrames[m_frameIndex];

	case AnimationState::LeftLoop:
		return m_leftFrames[m_frameIndex];

	case AnimationState::ToRight:
	case AnimationState::FromRight:
		return m_transitionRightFrames[m_frameIndex];

	case AnimationState::RightLoop:
		return m_rightFrames[m_frameIndex];
	}

	return m_forwardFrames[0];
}

void GAME2_Player::draw(sf::RenderWindow& window) const
{
	if (!m_sprite)
		return;

	// Blink while temporarily invulnerable so the player can read the damage grace period.
	if (m_invulnerabilityTimer > 0.f)
	{
		const int blinkPhase = static_cast<int>(m_invulnerabilityTimer * 18.f);
		if (blinkPhase % 2 != 0)
			return;
	}

	window.draw(*m_sprite);
}

sf::FloatRect GAME2_Player::getBounds() const
{
	if (m_sprite)
		return m_sprite->getGlobalBounds();

	return sf::FloatRect();
}

sf::FloatRect GAME2_Player::getCollisionBounds() const
{
	return getBounds();
}

void GAME2_Player::takeHit()
{
	if (m_invulnerabilityTimer > 0.f || m_lives <= 0)
		return;

	--m_lives;
	m_invulnerabilityTimer = m_invulnerabilityDuration;
}

int GAME2_Player::getLives() const
{
	return m_lives;
}

bool GAME2_Player::isGameOver() const
{
	return m_lives <= 0;
}

const std::string& GAME2_Player::getLastError() const
{
	return m_lastError;
}