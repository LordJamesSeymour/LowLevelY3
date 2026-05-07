#pragma once

#include "BombermanTypes.h"

#include <SFML/Graphics.hpp>
#include <optional>
#include <string>
#include <vector>

class BombermanLevel
{
public:
	static constexpr int TileSize = 48;

public:
	bool loadFromFile(const std::string& mapPath, const std::string& resourcesDirectory);

	void draw(sf::RenderTarget& target) const;

	void drawBaseLayer(sf::RenderTarget& target, bool includeSolidWalls) const;
	void drawSolidWallsOnly(sf::RenderTarget& target) const;
	void drawSolidWallAt(sf::RenderTarget& target, int col, int row) const;

	bool isInside(int col, int row) const;
	bool isWall(int col, int row) const;
	bool isBreakableBlock(int col, int row) const;
	bool isBlockedForMovement(int col, int row) const;
	bool canExplosionPassThrough(int col, int row) const;

	void destroyBreakableBlock(int col, int row);

	BombermanGridPosition getPlayerSpawn() const;
	const std::vector<BombermanGridPosition>& getEnemySpawns() const;

	bool hasExit() const;
	BombermanGridPosition getExitPosition() const;

	int getWidthInTiles() const;
	int getHeightInTiles() const;

	float getPixelWidth() const;
	float getPixelHeight() const;

	sf::Vector2f gridToWorldTopLeft(BombermanGridPosition gridPosition) const;
	sf::Vector2f gridToWorldCenter(BombermanGridPosition gridPosition) const;
	BombermanGridPosition worldToGrid(sf::Vector2f worldPosition) const;

	const std::string& getLastError() const;

private:
	bool loadTexture(sf::Texture& texture, const std::string& path, const std::string& readableName);
	void drawTextureInTile(sf::RenderTarget& target, const sf::Texture& texture, int col, int row) const;

private:
	std::vector<std::string> m_rows;

	sf::Texture m_floorTexture;
	sf::Texture m_wallTexture;
	sf::Texture m_breakableTexture;
	sf::Texture m_exitTexture;

	bool m_hasExitTexture = false;

	BombermanGridPosition m_playerSpawn{ 1, 1 };
	std::vector<BombermanGridPosition> m_enemySpawns;

	bool m_hasExit = false;
	BombermanGridPosition m_exitPosition{ 0, 0 };

	std::string m_lastError;
};