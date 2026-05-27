#pragma once

#include <SFML/Graphics.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/Mouse.hpp>
#include <string>

// In-game options popup overlay shown above the ArcadeHub. Always draws a
// gear icon in the bottom-right corner. When the gear is clicked (or the
// popup is otherwise opened), it overlays a modal panel with:
//   - CRT shader toggle
//   - Music volume slider (0..10, step 0.5)
//   - SFX volume slider (0..10, step 0.5)
//   - Close button
class ArcadeHubOptions
{
public:
	bool load(const std::string& fontPath, const std::string& settingsIconPath);

	void layout(const sf::RenderWindow& window);

	bool isOpen() const;
	void open();
	void close();

	const sf::FloatRect& getGearBounds() const;

	// Returns true when the click was consumed by the gear button or popup.
	bool handleMousePressed(sf::Vector2f mousePosition);
	void handleMouseReleased(sf::Vector2f mousePosition);
	void handleMouseMoved(sf::Vector2f mousePosition);

	bool handleKeyReleased(sf::Keyboard::Key key);
	void handleControllerInput();

	void draw(sf::RenderTarget& target) const;

	const std::string& getLastError() const;

private:
	enum class Row
	{
		CrtShader = 0,
		Music = 1,
		Sfx = 2,
		Close = 3,
		Count = 4
	};

	static bool containsPoint(const sf::FloatRect& rect, sf::Vector2f point);

	void moveSelection(int delta);
	void adjustSelected(int direction);
	void activateSelected();

	float sliderFractionFromX(const sf::FloatRect& row, float x) const;
	void getSliderTrackEdges(const sf::FloatRect& row, float& outLeft, float& outRight) const;

	void drawRowHighlight(sf::RenderTarget& target,
		const sf::FloatRect& bounds,
		bool selected) const;

	void drawSliderRow(sf::RenderTarget& target,
		const sf::FloatRect& row,
		const std::string& labelStr,
		float value,
		float maxValue,
		bool selected) const;

private:
	sf::Font m_font;

	sf::Texture m_settingsIconTexture;
	bool m_hasSettingsIcon = false;

	bool m_isOpen = false;

	sf::FloatRect m_gearBounds;
	sf::FloatRect m_panelBounds;
	sf::FloatRect m_crtToggleBounds;
	sf::FloatRect m_musicSliderBounds;
	sf::FloatRect m_sfxSliderBounds;
	sf::FloatRect m_closeButtonBounds;

	Row m_selectedRow = Row::CrtShader;

	bool m_draggingMusic = false;
	bool m_draggingSfx = false;

	sf::Vector2u m_lastLayoutSize{ 0, 0 };

	std::string m_lastError;
};
