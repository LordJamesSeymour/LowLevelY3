#pragma once

#include "BombermanBomb.h"
#include "BombermanEnemy.h"
#include "BombermanLevel.h"
#include "BombermanPlayer.h"
#include "BombermanTypes.h"

#include <SFML/Graphics.hpp>
#include <optional>
#include <string>
#include <vector>

class GAME1_BombermanWindow
{
public:
	bool load(const std::string& fontPath, const std::string& bombermanRootDirectory);

	void reset();

	void update(float deltaTime, sf::Vector2u windowSize);
	void layout(const sf::RenderWindow& window);
	void draw(sf::RenderWindow& window) const;

	const std::string& getLastError() const;

private:
	enum class PlayState
	{
		Playing,
		GameOver,
		Victory
	};

	struct ActiveExplosion
	{
		std::vector<BombermanExplosionTile> tiles;
		float timer = 0.35f;
	};

private:
	bool loadTexture(sf::Texture& texture, const std::string& path, const std::string& readableName);
	bool loadLevelAndActors();

	void placeBomb();
	bool isBombAtTile(BombermanGridPosition gridPosition) const;

	bool isTileBlockedForPlayer(int col, int row) const;
	bool isTileBlockedForEnemies(int col, int row) const;

	void refreshBombPassThroughState();

	void updateBombs(float deltaTime);
	void explodeBomb(BombermanBomb& bomb);
	void addExplosionTile(std::vector<BombermanExplosionTile>& tiles,
		BombermanGridPosition position,
		BombermanExplosionTileType type);

	void updateExplosions(float deltaTime);
	void applyExplosionDamage();
	bool isGridPositionCurrentlyExploding(BombermanGridPosition gridPosition) const;

	void updateEnemies(float deltaTime);
	void checkPlayerEnemyCollision();

	void updateWinLoseState();

	void drawTextureInTile(sf::RenderTarget& target,
		const sf::Texture& texture,
		BombermanGridPosition gridPosition) const;

	void refreshUiText(sf::Vector2u windowSize);

	sf::FloatRect getTileBounds(BombermanGridPosition gridPosition) const;
	bool rectsIntersect(const sf::FloatRect& a, const sf::FloatRect& b) const;

private:
	std::string m_bombermanRootDirectory;
	std::string m_resourcesDirectory;
	std::string m_mapsDirectory;

	BombermanLevel m_level;
	BombermanPlayer m_player;
	std::vector<BombermanEnemy> m_enemies;

	std::vector<BombermanBomb> m_bombs;
	std::vector<ActiveExplosion> m_explosions;

	sf::Texture m_bombTexture;
	sf::Texture m_explosionCenterTexture;
	sf::Texture m_explosionHorizontalTexture;
	sf::Texture m_explosionVerticalTexture;

	sf::Font m_font;

	std::optional<sf::Text> m_statusText;
	std::optional<sf::Text> m_helpText;
	std::optional<sf::Text> m_statsText;

	PlayState m_playState = PlayState::Playing;

	int m_playerLives = 3;
	int m_bombRange = 2;
	int m_maxActiveBombs = 1;

	bool m_spaceHeldLastFrame = false;
	bool m_restartHeldLastFrame = false;

	std::string m_lastError;
};