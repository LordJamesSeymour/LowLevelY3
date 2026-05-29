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
	bool load(const std::string& fontPath,
		const std::string& settingsIconPath,
		const std::string& volumeOnIconPath = "assets/Volume.png",
		const std::string& volumeOffIconPath = "assets/Volume2.png");

	void layout(const sf::RenderWindow& window);
	void update(float deltaTime);

	bool isOpen() const;
	void open();
	void close();

	// Hides the bottom-right gear icon. The options panel still works
	// (it can be opened via open() and closed normally), but the screen-corner
	// button is suppressed so other UI (like the pause popup) can own the
	// "open settings" action without a duplicated gear in the screen corner.
	void setGearVisible(bool visible);
	bool isGearVisible() const;

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
		Fullscreen = 0,
		CrtShader = 1,
		Music = 2,
		Sfx = 3,
		Close = 4,
		Count = 5
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

	void drawMuteButton(sf::RenderTarget& target,
		const sf::FloatRect& bounds,
		float animationT) const;

private:
	sf::Font m_font;

	sf::Texture m_settingsIconTexture;
	bool m_hasSettingsIcon = false;

	sf::Texture m_volumeOnTexture;
	sf::Texture m_volumeOffTexture;
	bool m_hasVolumeOnIcon = false;
	bool m_hasVolumeOffIcon = false;

	bool m_isOpen = false;
	bool m_gearVisible = true;

	sf::FloatRect m_gearBounds;
	sf::FloatRect m_panelBounds;
	sf::FloatRect m_fullscreenToggleBounds;
	sf::FloatRect m_crtToggleBounds;
	sf::FloatRect m_musicSliderBounds;
	sf::FloatRect m_sfxSliderBounds;
	sf::FloatRect m_musicMuteButtonBounds;
	sf::FloatRect m_sfxMuteButtonBounds;
	sf::FloatRect m_closeButtonBounds;

	// Animation 0..1: 0 = unmuted (Volume.png, no X), 1 = muted (Volume2.png + red X)
	float m_musicMuteAnimT = 0.f;
	float m_sfxMuteAnimT = 0.f;

	Row m_selectedRow = Row::Fullscreen;

	bool m_draggingMusic = false;
	bool m_draggingSfx = false;

	sf::Vector2u m_lastLayoutSize{ 0, 0 };

	std::string m_lastError;
};
