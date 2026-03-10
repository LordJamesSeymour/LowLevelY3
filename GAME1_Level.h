#pragma once

#include <SFML/Graphics.hpp>
#include <string>
#include <vector>

// This class owns the tile map data for Game 1.
// It can load the map from a text file, answer collision queries,
// draw tiles, and break breakable blocks.
class GAME1_Level
{
public:
	static constexpr int TileSize = 64;

	bool loadFromFile(const std::string& mapPath,
		const std::string& floorTexturePath,
		const std::string& breakTexturePath);

	void draw(sf::RenderWindow& window) const;

	bool isSolidTile(int col, int row) const;
	bool isBreakTile(int col, int row) const;
	char getTile(int col, int row) const;
	void breakTile(int col, int row);

	void spawnRandomBreakBlocks(int count, const sf::FloatRect& forbiddenArea);

	const std::string& getLastError() const;

private:
	bool isInside(int col, int row) const;

private:
	std::vector<std::string> m_rows;
	sf::Texture m_floorTexture;
	sf::Texture m_breakTexture;
	std::string m_lastError;
};