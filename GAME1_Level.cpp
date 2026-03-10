#include "GAME1_Level.h"

#include <algorithm>
#include <fstream>
#include <random>
#include <utility>

namespace
{
	// Simple AABB overlap check used when spawning random break blocks.
	bool RectsIntersect(const sf::FloatRect& a, const sf::FloatRect& b)
	{
		return a.position.x < (b.position.x + b.size.x) &&
			(a.position.x + a.size.x) > b.position.x &&
			a.position.y < (b.position.y + b.size.y) &&
			(a.position.y + a.size.y) > b.position.y;
	}
}

bool GAME1_Level::loadFromFile(const std::string& mapPath,
	const std::string& floorTexturePath,
	const std::string& breakTexturePath)
{
	m_rows.clear();
	m_lastError.clear();

	// Load both textures before we even open the map,
	// so the level only succeeds if it is fully drawable.
	if (!m_floorTexture.loadFromFile(floorTexturePath))
	{
		m_lastError = "Failed to load floor texture: " + floorTexturePath;
		return false;
	}

	if (!m_breakTexture.loadFromFile(breakTexturePath))
	{
		m_lastError = "Failed to load break block texture: " + breakTexturePath;
		return false;
	}

	std::ifstream file(mapPath);
	if (!file.is_open())
	{
		m_lastError = "Failed to open map file: " + mapPath;
		return false;
	}

	std::string line;
	std::size_t expectedWidth = 0;

	// Each non-empty line becomes one row in the map.
	while (std::getline(file, line))
	{
		if (line.empty())
			continue;

		// The first row sets the required map width.
		if (expectedWidth == 0)
		{
			expectedWidth = line.size();
		}
		else if (line.size() != expectedWidth)
		{
			m_lastError = "Map error: all rows must have the same width.";
			return false;
		}

		// Only the three known tile letters are allowed.
		for (char c : line)
		{
			if (c != 'X' && c != 'O' && c != 'B')
			{
				m_lastError = "Map error: only X, O and B are allowed.";
				return false;
			}
		}

		m_rows.push_back(line);
	}

	if (m_rows.empty())
	{
		m_lastError = "Map error: map file is empty.";
		return false;
	}

	return true;
}

void GAME1_Level::draw(sf::RenderWindow& window) const
{
	// Walk the 2D text map and draw a sprite for each visible tile.
	for (int row = 0; row < static_cast<int>(m_rows.size()); ++row)
	{
		for (int col = 0; col < static_cast<int>(m_rows[row].size()); ++col)
		{
			const char tile = m_rows[row][col];

			const sf::Texture* texture = nullptr;

			if (tile == 'X')
				texture = &m_floorTexture;
			else if (tile == 'B')
				texture = &m_breakTexture;
			else
				continue;

			sf::Sprite sprite(*texture);

			const sf::FloatRect localBounds = sprite.getLocalBounds();
			if (localBounds.size.x <= 0.f || localBounds.size.y <= 0.f)
				continue;

			// Scale the source image so every tile is exactly 64x64 in world space.
			sprite.setScale({
				static_cast<float>(TileSize) / localBounds.size.x,
				static_cast<float>(TileSize) / localBounds.size.y
				});

			sprite.setPosition({
				static_cast<float>(col * TileSize),
				static_cast<float>(row * TileSize)
				});

			window.draw(sprite);
		}
	}
}

bool GAME1_Level::isSolidTile(int col, int row) const
{
	if (!isInside(col, row))
		return false;

	const char tile = m_rows[row][col];
	return tile == 'X' || tile == 'B';
}

bool GAME1_Level::isBreakTile(int col, int row) const
{
	if (!isInside(col, row))
		return false;

	return m_rows[row][col] == 'B';
}

char GAME1_Level::getTile(int col, int row) const
{
	if (!isInside(col, row))
		return 'O';

	return m_rows[row][col];
}

void GAME1_Level::breakTile(int col, int row)
{
	if (!isInside(col, row))
		return;

	if (m_rows[row][col] == 'B')
	{
		m_rows[row][col] = 'O';
	}
}

void GAME1_Level::spawnRandomBreakBlocks(int count, const sf::FloatRect& forbiddenArea)
{
	std::vector<std::pair<int, int>> candidates;

	// Any empty tile that does not overlap the forbidden area becomes a candidate.
	for (int row = 0; row < static_cast<int>(m_rows.size()); ++row)
	{
		for (int col = 0; col < static_cast<int>(m_rows[row].size()); ++col)
		{
			if (m_rows[row][col] != 'O')
				continue;

			const sf::FloatRect tileRect(
				{ static_cast<float>(col * TileSize), static_cast<float>(row * TileSize) },
				{ static_cast<float>(TileSize), static_cast<float>(TileSize) }
			);

			if (!RectsIntersect(tileRect, forbiddenArea))
			{
				candidates.emplace_back(col, row);
			}
		}
	}

	// Shuffle the possible locations so the result feels random each run.
	std::random_device rd;
	std::mt19937 rng(rd());
	std::shuffle(candidates.begin(), candidates.end(), rng);

	const int blocksToPlace = std::min(count, static_cast<int>(candidates.size()));

	for (int i = 0; i < blocksToPlace; ++i)
	{
		const int col = candidates[i].first;
		const int row = candidates[i].second;
		m_rows[row][col] = 'B';
	}
}

const std::string& GAME1_Level::getLastError() const
{
	return m_lastError;
}

bool GAME1_Level::isInside(int col, int row) const
{
	if (row < 0 || row >= static_cast<int>(m_rows.size()))
		return false;

	if (col < 0 || col >= static_cast<int>(m_rows[row].size()))
		return false;

	return true;
}