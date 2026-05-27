#pragma once

#include <SFML/Graphics.hpp>

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

enum class GAME1_LevelEnemyType
{
	Basic
};

struct GAME1_LevelEnemySpawn
{
	GAME1_LevelEnemyType type = GAME1_LevelEnemyType::Basic;
	sf::Vector2f position{ 0.f, 0.f };
};

enum class GAME1_SurfaceTag
{
	Grass,
	Rock,
	Floor
};

// This class owns the tile map data for Game 1 / SurfersQuest.
// It loads the map, draws all world-specific floor tile variants,
// and answers collision / special-tile queries.
// Legacy B tiles are treated as empty space.
class GAME1_Level
{
public:
	static constexpr int TileSize = 64;

	// Special non-world tile codes used by the SurfersQuest editor/loader.
	// ^  = spike trap
	// [  = one-way platform left part
	// =  = one-way platform middle part
	// ]  = one-way platform right part
	static constexpr char SpikeTrapTile = '^';
	static constexpr char OneWayPlatformLeftTile = '[';
	static constexpr char OneWayPlatformMiddleTile = '=';
	static constexpr char OneWayPlatformRightTile = ']';
	static constexpr char BasicEnemySpawnTile = 'e';
	static constexpr char CheckpointTile = 'K';

	// New preferred loader: pass the SurfersQuest Resources directory.
	// Example: assets/Game#1/SurfersQuest/Resources
	bool loadFromFile(const std::string& mapPath,
		const std::string& resourcesDirectory);

	// Kept for old main.cpp compatibility. The middle argument is ignored.
	// The floor path is used to infer the Resources directory where possible.
	bool loadFromFile(const std::string& mapPath,
		const std::string& floorTexturePath,
		const std::string& ignoredLegacyTexturePath);

	void draw(sf::RenderWindow& window) const;
	void drawBackground(sf::RenderWindow& window) const;

	void updateCheckpoints(float deltaTime);

	int getCheckpointCount() const;
	sf::FloatRect getCheckpointBounds(int index) const;
	sf::Vector2f getCheckpointSpawnPosition(int index) const;
	int getCheckpointOrderIndex(int index) const;
	bool isCheckpointTriggered(int index) const;
	void triggerCheckpoint(int index);

	// Full solid tiles are the normal world/floor tiles.
	// Spike traps and one-way platforms are queried separately.
	bool isSolidTile(int col, int row) const;
	bool isSpikeTrapTile(int col, int row) const;
	bool isOneWayPlatformTile(int col, int row) const;
	char getTile(int col, int row) const;
	GAME1_SurfaceTag getSurfaceTagForTile(int col, int row) const;
	GAME1_SurfaceTag getSurfaceTagAtWorldPosition(sf::Vector2f worldPosition) const;

	int getWorldNumber() const;
	sf::Vector2f getPlayerSpawnPosition() const;
	const std::vector<GAME1_LevelEnemySpawn>& getEnemySpawns() const;

	int getWidthInTiles() const;
	int getHeightInTiles() const;
	float getPixelWidth() const;
	float getPixelHeight() const;

	const std::string& getLastError() const;

private:
	bool loadFromFileInternal(const std::string& mapPath,
		const std::string& resourcesDirectory,
		const std::string& ignoredLegacyTexturePath);

	bool loadWorldFloorTextures(const std::filesystem::path& worldTilesDirectory);
	void loadSpecialTileTextures(const std::filesystem::path& resourcesDirectory);
	void loadCheckpointTextures(const std::filesystem::path& resourcesDirectory);
	void loadWorldBackgroundTexture(const std::filesystem::path& resourcesDirectory);
	const sf::Texture* getCheckpointCurrentTexture(std::size_t checkpointIndex) const;

	bool isInside(int col, int row) const;
	bool isFloorTile(char tile) const;
	bool isSpikeTrapCode(char tile) const;
	bool isOneWayPlatformCode(char tile) const;
	bool isSupportedSpecialTileCode(char tile) const;
	const sf::Texture* getTextureForTile(char tile) const;

private:
	std::vector<std::string> m_rows;

	std::string m_resourcesDirectory;
	int m_worldNumber = 1;
	sf::Vector2f m_playerSpawnPosition{ 100.f, 100.f };
	std::vector<GAME1_LevelEnemySpawn> m_enemySpawns;

	std::unordered_map<char, sf::Texture> m_floorTextures;
	std::unordered_map<char, sf::Texture> m_specialTileTextures;

	enum class CheckpointPhase
	{
		NoFlag,
		Activating,
		Looping
	};

	struct CheckpointInstance
	{
		sf::Vector2f tilePosition{ 0.f, 0.f };
		int orderIndex = 0;
		CheckpointPhase phase = CheckpointPhase::NoFlag;
		std::size_t frameIndex = 0;
		float frameTimer = 0.f;
	};

	sf::Texture m_backgroundTexture;
	bool m_hasBackgroundTexture = false;

	sf::Texture m_checkpointNoFlagTexture;
	bool m_hasCheckpointNoFlagTexture = false;
	std::vector<sf::Texture> m_checkpointActivationFrames;
	std::vector<sf::Texture> m_checkpointLoopFrames;
	float m_checkpointFrameDuration = 0.06f;

	std::vector<CheckpointInstance> m_checkpoints;

	std::string m_lastError;
};
