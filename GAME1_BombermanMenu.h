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
	void handleMouseMoved(sf::Vector2f mousePosition);
	bool handleKeyReleased(sf::Keyboard::Key key);
	GAME1_BombermanMenuAction handleControllerInput();

	void resetSelection();
	void selectPreviousItem();
	void selectNextItem();
	GAME1_BombermanMenuAction activateSelectedItem();
	void draw(sf::RenderWindow& window) const;

	bool isControlsPopupOpen() const;
	const std::string& getLastError() const;

private:
	enum class MenuSelection
	{
		PlayLevels = 0,
		LevelEditor = 1,
		BackToHub = 2,
		Controls = 3
	};

	struct Button
	{
		sf::RectangleShape box;
		std::optional<sf::Text> text;
		GAME1_BombermanMenuAction action = GAME1_BombermanMenuAction::None;
	};

private:
	static bool containsPoint(const sf::FloatRect& bounds, sf::Vector2f point);
	void centerTextInButton(Button& button);
	void updateSelectionVisuals();
	Button* getButtonForSelection(MenuSelection selection);
	const Button* getButtonForSelection(MenuSelection selection) const;
	void setSelectedItem(MenuSelection selection);

	void showPreviousControlsPage();
	void showNextControlsPage();
	std::string getControlsPopupTitle() const;
	std::string getControlsPopupBody() const;

	void drawTextCentered(sf::RenderTarget& target,
		const std::string& string,
		unsigned int size,
		const sf::FloatRect& rect,
		sf::Color fill,
		float outlineThickness = 2.f) const;

	void drawControlsPopup(sf::RenderTarget& target, sf::Vector2u windowSize) const;

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
	Button m_controlsButton;
	MenuSelection m_selectedItem = MenuSelection::PlayLevels;

	bool m_controlsPopupOpen = false;
	int m_controlsPopupPage = 0;
	sf::FloatRect m_popupBounds;
	sf::FloatRect m_popupPreviousPageButtonBounds;
	sf::FloatRect m_popupNextPageButtonBounds;

	std::string m_lastError;
};
