#include "GAME2_Game.h"

#include <algorithm>
#include <filesystem>

GAME2_Game::GAME2_Game()
	: m_livesText(m_uiFont),
	m_gameOverText(m_uiFont),
	m_tryAgainText(m_uiFont),
	m_backToMenuText(m_uiFont)
{
}

namespace
{
	bool RectsIntersect(const sf::FloatRect& a, const sf::FloatRect& b)
	{
		return a.position.x < b.position.x + b.size.x &&
			a.position.x + a.size.x > b.position.x &&
			a.position.y < b.position.y + b.size.y &&
			a.position.y + a.size.y > b.position.y;
	}

	bool ContainsPoint(const sf::FloatRect& rect, sf::Vector2f point)
	{
		return point.x >= rect.position.x &&
			point.x <= rect.position.x + rect.size.x &&
			point.y >= rect.position.y &&
			point.y <= rect.position.y + rect.size.y;
	}

	void CenterTextInRect(sf::Text& text, const sf::FloatRect& rect)
	{
		const sf::FloatRect bounds = text.getLocalBounds();
		text.setPosition({
			rect.position.x + (rect.size.x - bounds.size.x) * 0.5f - bounds.position.x,
			rect.position.y + (rect.size.y - bounds.size.y) * 0.5f - bounds.position.y
			});
	}
}

bool GAME2_Game::load(const std::string& resourcesDirectory, sf::Vector2u windowSize)
{
	namespace fs = std::filesystem;

	m_lastError.clear();
	m_resourcesDirectory = resourcesDirectory;

	const fs::path backgroundDirectory = fs::path(resourcesDirectory) / "Background";
	const fs::path playerDirectory = fs::path(resourcesDirectory) / "Player";
	const fs::path enemiesDirectory = fs::path(resourcesDirectory) / "Enemies";
	const fs::path uiFontPath = fs::path(resourcesDirectory).parent_path().parent_path() / "menu.ttf";

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

	if (!m_uiFont.openFromFile(uiFontPath.string()))
	{
		m_lastError = "Failed to load Game 2 UI font: " + uiFontPath.string();
		return false;
	}

	m_livesText.setCharacterSize(24);
	m_livesText.setFillColor(sf::Color::White);
	m_livesText.setOutlineColor(sf::Color::Black);
	m_livesText.setOutlineThickness(2.f);

	m_gameOverText.setString("GAME OVER");
	m_gameOverText.setCharacterSize(60);
	m_gameOverText.setFillColor(sf::Color::White);
	m_gameOverText.setOutlineColor(sf::Color::Black);
	m_gameOverText.setOutlineThickness(3.f);

	m_tryAgainText.setString("TRY AGAIN");
	m_tryAgainText.setCharacterSize(28);
	m_tryAgainText.setFillColor(sf::Color::White);
	m_tryAgainText.setOutlineColor(sf::Color::Black);
	m_tryAgainText.setOutlineThickness(2.f);

	m_backToMenuText.setString("BACK TO THE MENU");
	m_backToMenuText.setCharacterSize(28);
	m_backToMenuText.setFillColor(sf::Color::White);
	m_backToMenuText.setOutlineColor(sf::Color::Black);
	m_backToMenuText.setOutlineThickness(2.f);

	m_tryAgainButton.setSize({ 240.f, 62.f });
	m_tryAgainButton.setFillColor(sf::Color(40, 140, 60));
	m_tryAgainButton.setOutlineColor(sf::Color::White);
	m_tryAgainButton.setOutlineThickness(2.f);

	m_backToMenuButton.setSize({ 320.f, 62.f });
	m_backToMenuButton.setFillColor(sf::Color(130, 60, 60));
	m_backToMenuButton.setOutlineColor(sf::Color::White);
	m_backToMenuButton.setOutlineThickness(2.f);

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

	m_player.reset(getPlayBounds());

	m_bullets.clear();
	m_enemies.clear();

	m_gameOver = false;

	scheduleNextSpawnDelay();
	m_spawnTimer = 0.45f;

	m_livesText.setString("LIVES: 3");
	refreshOverlayLayout(windowSize);
}

