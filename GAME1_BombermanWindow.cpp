#include "GAME1_BombermanWindow.h"

#include <algorithm>
#include <filesystem>

bool GAME1_BombermanWindow::load(const std::string& fontPath, const std::string& bombermanRootDirectory)
{
	namespace fs = std::filesystem;

	m_lastError.clear();

	m_bombermanRootDirectory = bombermanRootDirectory;
	m_resourcesDirectory = (fs::path(m_bombermanRootDirectory) / "Resources").string();
	m_mapsDirectory = (fs::path(m_bombermanRootDirectory) / "Maps").string();

	if (!m_font.openFromFile(fontPath))
	{
		m_lastError = "Failed to load Bomberman font: " + fontPath;
		return false;
	}

	m_statusText.emplace(m_font);
	m_statusText->setCharacterSize(44);
	m_statusText->setFillColor(sf::Color::White);
	m_statusText->setOutlineColor(sf::Color::Black);
	m_statusText->setOutlineThickness(3.f);

	m_helpText.emplace(m_font);
	m_helpText->setCharacterSize(20);
	m_helpText->setFillColor(sf::Color(230, 230, 230));
	m_helpText->setOutlineColor(sf::Color::Black);
	m_helpText->setOutlineThickness(1.5f);

	m_statsText.emplace(m_font);
	m_statsText->setCharacterSize(22);
	m_statsText->setFillColor(sf::Color::White);
	m_statsText->setOutlineColor(sf::Color::Black);
	m_statsText->setOutlineThickness(2.f);

	m_objectiveText.emplace(m_font);
	m_objectiveText->setCharacterSize(22);
	m_objectiveText->setFillColor(sf::Color(255, 230, 120));
	m_objectiveText->setOutlineColor(sf::Color::Black);
	m_objectiveText->setOutlineThickness(2.f);

	m_powerUpHudText.emplace(m_font);
	m_powerUpHudText->setCharacterSize(20);
	m_powerUpHudText->setFillColor(sf::Color::White);
	m_powerUpHudText->setOutlineColor(sf::Color::Black);
	m_powerUpHudText->setOutlineThickness(2.f);

	const fs::path bombsDirectory = fs::path(m_resourcesDirectory) / "Bombs";

	if (!loadTexture(m_bombTexture, (bombsDirectory / "bomb.png").string(), "bomb"))
		return false;

	if (!loadTexture(m_explosionCenterTexture, (bombsDirectory / "explosion_center.png").string(), "explosion center"))
		return false;

	if (!loadTexture(m_explosionHorizontalTexture, (bombsDirectory / "explosion_horizontal.png").string(), "horizontal explosion"))
		return false;

	if (!loadTexture(m_explosionVerticalTexture, (bombsDirectory / "explosion_vertical.png").string(), "vertical explosion"))
		return false;

	const fs::path powerUpsDirectory = fs::path(m_resourcesDirectory) / "PowerUps";

	tryLoadPowerUpTexture(m_fireUpTexture,
		{
			(powerUpsDirectory / "fire_up.png").string(),
			(powerUpsDirectory / "FireUp.png").string(),
			(powerUpsDirectory / "fire.png").string(),
			(powerUpsDirectory / "powerup_fire.png").string()
		});

	tryLoadPowerUpTexture(m_bombUpTexture,
		{
			(powerUpsDirectory / "bomb_up.png").string(),
			(powerUpsDirectory / "BombUp.png").string(),
			(powerUpsDirectory / "bombup.png").string(),
			(powerUpsDirectory / "powerup_bomb.png").string()
		});

	tryLoadPowerUpTexture(m_speedUpTexture,
		{
			(powerUpsDirectory / "speed_up.png").string(),
			(powerUpsDirectory / "SpeedUp.png").string(),
			(powerUpsDirectory / "speed.png").string(),
			(powerUpsDirectory / "powerup_speed.png").string()
		});

	if (!m_player.load((fs::path(m_resourcesDirectory) / "Player").string()))
	{
		m_lastError = m_player.getLastError();
		return false;
	}

	if (!loadLevelAndActors())
		return false;

	refreshUiText({ 1024, 640 });

	return true;
}

