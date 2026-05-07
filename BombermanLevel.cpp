#include "BombermanLevel.h"

#include <algorithm>
#include <filesystem>
#include <fstream>

bool BombermanLevel::loadFromFile(const std::string& mapPath, const std::string& resourcesDirectory)
{
	namespace fs = std::filesystem;

	m_lastError.clear();
	m_rows.clear();
	m_enemySpawns.clear();
	m_playerSpawn = { 1, 1 };
	m_hasExit = false;
	m_exitPosition = { 0, 0 };
	m_hasExitTexture = false;

	const fs::path resourcesPath(resourcesDirectory);
	const fs::path tilesPath = resourcesPath / "Tiles";

	if (!loadTexture(m_floorTexture, (tilesPath / "floor.png").string(), "floor"))
		return false;

	if (!loadTexture(m_wallTexture, (tilesPath / "solid_wall.png").string(), "solid wall"))
		return false;

	if (!loadTexture(m_breakableTexture, (tilesPath / "breakable_block.png").string(), "breakable block"))
		return false;

	m_hasExitTexture = m_exitTexture.loadFromFile((tilesPath / "exit.png").string());

	std::ifstream file(mapPath);
	if (!file.is_open())
	{
		m_lastError = "Failed to open Bomberman map file: " + mapPath;
		return false;
	}

	std::vector<std::string> rawLines;
	std::string line;
	std::size_t widestLine = 0;

	while (std::getline(file, line))
	{
		if (!line.empty() && line.back() == '\r')
			line.pop_back();

		if (line.empty())
			continue;

		widestLine = std::max(widestLine, line.size());
		rawLines.push_back(line);
	}

	if (rawLines.empty())
	{
		m_lastError = "Bomberman map file is empty: " + mapPath;
		return false;
	}

	bool foundPlayerSpawn = false;

	for (std::size_t row = 0; row < rawLines.size(); ++row)
	{
		std::string paddedLine = rawLines[row];
		paddedLine.resize(widestLine, ' ');

		for (std::size_t col = 0; col < paddedLine.size(); ++col)
		{
			const char tile = paddedLine[col];

			switch (tile)
			{
			case 'X':
			case 'B':
			case ' ':
				break;

			case 'P':
				m_playerSpawn = {
					static_cast<int>(col),
					static_cast<int>(row)
				};
				foundPlayerSpawn = true;
				paddedLine[col] = ' ';
				break;

			case 'O':
				m_enemySpawns.push_back({
					static_cast<int>(col),
					static_cast<int>(row)
					});
				paddedLine[col] = ' ';
				break;

			case 'E':
				m_hasExit = true;
				m_exitPosition = {
					static_cast<int>(col),
					static_cast<int>(row)
				};
				break;

			default:
				m_lastError =
					"Bomberman map error: unsupported character '" +
					std::string(1, tile) +
					"'. Allowed: X, B, P, O, E, and space.";
				return false;
			}
		}

		m_rows.push_back(paddedLine);
	}

	if (!foundPlayerSpawn)
	{
		m_lastError = "Bomberman map error: no player spawn 'P' found.";
		return false;
	}

	return true;
}

bool BombermanLevel::loadTexture(sf::Texture& texture, const std::string& path, const std::string& readableName)
{
	if (!texture.loadFromFile(path))
	{
		m_lastError = "Failed to load Bomberman " + readableName + " texture: " + path;
		return false;
	}

	return true;
}

void BombermanLevel::drawTextureInTile(sf::RenderTarget& target, const sf::Texture& texture, int col, int row) const
{
	sf::Sprite sprite(texture);

	const sf::FloatRect localBounds = sprite.getLocalBounds();

	if (localBounds.size.x <= 0.f || localBounds.size.y <= 0.f)
		return;

	sprite.setScale({
		static_cast<float>(TileSize) / localBounds.size.x,
		static_cast<float>(TileSize) / localBounds.size.y
		});

	sprite.setPosition({
		static_cast<float>(col * TileSize),
		static_cast<float>(row * TileSize)
		});

	target.draw(sprite);
}

void BombermanLevel::draw(sf::RenderTarget& target) const
{
	drawBaseLayer(target, true);
}

