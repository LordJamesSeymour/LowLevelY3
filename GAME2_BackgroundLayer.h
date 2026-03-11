#pragma once

#include <SFML/Graphics.hpp>
#include <optional>
#include <string>

// One endlessly looping vertical background layer.
// It uses 2 copies of the same image stacked vertically.
// Both move downward together, and when one leaves the screen,
// it jumps back above the other one to create an infinite loop.
class GAME2_BackgroundLayer
{
public:
	bool load(const std::string& texturePath, float scrollSpeed, sf::Vector2u viewSize);

	void setViewSize(sf::Vector2u viewSize);
	void reset();
	void update(float deltaTime);
	void draw(sf::RenderWindow& window) const;

	// The actual visible gameplay area occupied by this layer.
	// This is useful when the art does not fill the full screen width.
	sf::FloatRect getContentBounds() const;

	const std::string& getLastError() const;

private:
	sf::Texture m_texture;
	std::optional<sf::Sprite> m_spriteA;
	std::optional<sf::Sprite> m_spriteB;

	sf::Vector2u m_viewSize{ 0, 0 };

	float m_scrollSpeed = 0.f;

	// Cached scaled dimensions after applying uniform scaling.
	float m_scaledWidth = 0.f;
	float m_scaledHeight = 0.f;

	// Horizontal draw position so the layer is centered.
	float m_drawX = 0.f;

	std::string m_lastError;
};