bool GAME1_BombermanWindow::loadTexture(sf::Texture& texture, const std::string& path, const std::string& readableName)
{
	if (!texture.loadFromFile(path))
	{
		m_lastError = "Failed to load Bomberman " + readableName + " texture: " + path;
		return false;
	}

	return true;
}

bool GAME1_BombermanWindow::tryLoadPowerUpTexture(PowerUpTexture& target, const std::vector<std::string>& candidatePaths)
{
	for (const std::string& path : candidatePaths)
	{
		if (target.texture.loadFromFile(path))
		{
			target.loaded = true;
			return true;
		}
	}

	target.loaded = false;
	return false;
}

bool GAME1_BombermanWindow::loadLevelAndActors()
{
	namespace fs = std::filesystem;

	m_bombs.clear();
	m_explosions.clear();
	m_powerUps.clear();
	m_enemies.clear();

	const fs::path levelPath = fs::path(m_mapsDirectory) / "level01.txt";

	if (!m_level.loadFromFile(levelPath.string(), m_resourcesDirectory))
	{
		m_lastError = m_level.getLastError();
		return false;
	}

	m_player.reset(m_level.getPlayerSpawn(), m_level);

	m_playerLives = 3;
	m_bombRange = 2;
	m_maxActiveBombs = 1;

	m_fireUpLevel = 0;
	m_bombUpLevel = 0;
	m_speedUpLevel = 0;

	m_player.setMoveSpeed(m_basePlayerSpeed);

	const fs::path enemyTexturePath = fs::path(m_resourcesDirectory) / "Enemies" / "enemy_basic.png";

	const std::vector<BombermanGridPosition>& enemySpawns = m_level.getEnemySpawns();

	m_enemies.reserve(enemySpawns.size());

	for (const BombermanGridPosition& spawn : enemySpawns)
	{
		m_enemies.emplace_back();

		if (!m_enemies.back().load(enemyTexturePath.string(), spawn, m_level))
		{
			m_lastError = m_enemies.back().getLastError();
			m_enemies.pop_back();
			return false;
		}
	}

	m_playState = PlayState::Playing;
	m_spaceHeldLastFrame = false;
	m_restartHeldLastFrame = false;

	return true;
}

void GAME1_BombermanWindow::reset()
{
	m_lastError.clear();
	loadLevelAndActors();
}

void GAME1_BombermanWindow::update(float deltaTime, sf::Vector2u windowSize)
{
	const bool restartHeld = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::R);

	if (restartHeld && !m_restartHeldLastFrame)
	{
		reset();
	}

	m_restartHeldLastFrame = restartHeld;

	if (m_playState == PlayState::Playing)
	{
		const bool spaceHeld = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space);

		if (spaceHeld && !m_spaceHeldLastFrame)
		{
			placeBomb();
		}

		m_spaceHeldLastFrame = spaceHeld;

		m_player.update(
			deltaTime,
			m_level,
			[this](int col, int row)
			{
				return isTileBlockedForPlayer(col, row);
			});

		refreshBombPassThroughState();
		collectPowerUps();

		updateEnemies(deltaTime);
		updateBombs(deltaTime);
		updateExplosions(deltaTime);

		applyExplosionDamage();
		checkPlayerEnemyCollision();
		updateWinLoseState();
	}
	else
	{
		m_spaceHeldLastFrame = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space);
		updateExplosions(deltaTime);
	}

	refreshUiText(windowSize);
}

void GAME1_BombermanWindow::layout(const sf::RenderWindow& window)
{
	refreshUiText(window.getSize());
}

