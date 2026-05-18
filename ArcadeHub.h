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

	std::string splashFramesDirectory;
	std::string splashStillImagePath;

	bool isLocked = false;
};

class ArcadeHub
{
public:
	bool load(const std::string& fontPath,
		const std::string& projectName,
		const std::vector<ArcadeHubGameEntry>& games,
		const std::string& lockedImagePath = "");

	void updateClockText();
	void updateAnimation(float deltaTime);
	void updateVisualTheme(float totalTimeSeconds);
	void notifyUserActivity();

	void layout(const sf::RenderWindow& window);

	void navigateLeft();
	void navigateRight();

	ArcadeHubAction handleClick(sf::Vector2f mousePosition) const;

	void draw(sf::RenderTarget& target) const;

	std::size_t getSelectedIndex() const;
	const ArcadeHubGameEntry& getSelectedGame() const;
	bool isTransitioning() const;

	bool isGameLocked(std::size_t gameIndex) const;
	bool isSelectedGameLocked() const;

	const std::string& getLastError() const;

private:
	struct LoadedGameCard
	{
		ArcadeHubGameEntry data;

		sf::Texture splashStillTexture;
		bool hasSplashStillTexture = false;

		std::vector<sf::Texture> splashFrames;

		std::size_t currentFrameIndex = 0;
		float animationTimer = 0.f;
	};

private:
	static bool containsPoint(const sf::FloatRect& bounds, sf::Vector2f point);
	static sf::Color ColorFromHSV(float hueDegrees, float saturation, float value);
	static float ApplySwipeCurve(float t);

	void drawGameCard(sf::RenderTarget& target, std::size_t gameIndex, float offsetX) const;
	const sf::Texture* getCurrentTextureForGame(std::size_t gameIndex) const;

private:
	sf::Font m_font;

	std::vector<LoadedGameCard> m_games;

	sf::Texture m_lockTexture;
	bool m_hasLockTexture = false;

	std::size_t m_selectedIndex = 0;

	bool m_isSwiping = false;
	std::size_t m_previousIndex = 0;
	int m_swipeDirection = 1;
	float m_swipeTimer = 0.f;
	float m_swipeDuration = 0.58f;

	float m_animationFrameDuration = 1.f / 24.f;

	float m_idleTimer = 0.f;
	float m_idleDelayBeforeAnimation = 5.f;

	std::optional<sf::Text> m_projectNameText;
	std::optional<sf::Text> m_clockText;
	std::optional<sf::Text> m_creditText;
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