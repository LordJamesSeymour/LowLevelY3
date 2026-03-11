#pragma once

#include "GAME2_BackgroundLayer.h"
#include "GAME2_Bullet.h"
#include "GAME2_Enemy.h"
#include "GAME2_Player.h"

#include <SFML/Graphics.hpp>
#include <memory>
#include <random>
#include <string>
#include <vector>

enum class GAME2_GameAction
{
	None,
	BackToMenu
};

// Main gameplay container for Game 2.
// Owns:
// - all parallax background layers
// - the player
// - player bullets
// - the enemy spawn/update/destruction logic
// - lives UI and game over menu
class GAME2_Game
{
public:
	GAME2_Game();

	bool load(const std::string& resourcesDirectory, sf::Vector2u windowSize);
	void reset(sf::Vector2u windowSize);

	void update(float deltaTime, sf::Vector2u windowSize);
	void draw(sf::RenderWindow& window) const;

	GAME2_GameAction handleClick(sf::Vector2f mousePosition, sf::Vector2u windowSize);

	const std::string& getLastError() const;

private:
	void scheduleNextSpawnDelay();
	void trySpawnEnemy();
	void refreshOverlayLayout(sf::Vector2u windowSize);

	sf::FloatRect getPlayBounds() const;

private:
	GAME2_BackgroundLayer m_spaceBackgroundLayer;
	GAME2_BackgroundLayer m_startsLayer;
	GAME2_BackgroundLayer m_meteorsLayer;
	GAME2_BackgroundLayer m_planetsLayer;

	GAME2_Player m_player;

	std::vector<GAME2_Bullet> m_bullets;
	std::vector<std::unique_ptr<GAME2_Enemy>> m_enemies;

	std::mt19937 m_rng{ std::random_device{}() };
	float m_spawnTimer = 0.f;
	float m_nextSpawnDelay = 1.f;

	sf::Font m_uiFont;
	sf::Text m_livesText;
	sf::Text m_gameOverText;
	sf::Text m_tryAgainText;
	sf::Text m_backToMenuText;

	sf::RectangleShape m_tryAgainButton;
	sf::RectangleShape m_backToMenuButton;

	bool m_gameOver = false;
	bool m_loaded = false;

	float m_gameOverParallaxMultiplier = 0.12f;

	std::string m_resourcesDirectory;
	std::string m_lastError;
};