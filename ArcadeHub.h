#pragma once

#include <SFML/Graphics.hpp>
#include <optional>
#include <string>
#include <vector>

enum class ArcadeHubAction
{
	None,
	PreviousGame,
	NextGame,
	LaunchGame
};

struct ArcadeHubGameEntry
{
	std::string label;
	std::string displayName;

	// Folder containing ordered PNG frames for the animated splash.
	// Example: Assets/Game#1/GIFs/SplashScreen
	std::string splashFramesDirectory;
};

class ArcadeHub
{
public:
	bool load(const std::string& fontPath,
		const std::string& projectName,
		const std::vector<ArcadeHubGameEntry>& games);

	void updateClockText();
	void updateAnimation(float deltaTime);
	void updateVisualTheme(float totalTimeSeconds);

	void layout(const sf::RenderWindow& window);

	void navigateLeft();
	void navigateRight();

	ArcadeHubAction handleClick(sf::Vector2f mousePosition) const;

	void draw(sf::RenderTarget& target) const;

	std::size_t getSelectedIndex() const;
	const ArcadeHubGameEntry& getSelectedGame() const;

	const std::string& getLastError() const;

private:
	struct LoadedGameCard
	{
		ArcadeHubGameEntry data;
		std::vector<sf::Texture> splashFrames;
	};

private:
	static bool containsPoint(const sf::FloatRect& bounds, sf::Vector2f point);
	static sf::Color ColorFromHSV(float hueDegrees, float saturation, float value);

	void updateDisplayedTexts();

private:
	sf::Font m_font;

	std::vector<LoadedGameCard> m_games;
	std::size_t m_selectedIndex = 0;

	std::size_t m_currentFrameIndex = 0;
	float m_animationTimer = 0.f;
	float m_animationFrameDuration = 1.f / 24.f;

	std::optional<sf::Text> m_projectNameText;
	std::optional<sf::Text> m_clockText;
	std::optional<sf::Text> m_creditText;
	std::optional<sf::Text> m_gameLabelText;
	std::optional<sf::Text> m_gameNameText;
	std::optional<sf::Text> m_launchHintText;
	std::optional<sf::Text> m_leftArrowText;
	std::optional<sf::Text> m_rightArrowText;

	sf::FloatRect m_splashButtonBounds;
	sf::FloatRect m_leftButtonBounds;
	sf::FloatRect m_rightButtonBounds;

	sf::Vector2u m_lastLayoutSize{ 0, 0 };

	sf::Color m_themeBright = sf::Color::White;
	sf::Color m_themeMid = sf::Color(180, 180, 180);
	sf::Color m_themeDark = sf::Color(35, 35, 35);
	sf::Color m_themeBackground = sf::Color(12, 12, 12);

	std::string m_lastError;
};