void GAME1_BombermanWindow::placeBomb()
{
	if (!m_player.isAlive())
		return;

	const int activeBombCount = static_cast<int>(std::count_if(
		m_bombs.begin(),
		m_bombs.end(),
		[](const BombermanBomb& bomb)
		{
			return !bomb.hasExploded();
		}));

	if (activeBombCount >= m_maxActiveBombs)
		return;

	const BombermanGridPosition bombPosition = m_player.getGridPosition(m_level);

	if (isBombAtTile(bombPosition))
		return;

	BombermanBomb bomb(bombPosition, 2.0f, m_bombRange);
	bomb.setPlayerCanPassThrough(true);

	m_bombs.push_back(bomb);
}

bool GAME1_BombermanWindow::isBombAtTile(BombermanGridPosition gridPosition) const
{
	for (const BombermanBomb& bomb : m_bombs)
	{
		if (!bomb.hasExploded() && bomb.getGridPosition() == gridPosition)
			return true;
	}

	return false;
}

bool GAME1_BombermanWindow::isTileBlockedForPlayer(int col, int row) const
{
	if (m_level.isBlockedForMovement(col, row))
		return true;

	for (const BombermanBomb& bomb : m_bombs)
	{
		if (bomb.hasExploded())
			continue;

		const BombermanGridPosition bombGrid = bomb.getGridPosition();

		if (bombGrid.col != col || bombGrid.row != row)
			continue;

		if (bomb.canPlayerPassThrough())
			continue;

		return true;
	}

	return false;
}

bool GAME1_BombermanWindow::isTileBlockedForEnemies(int col, int row) const
{
	if (m_level.isBlockedForMovement(col, row))
		return true;

	for (const BombermanBomb& bomb : m_bombs)
	{
		if (bomb.hasExploded())
			continue;

		const BombermanGridPosition bombGrid = bomb.getGridPosition();

		if (bombGrid.col == col && bombGrid.row == row)
			return true;
	}

	return false;
}

void GAME1_BombermanWindow::refreshBombPassThroughState()
{
	if (!m_player.isAlive())
		return;

	const sf::FloatRect playerCollisionBounds = m_player.getCollisionBounds();

	for (BombermanBomb& bomb : m_bombs)
	{
		if (bomb.hasExploded())
			continue;

		if (!bomb.canPlayerPassThrough())
			continue;

		const sf::FloatRect bombTileBounds = getTileBounds(bomb.getGridPosition());

		if (!rectsIntersect(playerCollisionBounds, bombTileBounds))
		{
			bomb.setPlayerCanPassThrough(false);
		}
	}
}

void GAME1_BombermanWindow::damagePlayer()
{
	if (!m_player.isAlive())
		return;

	if (m_player.isInvincible())
		return;

	m_player.kill();
	--m_playerLives;

	if (m_playerLives <= 0)
	{
		m_playerLives = 0;
		m_playState = PlayState::GameOver;
		return;
	}

	respawnPlayerImmediately();
}

void GAME1_BombermanWindow::respawnPlayerImmediately()
{
	m_bombs.clear();
	m_explosions.clear();

	m_player.reset(m_level.getPlayerSpawn(), m_level);
	m_player.beginInvincibility(3.0f);
}

void GAME1_BombermanWindow::updateBombs(float deltaTime)
{
	for (BombermanBomb& bomb : m_bombs)
	{
		bomb.update(deltaTime);

		if (bomb.shouldExplode())
		{
			explodeBomb(bomb);
			bomb.markExploded();
		}
	}

	m_bombs.erase(
		std::remove_if(
			m_bombs.begin(),
			m_bombs.end(),
			[](const BombermanBomb& bomb)
			{
				return bomb.hasExploded();
			}),
		m_bombs.end());
}

