#pragma once

#include <SFML/Graphics.hpp>

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

// This class owns the tile map data for Game 1 / SurfersQuest.
// It loads the map, draws all world-specific floor tile variants,
// answers collision queries, and handles breakable blocks.
class GAME1_Level
{
public:
	static constexpr int TileSize = 64;

	// New preferred loader: pass the SurfersQuest Resources directory.
	// Example: assets/Game#1/SurfersQuest/Resources
	bool loadFromFile(const std::string& mapPath,
		const std::string& resourcesDirectory);

	// Kept for old main.cpp compatibility. The old floor/break paths are used
	// to infer the Resources directory where possible.
	bool loadFromFile(const std::string& mapPath,
		const std::string& floorTexturePath,
		const std::string& breakTexturePath);

	void draw(sf::RenderWindow& window) const;

	bool isSolidTile(int col, int row) const;
	bool isBreakTile(int col, int row) const;
	char getTile(int col, int row) const;
	void breakTile(int col, int row);

	void spawnRandomBreakBlocks(int count, const sf::FloatRect& forbiddenArea);

	int getWorldNumber() const;
	sf::Vector2f getPlayerSpawnPosition() const;

	int getWidthInTiles() const;
	int getHeightInTiles() const;
	float getPixelWidth() const;
	float getPixelHeight() const;

	const std::string& getLastError() const;

private:
	bool loadFromFileInternal(const std::string& mapPath,
		const std::string& resourcesDirectory,
		const std::string& optionalBreakTexturePath);

	bool loadWorldFloorTextures(const std::filesystem::path& worldTilesDirectory);
	bool loadBreakTexture(const std::filesystem::path& resourcesDirectory,
		const std::string& optionalBreakTexturePath);

	bool isInside(int col, int row) const;
	bool isFloorTile(char tile) const;
	const sf::Texture* getTextureForTile(char tile) const;

private:
	std::vector<std::string> m_rows;

	std::string m_resourcesDirectory;
	int m_worldNumber = 1;
	sf::Vector2f m_playerSpawnPosition{ 100.f, 100.f };

	std::unordered_map<char, sf::Texture> m_floorTextures;
	sf::Texture m_breakTexture;

	std::string m_lastError;
};
