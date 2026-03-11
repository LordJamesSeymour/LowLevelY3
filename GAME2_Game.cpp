#include "GAME2_Game.h"

#include <algorithm>
#include <filesystem>

bool GAME2_Game::load(const std::string& resourcesDirectory, sf::Vector2u windowSize)
{
	namespace fs = std::filesystem;

	m_lastError.clear();
	m_resourcesDirectory = resourcesDirectory;

	const fs::path backgroundDirectory = fs::path(resourcesDirectory) / "Background";
	const fs::path playerDirectory = fs::path(resourcesDirectory) / "Player";
	const fs::path enemiesDirectory = fs::path(resourcesDirectory) / "Enemies";

	// Faster overall parallax, with stronger separation:
	// Space = fastest
	// Stars = a bit faster
	// Meteors = slower
	// Planets = way slower
	if (!m_spaceBackgroundLayer.load((backgroundDirectory / "Space_BG_01.png").generic_string(), 420.f, windowSize))
	{
		m_lastError = m_spaceBackgroundLayer.getLastError();
		return false;
	}

	if (!m_startsLayer.load((backgroundDirectory / "Stars.png").generic_string(), 210.f, windowSize))
	{
		m_lastError = m_startsLayer.getLastError();
		return false;
	}

	if (!m_meteorsLayer.load((backgroundDirectory / "Meteors.png").generic_string(), 70.f, windowSize))
	{
		m_lastError = m_meteorsLayer.getLastError();
		return false;
	}

	if (!m_planetsLayer.load((backgroundDirectory / "Planets.png").generic_string(), 28.f, windowSize))
	{
		m_lastError = m_planetsLayer.getLastError();
		return false;
	}

	// The gameplay area is defined by the visible width of the main background.
	const sf::FloatRect playBounds = m_spaceBackgroundLayer.getContentBounds();

	if (!m_player.load(playerDirectory.generic_string(), playBounds))
	{
		m_lastError = m_player.getLastError();
		return false;
	}

	if (!GAME2_BigEnemy::loadSharedAssets(enemiesDirectory.generic_string(), m_lastError))
	{
		return false;
	}

	m_loaded = true;
	reset(windowSize);

	return true;
}

void GAME2_Game::reset(sf::Vector2u windowSize)
{
	if (!m_loaded)
		return;

	m_spaceBackgroundLayer.setViewSize(windowSize);
	m_startsLayer.setViewSize(windowSize);
	m_meteorsLayer.setViewSize(windowSize);
	m_planetsLayer.setViewSize(windowSize);

	m_spaceBackgroundLayer.reset();
	m_startsLayer.reset();
	m_meteorsLayer.reset();
	m_planetsLayer.reset();

	const sf::FloatRect playBounds = m_spaceBackgroundLayer.getContentBounds();
	m_player.reset(playBounds);

	m_enemies.clear();

	scheduleNextSpawnDelay();
	m_spawnTimer = 0.45f;
}

void GAME2_Game::scheduleNextSpawnDelay()
{
	std::uniform_real_distribution<float> spawnDelayDistribution(0.55f, 1.30f);
	m_nextSpawnDelay = spawnDelayDistribution(m_rng);
}

void GAME2_Game::trySpawnEnemy(sf::Vector2u windowSize)
{
	if (m_enemies.size() >= 10)
		return;

	const sf::FloatRect playBounds = m_spaceBackgroundLayer.getContentBounds();

	const float leftMargin = 20.f;
	const float rightMargin = 100.f;

	const float minX = playBounds.position.x + leftMargin;
	const float maxX = std::max(minX, playBounds.position.x + playBounds.size.x - rightMargin);

	std::uniform_real_distribution<float> xDistribution(minX, maxX);

	const float spawnX = xDistribution(m_rng);
	const float spawnY = playBounds.position.y - 120.f;

	m_enemies.push_back(std::make_unique<GAME2_BigEnemy>(sf::Vector2f{ spawnX, spawnY }));
}

void GAME2_Game::update(float deltaTime, sf::Vector2u windowSize)
{
	if (!m_loaded)
		return;

	m_spaceBackgroundLayer.setViewSize(windowSize);
	m_startsLayer.setViewSize(windowSize);
	m_meteorsLayer.setViewSize(windowSize);
	m_planetsLayer.setViewSize(windowSize);

	m_spaceBackgroundLayer.update(deltaTime);
	m_startsLayer.update(deltaTime);
	m_meteorsLayer.update(deltaTime);
	m_planetsLayer.update(deltaTime);

	const sf::FloatRect playBounds = m_spaceBackgroundLayer.getContentBounds();

	m_player.update(deltaTime, playBounds);

	if (m_enemies.size() < 10)
	{
		m_spawnTimer -= deltaTime;

		if (m_spawnTimer <= 0.f)
		{
			trySpawnEnemy(windowSize);
			scheduleNextSpawnDelay();
			m_spawnTimer = m_nextSpawnDelay;
		}
	}

	for (std::unique_ptr<GAME2_Enemy>& enemy : m_enemies)
	{
		enemy->update(deltaTime, playBounds);
	}

	m_enemies.erase(
		std::remove_if(m_enemies.begin(), m_enemies.end(),
			[playBounds](const std::unique_ptr<GAME2_Enemy>& enemy)
			{
				return enemy->isOffScreen(playBounds);
			}),
		m_enemies.end());
}

void GAME2_Game::draw(sf::RenderWindow& window) const
{
	m_spaceBackgroundLayer.draw(window);
	m_startsLayer.draw(window);
	m_meteorsLayer.draw(window);
	m_planetsLayer.draw(window);

	for (const std::unique_ptr<GAME2_Enemy>& enemy : m_enemies)
	{
		enemy->draw(window);
	}

	m_player.draw(window);
}

const std::string& GAME2_Game::getLastError() const
{
	return m_lastError;
}