void GAME2_Game::scheduleNextSpawnDelay()
{
	std::uniform_real_distribution<float> spawnDelayDistribution(0.55f, 1.30f);
	m_nextSpawnDelay = spawnDelayDistribution(m_rng);
}

void GAME2_Game::trySpawnEnemy()
{
	if (m_enemies.size() >= 10)
		return;

	const sf::FloatRect playBounds = getPlayBounds();

	const float leftMargin = 20.f;
	const float rightMargin = 100.f;

	const float minX = playBounds.position.x + leftMargin;
	const float maxX = std::max(minX, playBounds.position.x + playBounds.size.x - rightMargin);

	std::uniform_real_distribution<float> xDistribution(minX, maxX);

	const float spawnX = xDistribution(m_rng);
	const float spawnY = playBounds.position.y - 120.f;

	m_enemies.push_back(std::make_unique<GAME2_BigEnemy>(sf::Vector2f{ spawnX, spawnY }));
}

void GAME2_Game::refreshOverlayLayout(sf::Vector2u windowSize)
{
	const float windowWidth = static_cast<float>(windowSize.x);
	const float windowHeight = static_cast<float>(windowSize.y);

	m_livesText.setPosition({ 20.f, 18.f });

	const sf::FloatRect gameOverBounds = m_gameOverText.getLocalBounds();
	m_gameOverText.setPosition({
		windowWidth * 0.5f - gameOverBounds.size.x * 0.5f - gameOverBounds.position.x,
		windowHeight * 0.5f - 150.f
		});

	m_tryAgainButton.setPosition({
		windowWidth * 0.5f - m_tryAgainButton.getSize().x * 0.5f,
		windowHeight * 0.5f - 18.f
		});

	m_backToMenuButton.setPosition({
		windowWidth * 0.5f - m_backToMenuButton.getSize().x * 0.5f,
		windowHeight * 0.5f + 68.f
		});

	CenterTextInRect(m_tryAgainText, m_tryAgainButton.getGlobalBounds());
	CenterTextInRect(m_backToMenuText, m_backToMenuButton.getGlobalBounds());
}

sf::FloatRect GAME2_Game::getPlayBounds() const
{
	return m_spaceBackgroundLayer.getContentBounds();
}