void GAME1_BombermanWindow::explodeBomb(BombermanBomb& bomb)
{
	ActiveExplosion explosion;
	explosion.timer = 0.38f;

	const BombermanGridPosition center = bomb.getGridPosition();

	addExplosionTile(explosion.tiles, center, BombermanExplosionTileType::Center);

	constexpr int directionCount = 4;

	const int directionCols[directionCount] = { 1, -1, 0, 0 };
	const int directionRows[directionCount] = { 0, 0, 1, -1 };

	for (int directionIndex = 0; directionIndex < directionCount; ++directionIndex)
	{
		for (int distance = 1; distance <= bomb.getExplosionRange(); ++distance)
		{
			const BombermanGridPosition current{
				center.col + directionCols[directionIndex] * distance,
				center.row + directionRows[directionIndex] * distance
			};

			if (!m_level.isInside(current.col, current.row))
				break;

			if (m_level.isWall(current.col, current.row))
				break;

			const BombermanExplosionTileType visualType =
				directionRows[directionIndex] == 0
				? BombermanExplosionTileType::Horizontal
				: BombermanExplosionTileType::Vertical;

			addExplosionTile(explosion.tiles, current, visualType);

			for (BombermanBomb& otherBomb : m_bombs)
			{
				if (!otherBomb.hasExploded() && otherBomb.getGridPosition() == current)
				{
					otherBomb.triggerNow();
				}
			}

			if (m_level.isBreakableBlock(current.col, current.row))
			{
				m_level.destroyBreakableBlock(current.col, current.row);
				maybeSpawnPowerUpAt(current);
				break;
			}
		}
	}

	m_explosions.push_back(std::move(explosion));
}

void GAME1_BombermanWindow::maybeSpawnPowerUpAt(BombermanGridPosition gridPosition)
{
	if (isPowerUpAtTile(gridPosition))
		return;

	std::uniform_real_distribution<float> dropDistribution(0.f, 1.f);

	if (dropDistribution(m_rng) > m_powerUpDropChance)
		return;

	std::uniform_int_distribution<int> typeDistribution(0, 2);

	ActivePowerUp powerUp;
	powerUp.gridPosition = gridPosition;
	powerUp.collected = false;

	const int typeValue = typeDistribution(m_rng);

	if (typeValue == 0)
		powerUp.type = PowerUpType::FireUp;
	else if (typeValue == 1)
		powerUp.type = PowerUpType::BombUp;
	else
		powerUp.type = PowerUpType::SpeedUp;

	m_powerUps.push_back(powerUp);
}

bool GAME1_BombermanWindow::isPowerUpAtTile(BombermanGridPosition gridPosition) const
{
	for (const ActivePowerUp& powerUp : m_powerUps)
	{
		if (!powerUp.collected && powerUp.gridPosition == gridPosition)
			return true;
	}

	return false;
}

void GAME1_BombermanWindow::collectPowerUps()
{
	if (!m_player.isAlive())
		return;

	const BombermanGridPosition playerGrid = m_player.getGridPosition(m_level);

	for (ActivePowerUp& powerUp : m_powerUps)
	{
		if (powerUp.collected)
			continue;

		if (powerUp.gridPosition == playerGrid)
		{
			powerUp.collected = true;
			applyPowerUp(powerUp.type);
		}
	}

	m_powerUps.erase(
		std::remove_if(
			m_powerUps.begin(),
			m_powerUps.end(),
			[](const ActivePowerUp& powerUp)
			{
				return powerUp.collected;
			}),
		m_powerUps.end());
}

void GAME1_BombermanWindow::applyPowerUp(PowerUpType type)
{
	switch (type)
	{
	case PowerUpType::FireUp:
		++m_fireUpLevel;
		m_bombRange = std::min(8, m_bombRange + 1);
		break;

	case PowerUpType::BombUp:
		++m_bombUpLevel;
		m_maxActiveBombs = std::min(8, m_maxActiveBombs + 1);
		break;

	case PowerUpType::SpeedUp:
		++m_speedUpLevel;
		m_player.setMoveSpeed(std::min(m_maxPlayerSpeed, m_player.getMoveSpeed() + m_speedUpAmount));
		break;
	}
}

