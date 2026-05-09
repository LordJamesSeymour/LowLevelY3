#pragma once

#include "BombermanTypes.h"

#include <SFML/Graphics.hpp>
#include <cstddef>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

class BombermanLevel
{
public:
	static constexpr int TileSize = 48;

public:
	bool loadFromFile(const std::string& mapPath, const std::string& resourcesDirectory);

	void updateAnimations(float deltaTime);
	std::vector<BombermanGridPosition> consumeCompletedBrokenBlocks();

	void generateRandomBreakableBlocks(std::mt19937& rng,
		int minimumBlocks,
		float maxEmptyTileRatio,
		const std::vector<BombermanGridPosition>& forbiddenPositions);

	void draw(sf::RenderTarget& target) const;

	void drawFloorLayer(sf::RenderTarget& target) const;
	void drawWorldTileAt(sf::RenderTarget& target, int col, int row) const;

	void drawBaseLayer(sf::RenderTarget& target, bool includeSolidWalls) const;
	void drawSolidWallsOnly(sf::RenderTarget& target) const;
	void drawSolidWallAt(sf::RenderTarget& target, int col, int row) const;

	bool isInside(int col, int row) const;
	bool isWall(int col, int row) const;
	bool isBreakableBlock(int col, int row) const;
	bool isBlockedForMovement(int col, int row) const;
	bool canExplosionPassThrough(int col, int row) const;

	bool startBreakingBlock(int col, int row);
	void destroyBreakableBlock(int col, int row);

	bool revealExitAt(int col, int row);
	std::vector<BombermanGridPosition> getBreakableBlockPositions() const;

	BombermanGridPosition getPlayerSpawn() const;
	const std::vector<BombermanGridPosition>& getEnemySpawns() const;
	const std::vector<BombermanEnemySpawn>& getEnemySpawnEntries() const;

	bool hasExit() const;
	BombermanGridPosition getExitPosition() const;

	int getWorldNumber() const;

	int getWidthInTiles() const;
	int getHeightInTiles() const;

	float getPixelWidth() const;
	float getPixelHeight() const;

	sf::Vector2f gridToWorldTopLeft(BombermanGridPosition gridPosition) const;
	sf::Vector2f gridToWorldCenter(BombermanGridPosition gridPosition) const;
	BombermanGridPosition worldToGrid(sf::Vector2f worldPosition) const;

	const std::string& getLastError() const;

private:
	struct ActiveBrokenBlock
	{
		BombermanGridPosition gridPosition{ 0, 0 };
		float timer = 0.f;
	};

private:
	bool loadTexture(sf::Texture& texture, const std::string& path, const std::string& readableName);

	bool loadAnimationFramesFromDirectory(std::vector<sf::Texture>& frames,
		const std::string& directoryPath,
		const std::string& readableName);

	bool loadWorldTileTextures(const std::string& resourcesDirectory);
	bool loadWorldWallTexture(char tile, const std::string& texturePath, const std::string& readableName);

	void drawTextureInTile(sf::RenderTarget& target, const sf::Texture& texture, int col, int row) const;

	bool isSolidWallCharacter(char tile) const;
	const sf::Texture* getWallTextureForTile(char tile) const;

	const sf::Texture* getCurrentBreakableTexture() const;
	const sf::Texture* getCurrentExitTexture() const;

	const ActiveBrokenBlock* findActiveBrokenBlock(int col, int row) const;
	std::size_t getBrokenAnimationFrameIndex(const ActiveBrokenBlock& brokenBlock) const;
	float getBrokenAnimationDuration() const;

	bool isForbiddenBreakablePosition(int col,
		int row,
		const std::vector<BombermanGridPosition>& forbiddenPositions) const;

private:
	std::vector<std::string> m_rows;

	int m_worldNumber = 1;

	sf::Texture m_floorTexture;
	std::unordered_map<char, sf::Texture> m_wallTextures;

	std::vector<sf::Texture> m_exitFrames;
	std::size_t m_exitCurrentFrame = 0;
	float m_exitAnimationTimer = 0.f;
	float m_exitFrameDuration = 0.22f;

	std::vector<sf::Texture> m_breakableFrames;
	std::size_t m_breakableCurrentFrame = 0;
	float m_breakableAnimationTimer = 0.f;
	float m_breakableFrameDuration = 0.24f;

	std::vector<sf::Texture> m_brokenFrames;
	std::vector<ActiveBrokenBlock> m_activeBrokenBlocks;
	std::vector<BombermanGridPosition> m_completedBrokenBlocks;
	float m_brokenFrameDuration = 0.14f;

	BombermanGridPosition m_playerSpawn{ 1, 1 };
	std::vector<BombermanGridPosition> m_enemySpawns;
	std::vector<BombermanEnemySpawn> m_enemySpawnEntries;

	bool m_hasExit = false;
	BombermanGridPosition m_exitPosition{ 0, 0 };

	std::string m_lastError;
};