void GAME2_Game::update(float deltaTime, sf::Vector2u windowSize)
{
	if (!m_loaded)
		return;

	m_spaceBackgroundLayer.setViewSize(windowSize);
	m_startsLayer.setViewSize(windowSize);
	m_meteorsLayer.setViewSize(windowSize);
	m_planetsLayer.setViewSize(windowSize);

	const float parallaxDeltaTime = m_gameOver
		? deltaTime * m_gameOverParallaxMultiplier
		: deltaTime;

	m_spaceBackgroundLayer.update(parallaxDeltaTime);
	m_startsLayer.update(parallaxDeltaTime);
	m_meteorsLayer.update(parallaxDeltaTime);
	m_planetsLayer.update(parallaxDeltaTime);

	refreshOverlayLayout(windowSize);

	if (m_gameOver)
		return;

	const sf::FloatRect playBounds = getPlayBounds();

	m_player.update(deltaTime, playBounds);
	m_player.updateShooting(deltaTime, m_bullets);

	if (m_enemies.size() < 10)
	{
		m_spawnTimer -= deltaTime;

		if (m_spawnTimer <= 0.f)
		{
			trySpawnEnemy();
			scheduleNextSpawnDelay();
			m_spawnTimer = m_nextSpawnDelay;
		}
	}

	for (GAME2_Bullet& bullet : m_bullets)
	{
		bullet.update(deltaTime);
	}

	for (std::unique_ptr<GAME2_Enemy>& enemy : m_enemies)
	{
		enemy->update(deltaTime, playBounds);
	}

	// Bullet vs enemy collisions.
	for (GAME2_Bullet& bullet : m_bullets)
	{
		if (!bullet.isAlive())
			continue;

		for (std::unique_ptr<GAME2_Enemy>& enemy : m_enemies)
		{
			if (!enemy->isAlive())
				continue;

			if (RectsIntersect(bullet.getBounds(), enemy->getCollisionBounds()))
			{
				bullet.destroy();
				enemy->takeDamage(1.f);
				break;
			}
		}
	}

	// Enemy vs player collisions.
	for (std::unique_ptr<GAME2_Enemy>& enemy : m_enemies)
	{
		if (!enemy->isAlive())
			continue;

		if (RectsIntersect(enemy->getCollisionBounds(), m_player.getCollisionBounds()))
		{
			m_player.takeHit();
			enemy->destroy();

			if (m_player.isGameOver())
			{
				m_gameOver = true;
				break;
			}
		}
	}

	m_bullets.erase(
		std::remove_if(
			m_bullets.begin(),
			m_bullets.end(),
			[playBounds](const GAME2_Bullet& bullet)
			{
				return !bullet.isAlive() || bullet.isOffScreen(playBounds);
			}),
		m_bullets.end());

	m_enemies.erase(
		std::remove_if(
			m_enemies.begin(),
			m_enemies.end(),
			[playBounds](const std::unique_ptr<GAME2_Enemy>& enemy)
			{
				return !enemy->isAlive() || enemy->isOffScreen(playBounds);
			}),
		m_enemies.end());

	m_livesText.setString("LIVES: " + std::to_string(m_player.getLives()));
}

void GAME2_Game::draw(sf::RenderWindow& window) const
{
	const sf::View defaultView = window.getDefaultView();

	// Zoom the gameplay in without stretching textures.
	// Smaller view size = closer camera.
	sf::View gameplayView = defaultView;
	gameplayView.setSize({
		defaultView.getSize().x * m_gameplayZoom,
		defaultView.getSize().y * m_gameplayZoom
		});
	gameplayView.setCenter(defaultView.getCenter());

	window.setView(gameplayView);

	m_spaceBackgroundLayer.draw(window);
	m_startsLayer.draw(window);
	m_meteorsLayer.draw(window);
	m_planetsLayer.draw(window);

	for (const GAME2_Bullet& bullet : m_bullets)
	{
		bullet.draw(window);
	}

	for (const std::unique_ptr<GAME2_Enemy>& enemy : m_enemies)
	{
		enemy->draw(window);
	}

	m_player.draw(window);

	// Return to the normal screen-space view for UI.
	window.setView(defaultView);

	window.draw(m_livesText);

	if (m_gameOver)
	{
		sf::RectangleShape overlay;
		overlay.setSize({
			static_cast<float>(window.getSize().x),
			static_cast<float>(window.getSize().y)
			});
		overlay.setFillColor(sf::Color(0, 0, 0, 170));

		window.draw(overlay);
		window.draw(m_gameOverText);
		window.draw(m_tryAgainButton);
		window.draw(m_backToMenuButton);
		window.draw(m_tryAgainText);
		window.draw(m_backToMenuText);
	}
}

GAME2_GameAction GAME2_Game::handleClick(sf::Vector2f mousePosition, sf::Vector2u windowSize)
{
	if (!m_loaded || !m_gameOver)
		return GAME2_GameAction::None;

	refreshOverlayLayout(windowSize);

	if (ContainsPoint(m_tryAgainButton.getGlobalBounds(), mousePosition))
	{
		reset(windowSize);
		return GAME2_GameAction::None;
	}

	if (ContainsPoint(m_backToMenuButton.getGlobalBounds(), mousePosition))
	{
		return GAME2_GameAction::BackToMenu;
	}

	return GAME2_GameAction::None;
}

const std::string& GAME2_Game::getLastError() const
{
	return m_lastError;
}