void GAME1_BombermanWindow::addExplosionTile(std::vector<BombermanExplosionTile>& tiles,
	BombermanGridPosition position,
	BombermanExplosionTileType type)
{
	const auto alreadyExists = std::any_of(
		tiles.begin(),
		tiles.end(),
		[position](const BombermanExplosionTile& tile)
		{
			return tile.gridPosition == position;
		});

	if (alreadyExists)
		return;

	tiles.push_back({ position, type });
}

void GAME1_BombermanWindow::updateExplosions(float deltaTime)
{
	for (ActiveExplosion& explosion : m_explosions)
	{
		explosion.timer -= deltaTime;
	}

	m_explosions.erase(
		std::remove_if(
			m_explosions.begin(),
			m_explosions.end(),
			[](const ActiveExplosion& explosion)
			{
				return explosion.timer <= 0.f;
			}),
		m_explosions.end());
}

void GAME1_BombermanWindow::applyExplosionDamage()
{
	if (m_player.isAlive() && !m_player.isInvincible())
	{
		const BombermanGridPosition playerGrid = m_player.getGridPosition(m_level);

		if (isGridPositionCurrentlyExploding(playerGrid))
		{
			damagePlayer();
		}
	}

	for (BombermanEnemy& enemy : m_enemies)
	{
		if (!enemy.isAlive())
			continue;

		const BombermanGridPosition enemyGrid = enemy.getGridPosition(m_level);

		if (isGridPositionCurrentlyExploding(enemyGrid))
		{
			enemy.kill();
		}
	}
}

bool GAME1_BombermanWindow::isGridPositionCurrentlyExploding(BombermanGridPosition gridPosition) const
{
	for (const ActiveExplosion& explosion : m_explosions)
	{
		for (const BombermanExplosionTile& tile : explosion.tiles)
		{
			if (tile.gridPosition == gridPosition)
				return true;
		}
	}

	return false;
}

void GAME1_BombermanWindow::updateEnemies(float deltaTime)
{
	for (BombermanEnemy& enemy : m_enemies)
	{
		enemy.update(
			deltaTime,
			[this](int col, int row)
			{
				return isTileBlockedForEnemies(col, row);
			});
	}
}

void GAME1_BombermanWindow::checkPlayerEnemyCollision()
{
	if (!m_player.isAlive() || m_player.isInvincible())
		return;

	for (const BombermanEnemy& enemy : m_enemies)
	{
		if (!enemy.isAlive())
			continue;

		if (rectsIntersect(m_player.getCollisionBounds(), enemy.getBounds()))
		{
			damagePlayer();
			return;
		}
	}
}

void GAME1_BombermanWindow::updateWinLoseState()
{
	if (m_playState == PlayState::GameOver)
		return;

	if (!areAllEnemiesDefeated())
		return;

	if (m_level.hasExit())
	{
		if (isPlayerStandingOnExit())
		{
			m_playState = PlayState::Victory;
		}

		return;
	}

	m_playState = PlayState::Victory;
}

bool GAME1_BombermanWindow::areAllEnemiesDefeated() const
{
	return std::none_of(
		m_enemies.begin(),
		m_enemies.end(),
		[](const BombermanEnemy& enemy)
		{
			return enemy.isAlive();
		});
}

bool GAME1_BombermanWindow::isPlayerStandingOnExit() const
{
	if (!m_player.isAlive())
		return false;

	if (!m_level.hasExit())
		return false;

	return m_player.getGridPosition(m_level) == m_level.getExitPosition();
}

void GAME1_BombermanWindow::drawTextureInTile(sf::RenderTarget& target,
	const sf::Texture& texture,
	BombermanGridPosition gridPosition) const
{
	sf::Sprite sprite(texture);

	const sf::FloatRect localBounds = sprite.getLocalBounds();

	if (localBounds.size.x <= 0.f || localBounds.size.y <= 0.f)
		return;

	sprite.setScale({
		static_cast<float>(BombermanLevel::TileSize) / localBounds.size.x,
		static_cast<float>(BombermanLevel::TileSize) / localBounds.size.y
		});

	sprite.setPosition({
		static_cast<float>(gridPosition.col * BombermanLevel::TileSize),
		static_cast<float>(gridPosition.row * BombermanLevel::TileSize)
		});

	target.draw(sprite);
}

