#pragma once

#include <SFML/Graphics.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <string>

// Shared pause popup used by Super Bomberman and Surfers Quest while in
// gameplay. Centered popup with title "Paused", a green RESUME button, a
// red QUIT button, and a settings (gear) button in the top-right corner.
// All three controls are reachable via keyboard, controller, and mouse.
class GamePauseMenu
{
public:
	enum class Action
	{
		None,
		Resume,
		Quit,
		OpenSettings
	};

	bool load(const std::string& fontPath,
		const std::string& settingsIconPath);

	void layout(const sf::RenderWindow& window);

	bool isOpen() const;
	void open();
	void close();

	Action handleKeyReleased(sf::Keyboard::Key key);
	Action handleMousePressed(sf::Vector2f mousePosition);
	void handleMouseMoved(sf::Vector2f mousePosition);
	Action handleControllerInput();

	void draw(sf::RenderTarget& target) const;

	const std::string& getLastError() const;

private:
	enum class Item
	{
		Resume = 0,
		Quit = 1,
		Settings = 2,
		Count = 3
	};

	static bool containsPoint(const sf::FloatRect& rect, sf::Vector2f point);

	Action activate(Item item) const;
	void moveSelection(int delta);

	void drawButton(sf::RenderTarget& target,
		const sf::FloatRect& bounds,
		const std::string& label,
		sf::Color baseColor,
		bool selected) const;

	void drawSettingsButton(sf::RenderTarget& target, bool selected) const;

private:
	sf::Font m_font;

	sf::Texture m_settingsIconTexture;
	bool m_hasSettingsIcon = false;

	bool m_isOpen = false;

	sf::FloatRect m_panelBounds;
	sf::FloatRect m_resumeBounds;
	sf::FloatRect m_quitBounds;
	sf::FloatRect m_settingsBounds;

	Item m_selectedItem = Item::Resume;

	sf::Vector2u m_lastLayoutSize{ 0, 0 };

	std::string m_lastError;
};
