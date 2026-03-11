#pragma once

#include <SFML/Graphics.hpp>
#include <optional>
#include <string>

// For now Game 2 only has a play button, and pressing it does nothing yet.
enum class GAME2_MenuAction
{
	None,
	Play
};

// Minimal starter menu for Game 2.
// It shows a centered logo and a centered play button.
// Escape returns to the arcade hub.
class GAME2_Menu
{
public:
	bool load(const std::string& logoPath,
		const std::string& playPath);

	void layout(const sf::RenderWindow& window);
	GAME2_MenuAction handleClick(sf::Vector2f mousePosition) const;
	void draw(sf::RenderWindow& window) const;

	const std::string& getLastError() const;

private:
	static void scaleToWidth(sf::Sprite& sprite, float targetWidth);
	static bool containsPoint(const sf::FloatRect& bounds, sf::Vector2f point);

private:
	sf::Texture m_logoTexture;
	sf::Texture m_playTexture;

	std::optional<sf::Sprite> m_logoSprite;
	std::optional<sf::Sprite> m_playSprite;

	std::string m_lastError;
};