void GAME1_BombermanWindow::drawPowerUpInTile(sf::RenderTarget& target, const ActivePowerUp& powerUp) const
{
	if (powerUp.collected)
		return;

	const PowerUpTexture& powerUpTexture = getPowerUpTexture(powerUp.type);

	if (powerUpTexture.loaded)
	{
		drawTextureInTile(target, powerUpTexture.texture, powerUp.gridPosition);
		return;
	}

	const sf::FloatRect tileBounds = getTileBounds(powerUp.gridPosition);

	sf::CircleShape fallbackCircle;
	fallbackCircle.setRadius(15.f);
	fallbackCircle.setPointCount(24);
	fallbackCircle.setFillColor(getFallbackPowerUpColor(powerUp.type));
	fallbackCircle.setOutlineColor(sf::Color::White);
	fallbackCircle.setOutlineThickness(2.f);
	fallbackCircle.setPosition({
		tileBounds.position.x + tileBounds.size.x * 0.5f - 15.f,
		tileBounds.position.y + tileBounds.size.y * 0.5f - 15.f
		});

	target.draw(fallbackCircle);

	sf::Text label(m_font);
	label.setString(getPowerUpShortName(powerUp.type));
	label.setCharacterSize(15);
	label.setFillColor(sf::Color::Black);
	label.setStyle(sf::Text::Bold);

	const sf::FloatRect bounds = label.getLocalBounds();
	label.setPosition({
		tileBounds.position.x + (tileBounds.size.x - bounds.size.x) * 0.5f - bounds.position.x,
		tileBounds.position.y + (tileBounds.size.y - bounds.size.y) * 0.5f - bounds.position.y - 2.f
		});

	target.draw(label);
}

void GAME1_BombermanWindow::drawPowerUpIcon(sf::RenderTarget& target, PowerUpType type, sf::Vector2f position, float size) const
{
	const PowerUpTexture& powerUpTexture = getPowerUpTexture(type);

	if (powerUpTexture.loaded)
	{
		sf::Sprite sprite(powerUpTexture.texture);

		const sf::FloatRect localBounds = sprite.getLocalBounds();

		if (localBounds.size.x > 0.f && localBounds.size.y > 0.f)
		{
			sprite.setScale({
				size / localBounds.size.x,
				size / localBounds.size.y
				});
			sprite.setPosition(position);
			target.draw(sprite);
			return;
		}
	}

	sf::CircleShape fallbackCircle;
	fallbackCircle.setRadius(size * 0.5f);
	fallbackCircle.setPointCount(24);
	fallbackCircle.setFillColor(getFallbackPowerUpColor(type));
	fallbackCircle.setOutlineColor(sf::Color::White);
	fallbackCircle.setOutlineThickness(1.5f);
	fallbackCircle.setPosition(position);

	target.draw(fallbackCircle);
}

const GAME1_BombermanWindow::PowerUpTexture& GAME1_BombermanWindow::getPowerUpTexture(PowerUpType type) const
{
	switch (type)
	{
	case PowerUpType::FireUp:
		return m_fireUpTexture;

	case PowerUpType::BombUp:
		return m_bombUpTexture;

	case PowerUpType::SpeedUp:
	default:
		return m_speedUpTexture;
	}
}

sf::Color GAME1_BombermanWindow::getFallbackPowerUpColor(PowerUpType type) const
{
	switch (type)
	{
	case PowerUpType::FireUp:
		return sf::Color(255, 100, 50);

	case PowerUpType::BombUp:
		return sf::Color(90, 170, 255);

	case PowerUpType::SpeedUp:
	default:
		return sf::Color(120, 255, 120);
	}
}

std::string GAME1_BombermanWindow::getPowerUpShortName(PowerUpType type) const
{
	switch (type)
	{
	case PowerUpType::FireUp:
		return "F";

	case PowerUpType::BombUp:
		return "B";

	case PowerUpType::SpeedUp:
	default:
		return "S";
	}
}

