#pragma once

#include <SFML/Graphics.hpp>

#include "GAME1_ParallaxBackground.h"
#include "GAME1_LevelObject.h"
#include "GAME1_Pickup.h"
#include "GAME1_Trap.h"

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
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
	// Fruit pickups use lowercase ASCII codes:
	// c Cherry, s Strawberry, a Apple, b Banana, o Orange, k Kiwi,
	// m Melon, p Pineapple.
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
	// Parallax-aware background. Pass the gameplay camera centre (world px).
	// Falls back to the legacy bg.png if no parallax set is loaded.
	void drawBackground(sf::RenderWindow& window,
		sf::Vector2f cameraCenter) const;

	void updateCheckpoints(float deltaTime);
	void resetTraps();
	void updateGoalTiles(float deltaTime);
	void resetGoalTiles();

	int getCheckpointCount() const;
	sf::FloatRect getCheckpointBounds(int index) const;
	sf::Vector2f getCheckpointSpawnPosition(int index) const;
	int getCheckpointOrderIndex(int index) const;
	bool isCheckpointTriggered(int index) const;
	void triggerCheckpoint(int index);

	int getEndTileCount() const;
	sf::FloatRect getEndTileBounds(int index) const;
	bool isEndTileTriggered(int index) const;
	bool hasEndTileCompletedAnimationCycles(int index, int cycleCount) const;
	void triggerEndTile(int index);

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
	const std::vector<GAME1_PickupSpawn>& getPickupSpawns() const;
	const std::vector<GAME1_TrapSpawn>& getTrapSpawns() const;

	void updateTraps(float deltaTime);
	std::optional<GAME1_TrapPlatformContact> findPlatformContact(
		const sf::FloatRect& playerBounds,
		float previousPlayerBottom) const;
	void markTrapPlayerStanding(std::size_t trapIndex);
	sf::Vector2f getFanForceForBounds(const sf::FloatRect& playerBounds) const;
	std::optional<sf::FloatRect> getActiveFireDamageBounds(const sf::FloatRect& playerBounds) const;

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
	void loadWorldBackgroundTileTextures(const std::filesystem::path& worldTilesDirectory);
	void loadSpecialTileTextures(const std::filesystem::path& resourcesDirectory);
	void loadCheckpointTextures(const std::filesystem::path& resourcesDirectory);
	void loadGoalTileTextures(const std::filesystem::path& resourcesDirectory);
	void loadWorldBackgroundTexture(const std::filesystem::path& resourcesDirectory);
	void loadParallaxBackground(const std::filesystem::path& resourcesDirectory);
	const sf::Texture* getCheckpointCurrentTexture(std::size_t checkpointIndex) const;
	const sf::Texture* getStartTileCurrentTexture(std::size_t startIndex) const;
	const sf::Texture* getEndTileCurrentTexture(std::size_t endIndex) const;

	bool isInside(int col, int row) const;
	bool isFloorTile(char tile) const;
	bool isSpikeTrapCode(char tile) const;
	bool isOneWayPlatformCode(char tile) const;
	bool isSupportedSpecialTileCode(char tile) const;
	const sf::Texture* getTextureForTile(char tile) const;
	void rebuildTrapRuntime();
	void configureMovingPlatformPaths();
	bool hasChainAt(sf::Vector2i gridPosition) const;
	// Decoration-only background tiles. Drawn after the parallax background but
	// behind every solid/foreground tile. They have no collision or gameplay
	// effect (gameplay queries only read m_rows).
	void drawBackgroundTiles(sf::RenderWindow& window) const;
	void drawTraps(sf::RenderWindow& window) const;
	void drawGoalTiles(sf::RenderWindow& window) const;
	static bool rectsIntersect(const sf::FloatRect& a, const sf::FloatRect& b);

private:
	std::vector<std::string> m_rows;
	// Parallel grid of background-decoration tile codes ('O' = empty). Same
	// dimensions as m_rows. Saved in a separate #BACKGROUND map section.
	std::vector<std::string> m_bgRows;

	std::string m_resourcesDirectory;
	int m_worldNumber = 1;
	sf::Vector2f m_playerSpawnPosition{ 100.f, 100.f };
	std::vector<GAME1_LevelEnemySpawn> m_enemySpawns;
	std::vector<GAME1_PickupSpawn> m_pickupSpawns;
	std::vector<GAME1_TrapSpawn> m_trapSpawns;
	std::vector<GAME1_Trap> m_traps;
	// Grid cells occupied by the solid base block of a Fire trap.  Packed
	// as (col * 65536 + row) for O(1) lookup from isSolidTile().
	std::unordered_set<long long> m_fireBaseTileCells;
	GAME1_TrapAssets m_trapAssets;

	std::unordered_map<char, sf::Texture> m_floorTextures;
	std::unordered_map<char, sf::Texture> m_backgroundTileTextures;
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

	GAME1_ParallaxBackground m_parallaxBackground;

	sf::Texture m_checkpointNoFlagTexture;
	bool m_hasCheckpointNoFlagTexture = false;
	std::vector<sf::Texture> m_checkpointActivationFrames;
	std::vector<sf::Texture> m_checkpointLoopFrames;
	float m_checkpointFrameDuration = 0.06f;

	std::vector<CheckpointInstance> m_checkpoints;

	enum class StartTilePhase
	{
		Moving,
		Idle
	};

	struct StartTileInstance
	{
		sf::Vector2f tilePosition{ 0.f, 0.f };
		StartTilePhase phase = StartTilePhase::Moving;
		std::size_t frameIndex = 0;
		float frameTimer = 0.f;
	};

	struct EndTileInstance
	{
		sf::Vector2f tilePosition{ 0.f, 0.f };
		bool triggered = false;
		std::size_t frameIndex = 0;
		float frameTimer = 0.f;
		int completedAnimationCycles = 0;
	};

	sf::Texture m_startIdleTexture;
	bool m_hasStartIdleTexture = false;
	std::vector<sf::Texture> m_startMovingFrames;

	sf::Texture m_endIdleTexture;
	bool m_hasEndIdleTexture = false;
	std::vector<sf::Texture> m_endLoopFrames;

	float m_goalTileFrameDuration = 0.08f;
	std::vector<StartTileInstance> m_startTiles;
	std::vector<EndTileInstance> m_endTiles;

	std::string m_lastError;
};
