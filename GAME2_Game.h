#pragma once

#include "GAME2_BackgroundLayer.h"
#include "GAME2_Enemy.h"
#include "GAME2_Player.h"

#include <memory>
#include <random>
#include <string>
#include <vector>

// Main gameplay container for Game 2.
// Owns:
// - all parallax background layers
// - the player
// - the enemy spawn/update/destruction logic
class GAME2_Game
{
public:
	bool load(const std::string& resourcesDirectory, sf::Vector2u windowSize);
	void reset(sf::Vector2u windowSize);

	void update(float deltaTime, sf::Vector2u windowSize);
	void draw(sf::RenderWindow& window) const;

	const std::string& getLastError() const;

private:
	void scheduleNextSpawnDelay();
	void trySpawnEnemy(sf::Vector2u windowSize);

private:
	GAME2_BackgroundLayer m_spaceBackgroundLayer;
	GAME2_BackgroundLayer m_startsLayer;
	GAME2_BackgroundLayer m_meteorsLayer;
	GAME2_BackgroundLayer m_planetsLayer;

	GAME2_Player m_player;

	std::vector<std::unique_ptr<GAME2_Enemy>> m_enemies;

	std::mt19937 m_rng{ std::random_device{}() };
	float m_spawnTimer = 0.f;
	float m_nextSpawnDelay = 1.f;

	bool m_loaded = false;
	std::string m_resourcesDirectory;
	std::string m_lastError;
};