#include "GAME2_Bullet.h"

GAME2_Bullet::GAME2_Bullet(sf::Vector2f startPosition, float speed)
	: m_speed(speed)
{
	m_shape = sf::CircleShape(7.f);
	m_shape.setPointCount(30);

	// Make the circle look like a vertical bullet / ellipse.
	m_shape.setScale({ 0.65f, 1.95f });
	m_shape.setFillColor(sf::Color(220, 40, 40));
	m_shape.setOutlineColor(sf::Color::White);
	m_shape.setOutlineThickness(1.2f);

	const sf::FloatRect localBounds = m_shape.getLocalBounds();
	m_shape.setOrigin({
		localBounds.position.x + localBounds.size.x * 0.5f,
		localBounds.position.y + localBounds.size.y * 0.5f
		});

	m_shape.setPosition(startPosition);
}

void GAME2_Bullet::update(float deltaTime)
{
	if (!m_alive)
		return;

	m_shape.move({ 0.f, -m_speed * deltaTime });
}

void GAME2_Bullet::draw(sf::RenderWindow& window) const
{
	if (!m_alive)
		return;

	window.draw(m_shape);
}

sf::FloatRect GAME2_Bullet::getBounds() const
{
	return m_shape.getGlobalBounds();
}

bool GAME2_Bullet::isAlive() const
{
	return m_alive;
}

void GAME2_Bullet::destroy()
{
	m_alive = false;
}

bool GAME2_Bullet::isOffScreen(const sf::FloatRect& playBounds) const
{
	const sf::FloatRect bounds = m_shape.getGlobalBounds();
	return bounds.position.y + bounds.size.y < playBounds.position.y - 24.f;
}