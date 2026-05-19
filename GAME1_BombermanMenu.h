#pragma once

#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>
#include <optional>
#include <string>
#include <vector>

enum class GAME1_BombermanMenuAction
{
	None,
	PlayLevels,
	LevelEditor,
	BackToHub
};

class GAME1_BombermanMenu
{
public:
	bool load(const std::string& fontPath, const std::string& bombermanRootDirectory);

	void startMusic();
	void stopMusic();

	void layout(const sf::RenderWindow& window);
	GAME1_BombermanMenuAction handleClick(sf::Vector2f mousePosition);

	void selectPreviousButton();
	void selectNextButton();
	GAME1_BombermanMenuAction activateSelectedButton() const;

	void draw(sf::RenderWindow& window) const;

	const std::string& getLastError() const;

private:
	struct Button
	{
		sf::RectangleShape box;
		std::optional<sf::Text> text;
		GAME1_BombermanMenuAction action = GAME1_BombermanMenuAction::None;
	};

private:
	static bool containsPoint(const sf::FloatRect& bounds, sf::Vector2f point);
	void centerTextInButton(Button& button);
	void refreshSelectionVisuals();
	Button* getButtonByIndex(int index);
	const Button* getButtonByIndex(int index) const;

	bool tryOpenMusicFile(const std::vector<std::string>& candidatePaths);

private:
	sf::Font m_font;
	sf::Music m_music;
	bool m_hasMusic = false;

	std::optional<sf::Text> m_titleText;
	std::optional<sf::Text> m_subtitleText;

	Button m_playLevelsButton;
	Button m_levelEditorButton;
	Button m_backButton;

	int m_selectedButtonIndex = 0;

	std::string m_lastError;
};
