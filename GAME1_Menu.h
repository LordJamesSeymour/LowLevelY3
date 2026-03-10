#pragma once

#include <SFML/Graphics.hpp>
#include <optional>
#include <string>

// These are the three outcomes of clicking on the Game 1 menu.
enum class GAME1_MenuAction
{
	None,
	Play,
	LevelEditor,
	Quit
};

// Game 1's internal front menu.
// This is separate from the new arcade hub, which now sits above all games.
class GAME1_Menu
{
public:
	bool load(const std::string& closeButtonPath,
		const std::string& logoPath,
		const std::string& playPath,
		const std::string& levelEditorPath);

	void layout(const sf::RenderWindow& window);
	GAME1_MenuAction handleClick(sf::Vector2f mousePosition) const;
	void draw(sf::RenderWindow& window) const;

	const std::string& getLastError() const;

private:
	static void scaleToWidth(sf::Sprite& sprite, float targetWidth);
	static void scaleToSize(sf::Sprite& sprite, float targetWidth, float targetHeight);
	static bool containsPoint(const sf::FloatRect& bounds, sf::Vector2f point);

private:
	sf::Texture m_closeTexture;
	sf::Texture m_logoTexture;
	sf::Texture m_playTexture;
	sf::Texture m_levelEditorTexture;

	std::optional<sf::Sprite> m_closeSprite;
	std::optional<sf::Sprite> m_logoSprite;
	std::optional<sf::Sprite> m_playSprite;
	std::optional<sf::Sprite> m_levelEditorSprite;

	std::string m_lastError;
};