void BombermanLevel::drawBaseLayer(sf::RenderTarget& target, bool includeSolidWalls) const
{
	for (int row = 0; row < getHeightInTiles(); ++row)
	{
		for (int col = 0; col < getWidthInTiles(); ++col)
		{
			drawTextureInTile(target, m_floorTexture, col, row);

			const char tile = m_rows[row][col];

			if (tile == 'X')
			{
				if (includeSolidWalls)
					drawTextureInTile(target, m_wallTexture, col, row);
			}
			else if (tile == 'B')
			{
				drawTextureInTile(target, m_breakableTexture, col, row);
			}
			else if (tile == 'E')
			{
				if (m_hasExitTexture)
				{
					drawTextureInTile(target, m_exitTexture, col, row);
				}
				else
				{
					sf::RectangleShape exitTile;
					exitTile.setPosition({
						static_cast<float>(col * TileSize),
						static_cast<float>(row * TileSize)
						});
					exitTile.setSize({
						static_cast<float>(TileSize),
						static_cast<float>(TileSize)
						});
					exitTile.setFillColor(sf::Color(60, 190, 90, 180));
					exitTile.setOutlineColor(sf::Color::White);
					exitTile.setOutlineThickness(-2.f);
					target.draw(exitTile);
				}
			}
		}
	}
}

void BombermanLevel::drawSolidWallsOnly(sf::RenderTarget& target) const
{
	for (int row = 0; row < getHeightInTiles(); ++row)
	{
		for (int col = 0; col < getWidthInTiles(); ++col)
		{
			if (m_rows[row][col] == 'X')
			{
				drawTextureInTile(target, m_wallTexture, col, row);
			}
		}
	}
}

void BombermanLevel::drawSolidWallAt(sf::RenderTarget& target, int col, int row) const
{
	if (!isWall(col, row))
		return;

	drawTextureInTile(target, m_wallTexture, col, row);
}

bool BombermanLevel::isInside(int col, int row) const
{
	if (row < 0 || row >= static_cast<int>(m_rows.size()))
		return false;

	if (col < 0 || col >= static_cast<int>(m_rows[row].size()))
		return false;

	return true;
}

bool BombermanLevel::isWall(int col, int row) const
{
	if (!isInside(col, row))
		return true;

	return m_rows[row][col] == 'X';
}

bool BombermanLevel::isBreakableBlock(int col, int row) const
{
	if (!isInside(col, row))
		return false;

	return m_rows[row][col] == 'B';
}

bool BombermanLevel::isBlockedForMovement(int col, int row) const
{
	if (!isInside(col, row))
		return true;

	const char tile = m_rows[row][col];
	return tile == 'X' || tile == 'B';
}

bool BombermanLevel::canExplosionPassThrough(int col, int row) const
{
	if (!isInside(col, row))
		return false;

	return m_rows[row][col] != 'X';
}

void BombermanLevel::destroyBreakableBlock(int col, int row)
{
	if (!isInside(col, row))
		return;

	if (m_rows[row][col] == 'B')
	{
		m_rows[row][col] = ' ';
	}
}

BombermanGridPosition BombermanLevel::getPlayerSpawn() const
{
	return m_playerSpawn;
}

const std::vector<BombermanGridPosition>& BombermanLevel::getEnemySpawns() const
{
	return m_enemySpawns;
}

bool BombermanLevel::hasExit() const
{
	return m_hasExit;
}

BombermanGridPosition BombermanLevel::getExitPosition() const
{
	return m_exitPosition;
}

int BombermanLevel::getWidthInTiles() const
{
	if (m_rows.empty())
		return 0;

	return static_cast<int>(m_rows[0].size());
}

int BombermanLevel::getHeightInTiles() const
{
	return static_cast<int>(m_rows.size());
}

float BombermanLevel::getPixelWidth() const
{
	return static_cast<float>(getWidthInTiles() * TileSize);
}

float BombermanLevel::getPixelHeight() const
{
	return static_cast<float>(getHeightInTiles() * TileSize);
}

sf::Vector2f BombermanLevel::gridToWorldTopLeft(BombermanGridPosition gridPosition) const
{
	return {
		static_cast<float>(gridPosition.col * TileSize),
		static_cast<float>(gridPosition.row * TileSize)
	};
}

sf::Vector2f BombermanLevel::gridToWorldCenter(BombermanGridPosition gridPosition) const
{
	return {
		static_cast<float>(gridPosition.col * TileSize) + static_cast<float>(TileSize) * 0.5f,
		static_cast<float>(gridPosition.row * TileSize) + static_cast<float>(TileSize) * 0.5f
	};
}

BombermanGridPosition BombermanLevel::worldToGrid(sf::Vector2f worldPosition) const
{
	return {
		static_cast<int>(worldPosition.x / static_cast<float>(TileSize)),
		static_cast<int>(worldPosition.y / static_cast<float>(TileSize))
	};
}

const std::string& BombermanLevel::getLastError() const
{
	return m_lastError;
}