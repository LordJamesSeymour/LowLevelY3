#pragma once

#include <SFML/Graphics.hpp>
#include <optional>
#include <string>
#include <vector>

// Base enemy class for Game 2.
// This parent declares the common stats and common movement/animation data
// so child enemy types can be added quickly by inheritance.
class GAME2_Enemy
{
public:
	virtual ~GAME2_Enemy() = default;

	virtual void update(float deltaTime, const sf::FloatRect& playBounds);
	virtual void draw(sf::RenderWindow& window) const;

	bool isOffScreen(const sf::FloatRect& playBounds) const;
	bool isAlive() const;

	void takeDamage(float amount);
	void destroy();

	float getHealth() const;
	float getDamage() const;
	float getSpeed() const;

	sf::FloatRect getBounds() const;
	sf::FloatRect getCollisionBounds() const;

protected:
	GAME2_Enemy(float health, float damage, float speed);

	void initialiseFromSharedFrames(const std::vector<sf::Texture>& frames, sf::Vector2f startPosition);
	virtual const std::vector<sf::Texture>& getSharedFrames() const = 0;

protected:
	float m_health = 0.f;
	float m_damage = 0.f;
	float m_speed = 0.f;
	bool m_alive = true;

	sf::Vector2f m_position{ 0.f, 0.f };
	float m_spawnOriginX = 0.f;

	// These values shape the enemy's S-like winding path.
	float m_age = 0.f;
	float m_sineAmplitude = 70.f;
	float m_sineFrequency = 2.4f;

	float m_animationTimer = 0.f;
	float m_animationFrameDuration = 0.18f;
	std::size_t m_currentFrameIndex = 0;

	std::optional<sf::Sprite> m_sprite;
};

// First concrete enemy type.
// BigEnemy uses BigEnemy-1.png and BigEnemy-2.png as its animation.
class GAME2_BigEnemy final : public GAME2_Enemy
{
public:
	static bool loadSharedAssets(const std::string& enemiesDirectory, std::string& outError);

	explicit GAME2_BigEnemy(sf::Vector2f startPosition);

private:
	const std::vector<sf::Texture>& getSharedFrames() const override;

private:
	inline static std::vector<sf::Texture> s_sharedFrames;
};