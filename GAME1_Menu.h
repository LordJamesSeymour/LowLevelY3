#pragma once

#include <SFML/Graphics.hpp>

#include <optional>
#include <string>

enum class GAME1_MenuAction
{
	None,
	Quit,
	Play,
	LevelEditor
};

class GAME1_Menu
{
public:
	// Kept the old 4-argument signature so main.cpp does not need changing.
	// The old PNG paths are no longer used because this menu is now text/button based.
	bool load(const std::string& closeButtonTexturePath,
		const std::string& logoTexturePath,
		const std::string& playButtonTexturePath,
		const std::string& levelEditorTexturePath);

	void layout(const sf::RenderWindow& window);
	GAME1_MenuAction handleClick(sf::Vector2f mousePosition) const;
	void draw(sf::RenderWindow& window) const;

	const std::string& getLastError() const;

private:
	struct Button
	{
		sf::RectangleShape box;
		std::optional<sf::Text> text;
		GAME1_MenuAction action = GAME1_MenuAction::None;
	};

private:
	static bool containsPoint(const sf::FloatRect& bounds, sf::Vector2f point);
	void centerTextInButton(Button& button);

private:
	sf::Font m_font;

	std::optional<sf::Text> m_titleText;
	std::optional<sf::Text> m_subtitleText;

	Button m_playButton;
	Button m_levelEditorButton;
	Button m_backButton;

	std::string m_lastError;
};