void GAME1_BombermanWindow::draw(sf::RenderWindow& window) const
{
	const sf::Vector2u windowSize = window.getSize();

	const sf::View previousView = window.getView();

	sf::View screenView(
		sf::FloatRect(
			{ 0.f, 0.f },
			{ static_cast<float>(windowSize.x), static_cast<float>(windowSize.y) }
		)
	);

	window.setView(screenView);

	sf::RectangleShape background;
	background.setPosition({ 0.f, 0.f });
	background.setSize({
		static_cast<float>(windowSize.x),
		static_cast<float>(windowSize.y)
		});
	background.setFillColor(sf::Color(28, 28, 36));
	window.draw(background);

	sf::View worldView;
	worldView.setSize({
		static_cast<float>(windowSize.x),
		static_cast<float>(windowSize.y)
		});
	worldView.setCenter({
		m_level.getPixelWidth() * 0.5f,
		m_level.getPixelHeight() * 0.5f
		});

	window.setView(worldView);

	m_level.drawFloorLayer(window);

	const BombermanGridPosition playerGrid = m_player.getGridPosition(m_level);

	for (int row = 0; row < m_level.getHeightInTiles(); ++row)
	{
		for (int col = 0; col < m_level.getWidthInTiles(); ++col)
		{
			m_level.drawWorldTileAt(window, col, row);
		}

		for (const ActivePowerUp& powerUp : m_powerUps)
		{
			if (!powerUp.collected && powerUp.gridPosition.row == row)
			{
				drawPowerUpInTile(window, powerUp);
			}
		}

		for (const BombermanBomb& bomb : m_bombs)
		{
			if (bomb.getGridPosition().row == row)
			{
				drawTextureInTile(window, m_bombTexture, bomb.getGridPosition());
			}
		}

		for (const BombermanEnemy& enemy : m_enemies)
		{
			if (!enemy.isAlive())
				continue;

			if (enemy.getGridPosition(m_level).row == row)
			{
				enemy.draw(window);
			}
		}

		if (m_player.isAlive() && playerGrid.row == row)
		{
			m_player.draw(window);
		}

		for (const ActiveExplosion& explosion : m_explosions)
		{
			for (const BombermanExplosionTile& tile : explosion.tiles)
			{
				if (tile.gridPosition.row != row)
					continue;

				switch (tile.type)
				{
				case BombermanExplosionTileType::Center:
					drawTextureInTile(window, m_explosionCenterTexture, tile.gridPosition);
					break;

				case BombermanExplosionTileType::Horizontal:
					drawTextureInTile(window, m_explosionHorizontalTexture, tile.gridPosition);
					break;

				case BombermanExplosionTileType::Vertical:
					drawTextureInTile(window, m_explosionVerticalTexture, tile.gridPosition);
					break;
				}
			}
		}
	}

	window.setView(screenView);

	if (m_statsText)
		window.draw(*m_statsText);

	if (m_objectiveText)
		window.draw(*m_objectiveText);

	drawPowerUpIcon(window, PowerUpType::FireUp, { 18.f, 76.f }, 26.f);
	drawPowerUpIcon(window, PowerUpType::BombUp, { 118.f, 76.f }, 26.f);
	drawPowerUpIcon(window, PowerUpType::SpeedUp, { 218.f, 76.f }, 26.f);

	if (m_powerUpHudText)
		window.draw(*m_powerUpHudText);

	if (m_helpText)
		window.draw(*m_helpText);

	if (m_playState != PlayState::Playing && m_statusText)
	{
		sf::RectangleShape overlay;
		overlay.setPosition({ 0.f, 0.f });
		overlay.setSize({
			static_cast<float>(windowSize.x),
			static_cast<float>(windowSize.y)
			});
		overlay.setFillColor(sf::Color(0, 0, 0, 145));
		window.draw(overlay);

		window.draw(*m_statusText);
	}

	window.setView(previousView);
}

