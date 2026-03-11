#include "GAME2_Enemy.h"

#include <algorithm>
#include <cmath>
#include <filesystem>

GAME2_Enemy::GAME2_Enemy(float health, float damage, float speed)
	: m_health(health), m_damage(damage), m_speed(speed)
{
}

void GAME2_Enemy::initialiseFromSharedFrames(const std::vector<sf::Texture>& frames, sf::Vector2f startPosition)
{
	if (frames.empty())
		return;

	m_alive = true;
	m_position = startPosition;
	m_spawnOriginX = startPosition.x;
	m_currentFrameIndex = 0;
	m_animationTimer = 0.f;
	m_age = 0.f;

	m_sprite.emplace(frames[0]);
	m_sprite->setPosition(m_position);
}

void GAME2_Enemy::update(float deltaTime, const sf::FloatRect& playBounds)
{
	if (!m_sprite || !m_alive)
		return;

	m_age += deltaTime;
	m_animationTimer += deltaTime;

	const std::vector<sf::Texture>& frames = getSharedFrames();
	if (frames.size() > 1)
	{
		while (m_animationTimer >= m_animationFrameDuration)
		{
			m_animationTimer -= m_animationFrameDuration;
			m_currentFrameIndex = (m_currentFrameIndex + 1) % frames.size();
			m_sprite->setTexture(frames[m_currentFrameIndex], true);
		}
	}

	m_position.y += m_speed * deltaTime;

	// S-shaped horizontal wind, but constrained to the gameplay area.
	m_position.x = m_spawnOriginX + std::sin(m_age * m_sineFrequency) * m_sineAmplitude;

	const sf::FloatRect bounds = m_sprite->getGlobalBounds();

	m_position.x = std::clamp(
		m_position.x,
		playBounds.position.x,
		playBounds.position.x + playBounds.size.x - bounds.size.x);

	m_sprite->setPosition(m_position);
}

void GAME2_Enemy::draw(sf::RenderWindow& window) const
{
	if (m_sprite && m_alive)
		window.draw(*m_sprite);
}

bool GAME2_Enemy::isOffScreen(const sf::FloatRect& playBounds) const
{
	if (!m_sprite)
		return true;

	const sf::FloatRect bounds = m_sprite->getGlobalBounds();
	return bounds.position.y > playBounds.position.y + playBounds.size.y;
}

bool GAME2_Enemy::isAlive() const
{
	return m_alive;
}

void GAME2_Enemy::takeDamage(float amount)
{
	if (!m_alive)
		return;

	m_health -= amount;

	if (m_health <= 0.f)
	{
		m_health = 0.f;
		m_alive = false;
	}
}

void GAME2_Enemy::destroy()
{
	m_alive = false;
}

float GAME2_Enemy::getHealth() const
{
	return m_health;
}

float GAME2_Enemy::getDamage() const
{
	return m_damage;
}

float GAME2_Enemy::getSpeed() const
{
	return m_speed;
}

sf::FloatRect GAME2_Enemy::getBounds() const
{
	if (m_sprite)
		return m_sprite->getGlobalBounds();

	return sf::FloatRect();
}

sf::FloatRect GAME2_Enemy::getCollisionBounds() const
{
	return getBounds();
}

bool GAME2_BigEnemy::loadSharedAssets(const std::string& enemiesDirectory, std::string& outError)
{
	if (!s_sharedFrames.empty())
		return true;

	namespace fs = std::filesystem;

	const fs::path base(enemiesDirectory);
	const fs::path frame1 = base / "BigEnemy-1.png";
	const fs::path frame2 = base / "BigEnemy-2.png";

	sf::Texture texture1;
	if (!texture1.loadFromFile(frame1.string()))
	{
		outError = "Failed to load Game 2 enemy texture: " + frame1.string();
		return false;
	}

	sf::Texture texture2;
	if (!texture2.loadFromFile(frame2.string()))
	{
		outError = "Failed to load Game 2 enemy texture: " + frame2.string();
		return false;
	}

	s_sharedFrames.push_back(std::move(texture1));
	s_sharedFrames.push_back(std::move(texture2));

	return true;
}

GAME2_BigEnemy::GAME2_BigEnemy(sf::Vector2f startPosition)
	: GAME2_Enemy(2.f, 1.f, 170.f)
{
	initialiseFromSharedFrames(s_sharedFrames, startPosition);
}

const std::vector<sf::Texture>& GAME2_BigEnemy::getSharedFrames() const
{
	return s_sharedFrames;
}