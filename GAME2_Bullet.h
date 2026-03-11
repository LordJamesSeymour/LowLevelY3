#pragma once

#include <SFML/Graphics.hpp>

// Simple player projectile for Game 2.
// It travels upward and uses a stretched circle so it reads like a bullet.
class GAME2_Bullet
{
public:
	explicit GAME2_Bullet(sf::Vector2f startPosition, float speed = 760.f);

	void update(float deltaTime);
	void draw(sf::RenderWindow& window) const;

	sf::FloatRect getBounds() const;

	bool isAlive() const;
	void destroy();

	bool isOffScreen(const sf::FloatRect& playBounds) const;

private:
	sf::CircleShape m_shape;
	float m_speed = 760.f;
	bool m_alive = true;
};