void GAME1_BombermanWindow::refreshUiText(sf::Vector2u windowSize)
{
	const int livingEnemies = static_cast<int>(std::count_if(
		m_enemies.begin(),
		m_enemies.end(),
		[](const BombermanEnemy& enemy)
		{
			return enemy.isAlive();
		}));

	if (m_statsText)
	{
		m_statsText->setString(
			"Lives: " + std::to_string(std::max(0, m_playerLives)) +
			"   Bombs: " + std::to_string(m_maxActiveBombs) +
			"   Range: " + std::to_string(m_bombRange) +
			"   Speed: " + std::to_string(static_cast<int>(m_player.getMoveSpeed())) +
			"   Enemies: " + std::to_string(livingEnemies)
		);

		m_statsText->setPosition({ 18.f, 14.f });
	}

	if (m_objectiveText)
	{
		if (!m_level.hasExit())
		{
			m_objectiveText->setString("Objective: Defeat all enemies");
			m_objectiveText->setFillColor(sf::Color(255, 230, 120));
		}
		else if (livingEnemies > 0)
		{
			m_objectiveText->setString("Objective: Defeat all enemies to unlock the exit");
			m_objectiveText->setFillColor(sf::Color(255, 230, 120));
		}
		else
		{
			m_objectiveText->setString("Objective: Exit unlocked! Step on the exit tile");
			m_objectiveText->setFillColor(sf::Color(120, 255, 140));
		}

		m_objectiveText->setPosition({ 18.f, 44.f });
	}

	if (m_powerUpHudText)
	{
		m_powerUpHudText->setString(
			"x" + std::to_string(m_fireUpLevel) +
			"        x" + std::to_string(m_bombUpLevel) +
			"        x" + std::to_string(m_speedUpLevel)
		);

		m_powerUpHudText->setPosition({ 50.f, 78.f });
	}

	if (m_helpText)
	{
		m_helpText->setString("WASD / Arrows: Move    Space: Place Bomb    R: Restart    ESC: Hub");

		const sf::FloatRect bounds = m_helpText->getLocalBounds();

		m_helpText->setPosition({
			(static_cast<float>(windowSize.x) - bounds.size.x) * 0.5f - bounds.position.x,
			static_cast<float>(windowSize.y) - 42.f - bounds.position.y
			});
	}

	if (m_statusText)
	{
		if (m_playState == PlayState::Victory)
		{
			m_statusText->setString("VICTORY!\nYou escaped the arena\nPress R to restart");
		}
		else if (m_playState == PlayState::GameOver)
		{
			m_statusText->setString("GAME OVER\nPress R to restart");
		}
		else
		{
			m_statusText->setString("");
		}

		const sf::FloatRect bounds = m_statusText->getLocalBounds();

		m_statusText->setPosition({
			(static_cast<float>(windowSize.x) - bounds.size.x) * 0.5f - bounds.position.x,
			(static_cast<float>(windowSize.y) - bounds.size.y) * 0.5f - bounds.position.y
			});
	}
}

sf::FloatRect GAME1_BombermanWindow::getTileBounds(BombermanGridPosition gridPosition) const
{
	return sf::FloatRect(
		{
			static_cast<float>(gridPosition.col * BombermanLevel::TileSize),
			static_cast<float>(gridPosition.row * BombermanLevel::TileSize)
		},
		{
			static_cast<float>(BombermanLevel::TileSize),
			static_cast<float>(BombermanLevel::TileSize)
		}
	);
}

bool GAME1_BombermanWindow::rectsIntersect(const sf::FloatRect& a, const sf::FloatRect& b) const
{
	return a.position.x < b.position.x + b.size.x &&
		a.position.x + a.size.x > b.position.x &&
		a.position.y < b.position.y + b.size.y &&
		a.position.y + a.size.y > b.position.y;
}

const std::string& GAME1_BombermanWindow::getLastError() const
{
	return m_lastError;
}