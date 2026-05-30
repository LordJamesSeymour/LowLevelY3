#include "GAME1_Level.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iostream>
#include <optional>
#include <unordered_set>
#include <utility>

namespace
{
	std::string ToLower(std::string value)
	{
		std::transform(value.begin(), value.end(), value.begin(),
			[](unsigned char c)
			{
				return static_cast<char>(std::tolower(c));
			});

		return value;
	}

	bool IsPngFile(const std::filesystem::path& path)
	{
		return path.has_extension() && ToLower(path.extension().string()) == ".png";
	}

	std::optional<int> ExtractTrailingNumber(const std::filesystem::path& path)
	{
		const std::string stem = path.stem().string();

		if (stem.empty())
			return std::nullopt;

		int end = static_cast<int>(stem.size()) - 1;

		if (!std::isdigit(static_cast<unsigned char>(stem[end])))
			return std::nullopt;

		int start = end;
		while (start > 0 && std::isdigit(static_cast<unsigned char>(stem[start - 1])))
			--start;

		try
		{
			return std::stoi(stem.substr(
				static_cast<std::size_t>(start),
				static_cast<std::size_t>(end - start + 1)));
		}
		catch (...)
		{
			return std::nullopt;
		}
	}

	bool NaturalFrameSort(const std::filesystem::path& a, const std::filesystem::path& b)
	{
		const std::optional<int> numberA = ExtractTrailingNumber(a);
		const std::optional<int> numberB = ExtractTrailingNumber(b);

		if (numberA.has_value() && numberB.has_value() && numberA.value() != numberB.value())
			return numberA.value() < numberB.value();

		return a.filename().string() < b.filename().string();
	}

	bool TryParseWorldMetadata(const std::string& line, int& outWorldNumber)
	{
		const std::string prefix = "#WORLD=";

		if (line.rfind(prefix, 0) != 0)
			return false;

		try
		{
			const int parsedWorld = std::stoi(line.substr(prefix.size()));

			if (parsedWorld >= 1)
				outWorldNumber = parsedWorld;
		}
		catch (...)
		{
		}

		return true;
	}

	bool ContainsAnyKeyword(const std::filesystem::path& path,
		const std::vector<std::string>& keywords)
	{
		const std::string stem = ToLower(path.stem().string());
		const std::string parent = ToLower(path.parent_path().filename().string());
		const std::string combined = parent + "_" + stem;

		for (const std::string& keyword : keywords)
		{
			if (combined.find(ToLower(keyword)) != std::string::npos)
				return true;
		}

		return false;
	}

	std::vector<std::filesystem::path> FindSpecialPngFiles(
		const std::filesystem::path& searchRoot,
		const std::vector<std::string>& keywords)
	{
		namespace fs = std::filesystem;

		std::vector<fs::path> paths;

		if (!fs::exists(searchRoot) || !fs::is_directory(searchRoot))
			return paths;

		for (const auto& entry : fs::recursive_directory_iterator(searchRoot))
		{
			if (!entry.is_regular_file())
				continue;

			if (!IsPngFile(entry.path()))
				continue;

			if (ContainsAnyKeyword(entry.path(), keywords))
				paths.push_back(entry.path());
		}

		std::sort(paths.begin(), paths.end(), NaturalFrameSort);
		return paths;
	}

	bool LoadTextureIfFileExists(sf::Texture& texture, const std::filesystem::path& path)
	{
		if (!std::filesystem::exists(path) || !std::filesystem::is_regular_file(path))
			return false;

		return texture.loadFromFile(path.string());
	}

	void WarnLevelAsset(const std::string& message)
	{
		std::cerr << message << '\n';
	}

	std::optional<char> KnownTileCodeForStem(const std::string& rawStem)
	{
		const std::string stem = ToLower(rawStem);

		if (stem == "floor_center_0" || stem == "floor_center" || stem == "floor" || stem == "floortile")
			return 'X';

		if (stem == "floor_bottom") return 'D';
		if (stem == "floor_bottomleft") return 'Z';
		if (stem == "floor_bottomright") return 'C';
		if (stem == "floor_left") return 'L';
		if (stem == "floor_right") return 'R';

		if (stem == "grass_top") return 'G';
		if (stem == "grass_left") return 'H';
		if (stem == "grass_right") return 'J';
		if (stem == "grass_topleft") return 'Q';
		if (stem == "grass_topright") return 'Y';

		// Solid terrain blocks. These share one logical id per shape across all
		// worlds; only the texture folder changes with the level's #WORLD tag.
		if (stem == "solidsingle") return 'S';
		if (stem == "solidlong_left") return 'T';
		if (stem == "solidlong_center") return 'U';
		if (stem == "solidlong_right") return 'V';
		if (stem == "solidtall_top") return 'W';
		if (stem == "solidtall_center") return 'A';
		if (stem == "solidtall_bot") return 'E';
		if (stem == "solid2x2_topleft") return 'F';
		if (stem == "solid2x2_topright") return 'I';
		if (stem == "solid2x2_botleft") return 'M';
		if (stem == "solid2x2_botright") return 'N';

		return std::nullopt;
	}

	bool IsBackgroundTileStem(const std::string& rawStem)
	{
		return ToLower(rawStem).rfind("bg_", 0) == 0;
	}

	// Logical code for each background-decoration tile. The background layer is
	// its own map section / grid, so these lowercase codes are deliberately
	// disjoint from the foreground tile codes and never collide with them.
	// Must stay in sync with the editor's copy in GAME1_LevelEditor.cpp.
	std::optional<char> KnownBackgroundTileCodeForStem(const std::string& rawStem)
	{
		const std::string stem = ToLower(rawStem);

		if (stem == "bg_topleft") return 'd';
		if (stem == "bg_topcenter") return 'f';
		if (stem == "bg_topright") return 'g';
		if (stem == "bg_leftcenter") return 'h';
		if (stem == "bg_rightcenter") return 'i';
		if (stem == "bg_botleft") return 'j';
		if (stem == "bg_botcenter") return 'l';
		if (stem == "bg_botright") return 'n';
		if (stem == "bg_topleftend") return 'q';
		if (stem == "bg_toprightend") return 'r';
		if (stem == "bg_botleftend") return 't';
		if (stem == "bg_botrightend") return 'u';
		if (stem == "bg_empty") return 'v';

		return std::nullopt;
	}

	std::vector<char> FallbackTileCodes()
	{
		// P is reserved for PlayerSpawn, O is empty, and special trap/platform
		// characters plus lowercase fruit pickup codes are handled separately.
		return
		{
			'X', 'A', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
			'N', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'Y', 'Z'
		};
	}

	bool IsReservedMapTileCode(char code)
	{
		GAME1_FruitType ignoredFruitType;

		return code == 'O' ||
			code == 'B' ||
			code == 'P' ||
			code == GAME1_Level::SpikeTrapTile ||
			code == GAME1_Level::OneWayPlatformLeftTile ||
			code == GAME1_Level::OneWayPlatformMiddleTile ||
			code == GAME1_Level::OneWayPlatformRightTile ||
			code == GAME1_Level::CheckpointTile ||
			GAME1_TryGetFruitTypeForMapCode(code, ignoredFruitType);
	}

	std::vector<std::pair<char, std::filesystem::path>> BuildWorldTileDefinitions(
		const std::filesystem::path& worldTilesDirectory)
	{
		namespace fs = std::filesystem;

		std::vector<fs::path> pngPaths;

		if (!fs::exists(worldTilesDirectory) || !fs::is_directory(worldTilesDirectory))
			return {};

		for (const auto& entry : fs::directory_iterator(worldTilesDirectory))
		{
			if (entry.is_regular_file() && IsPngFile(entry.path()))
			{
				// Background-decoration tiles share this folder but belong to a
				// separate layer; keep them out of the foreground tile codes.
				if (IsBackgroundTileStem(entry.path().stem().string()))
					continue;

				pngPaths.push_back(entry.path());
			}
		}

		std::sort(pngPaths.begin(), pngPaths.end(), NaturalFrameSort);

		std::vector<std::pair<char, fs::path>> definitions;
		std::unordered_set<char> usedCodes;
		std::unordered_set<std::string> usedFiles;

		auto addDefinition = [&](char requestedCode, const fs::path& path)
			{
				if (usedFiles.find(path.string()) != usedFiles.end())
					return;

				char finalCode = requestedCode;

				if (usedCodes.find(finalCode) != usedCodes.end() || IsReservedMapTileCode(finalCode))
				{
					for (char fallback : FallbackTileCodes())
					{
						if (!IsReservedMapTileCode(fallback) && usedCodes.find(fallback) == usedCodes.end())
						{
							finalCode = fallback;
							break;
						}
					}
				}

				if (usedCodes.find(finalCode) != usedCodes.end() || IsReservedMapTileCode(finalCode))
					return;

				usedCodes.insert(finalCode);
				usedFiles.insert(path.string());
				definitions.push_back({ finalCode, path });
			};

		// Keep legacy maps stable by ensuring the centre floor uses X when possible.
		for (const fs::path& path : pngPaths)
		{
			const std::optional<char> knownCode = KnownTileCodeForStem(path.stem().string());

			if (knownCode.has_value() && knownCode.value() == 'X')
			{
				addDefinition('X', path);
				break;
			}
		}

		for (const fs::path& path : pngPaths)
		{
			const std::optional<char> knownCode = KnownTileCodeForStem(path.stem().string());

			if (knownCode.has_value())
				addDefinition(knownCode.value(), path);
		}

		std::size_t fallbackIndex = 0;
		const std::vector<char> fallbackCodes = FallbackTileCodes();

		for (const fs::path& path : pngPaths)
		{
			if (usedFiles.find(path.string()) != usedFiles.end())
				continue;

			while (fallbackIndex < fallbackCodes.size() &&
				(IsReservedMapTileCode(fallbackCodes[fallbackIndex]) ||
					usedCodes.find(fallbackCodes[fallbackIndex]) != usedCodes.end()))
			{
				++fallbackIndex;
			}

			if (fallbackIndex >= fallbackCodes.size())
				break;

			addDefinition(fallbackCodes[fallbackIndex], path);
			++fallbackIndex;
		}

		// If the folder has PNGs but none were known, force the first one to X.
		if (definitions.empty() && !pngPaths.empty())
			definitions.push_back({ 'X', pngPaths.front() });

		return definitions;
	}

	std::filesystem::path InferResourcesDirectoryFromOldFloorPath(const std::string& floorTexturePath)
	{
		namespace fs = std::filesystem;

		const fs::path floorPath(floorTexturePath);
		const fs::path parent = floorPath.parent_path();

		if (ToLower(parent.filename().string()).rfind("world", 0) == 0)
		{
			// .../Resources/Tiles/World1/Floor_Center_0.png
			return parent.parent_path().parent_path();
		}

		if (ToLower(parent.filename().string()) == "tiles")
		{
			// .../Resources/Tiles/FloorTile.png
			return parent.parent_path();
		}

		if (!parent.empty())
		{
			// Old layout: .../Resources/FloorTile.png
			return parent;
		}

		return fs::path("assets") / "Game#1" / "SurfersQuest" / "Resources";
	}
}

bool GAME1_Level::loadFromFile(const std::string& mapPath,
	const std::string& resourcesDirectory)
{
	return loadFromFileInternal(mapPath, resourcesDirectory, "");
}

bool GAME1_Level::loadFromFile(const std::string& mapPath,
	const std::string& floorTexturePath,
	const std::string& ignoredLegacyTexturePath)
{
	(void)ignoredLegacyTexturePath;
	const std::filesystem::path inferredResourcesDirectory = InferResourcesDirectoryFromOldFloorPath(floorTexturePath);
	return loadFromFileInternal(mapPath, inferredResourcesDirectory.string(), "");
}

bool GAME1_Level::loadFromFileInternal(const std::string& mapPath,
	const std::string& resourcesDirectory,
	const std::string& ignoredLegacyTexturePath)
{
	namespace fs = std::filesystem;

	(void)ignoredLegacyTexturePath;

	m_rows.clear();
	m_bgRows.clear();
	m_lastError.clear();
	m_floorTextures.clear();
	m_backgroundTileTextures.clear();
	m_specialTileTextures.clear();
	m_enemySpawns.clear();
	m_pickupSpawns.clear();
	m_trapSpawns.clear();
	m_traps.clear();
	m_checkpoints.clear();
	m_checkpointActivationFrames.clear();
	m_checkpointLoopFrames.clear();
	m_hasCheckpointNoFlagTexture = false;
	m_startTiles.clear();
	m_endTiles.clear();
	m_startMovingFrames.clear();
	m_endLoopFrames.clear();
	m_hasStartIdleTexture = false;
	m_hasEndIdleTexture = false;
	m_hasBackgroundTexture = false;
	m_worldNumber = 1;
	m_playerSpawnPosition = { 100.f, 100.f };
	m_resourcesDirectory = resourcesDirectory;

	std::ifstream file(mapPath);
	if (!file.is_open())
	{
		m_lastError = "Failed to open map file: " + mapPath;
		return false;
	}

	std::vector<std::string> rawRows;
	std::vector<std::string> rawBgRows;
	std::string line;
	std::size_t expectedWidth = 0;
	bool objectSectionStarted = false;
	bool backgroundSectionStarted = false;

	while (std::getline(file, line))
	{
		if (!line.empty() && line.back() == '\r')
			line.pop_back();

		if (line.empty())
			continue;

		if (TryParseWorldMetadata(line, m_worldNumber))
			continue;

		if (line == "#BACKGROUND")
		{
			backgroundSectionStarted = true;
			continue;
		}

		if (line == "#OBJECTS")
		{
			objectSectionStarted = true;
			backgroundSectionStarted = false;
			continue;
		}

		if (!line.empty() && line[0] == '#')
			continue;

		// Background-layer rows are captured verbatim and aligned to the
		// foreground grid after the map body is fully parsed.
		if (backgroundSectionStarted)
		{
			rawBgRows.push_back(line);
			continue;
		}

		if (objectSectionStarted || line.rfind("OBJECT ", 0) == 0)
		{
			GAME1_LevelObjectSpawn levelObjectSpawn;
			if (GAME1_TryParseLevelObjectLine(line, levelObjectSpawn))
			{
				const sf::Vector2f position(
					static_cast<float>(levelObjectSpawn.gridPosition.x * TileSize),
					static_cast<float>(levelObjectSpawn.gridPosition.y * TileSize));

				if (levelObjectSpawn.type == GAME1_LevelObjectType::StartTile)
				{
					StartTileInstance instance;
					instance.tilePosition = position;
					m_startTiles.push_back(instance);
				}
				else if (levelObjectSpawn.type == GAME1_LevelObjectType::EndTile)
				{
					EndTileInstance instance;
					instance.tilePosition = position;
					m_endTiles.push_back(instance);
				}

				continue;
			}

			GAME1_TrapSpawn trapSpawn;
			if (!GAME1_TryParseTrapObjectLine(line, trapSpawn))
			{
				m_lastError = "Map error: invalid object line: " + line;
				return false;
			}

			m_trapSpawns.push_back(trapSpawn);
			continue;
		}

		if (expectedWidth == 0)
		{
			expectedWidth = line.size();
		}
		else if (line.size() != expectedWidth)
		{
			m_lastError = "Map error: all rows must have the same width.";
			return false;
		}

		rawRows.push_back(line);
	}

	if (rawRows.empty())
	{
		m_lastError = "Map error: map file is empty.";
		return false;
	}

	fs::path resourcesPath(resourcesDirectory);

	if (!fs::exists(resourcesPath) || !fs::is_directory(resourcesPath))
	{
		const fs::path fallback = fs::path("assets") / "Game#1" / "SurfersQuest" / "Resources";
		if (fs::exists(fallback) && fs::is_directory(fallback))
		{
			resourcesPath = fallback;
			m_resourcesDirectory = fallback.string();
		}
	}

	const fs::path worldTilesDirectory = resourcesPath / "Tiles" / ("World" + std::to_string(m_worldNumber));

	if (!loadWorldFloorTextures(worldTilesDirectory))
		return false;

	loadWorldBackgroundTileTextures(worldTilesDirectory);

	m_trapAssets.load(resourcesPath.string());
	loadSpecialTileTextures(resourcesPath);
	loadCheckpointTextures(resourcesPath);
	loadGoalTileTextures(resourcesPath);
	loadWorldBackgroundTexture(resourcesPath);
	loadParallaxBackground(resourcesPath);

	for (std::size_t row = 0; row < rawRows.size(); ++row)
	{
		for (std::size_t col = 0; col < rawRows[row].size(); ++col)
		{
			const char tile = rawRows[row][col];

			if (tile == 'P')
			{
				m_playerSpawnPosition = {
					static_cast<float>(col * TileSize),
					static_cast<float>(row * TileSize)
				};

				// The player spawn marker should not act as a solid floor tile.
				rawRows[row][col] = 'O';
				continue;
			}

			if (tile == 'B')
			{
				// Legacy B tiles are now treated as empty so old maps can still load.
				rawRows[row][col] = 'O';
				continue;
			}

			if (tile == BasicEnemySpawnTile)
			{
				m_enemySpawns.push_back(
					{
						GAME1_LevelEnemyType::Basic,
						{
							static_cast<float>(col * TileSize),
							static_cast<float>(row * TileSize)
						}
					});

				rawRows[row][col] = 'O';
				continue;
			}

			if (tile == CheckpointTile)
			{
				CheckpointInstance instance;
				instance.tilePosition = {
					static_cast<float>(col * TileSize),
					static_cast<float>(row * TileSize)
				};
				m_checkpoints.push_back(instance);

				rawRows[row][col] = 'O';
				continue;
			}

			GAME1_FruitType pickupType;
			if (GAME1_TryGetFruitTypeForMapCode(tile, pickupType))
			{
				const sf::Vector2i gridPosition(
					static_cast<int>(col),
					static_cast<int>(row));

				const float pickupOffset =
					(static_cast<float>(TileSize) - GAME1_Pickup::DrawSize) * 0.5f;

				m_pickupSpawns.push_back(
					{
						pickupType,
						gridPosition,
						{
							static_cast<float>(col * TileSize) + pickupOffset,
							static_cast<float>(row * TileSize) + pickupOffset
						}
					});

				rawRows[row][col] = 'O';
				continue;
			}

			if (tile == 'O' || isFloorTile(tile) || isSupportedSpecialTileCode(tile))
				continue;

			m_lastError =
				"Map error: unsupported tile character '" +
				std::string(1, tile) +
				"' at row " + std::to_string(row + 1) +
				", column " + std::to_string(col + 1) +
				". Use O for empty, P for player spawn, e for enemy spawn, c/s/a/b/o/k/m/p for fruit, a floor tile letter from the editor, ^ for spikes, or [/= /] for one-way platforms.";
			return false;
		}
	}

	m_rows = std::move(rawRows);

	// Build the background-decoration grid so it exactly matches the foreground
	// dimensions. Unknown / unsupported characters are treated as empty so old
	// maps (and maps authored for a different world set) still load cleanly.
	{
		const std::size_t height = m_rows.size();
		const std::size_t width = m_rows.empty() ? 0 : m_rows[0].size();

		m_bgRows.assign(height, std::string(width, 'O'));

		for (std::size_t row = 0; row < rawBgRows.size() && row < height; ++row)
		{
			const std::string& source = rawBgRows[row];

			for (std::size_t col = 0; col < source.size() && col < width; ++col)
			{
				const char code = source[col];

				if (code != 'O' &&
					m_backgroundTileTextures.find(code) != m_backgroundTileTextures.end())
				{
					m_bgRows[row][col] = code;
				}
			}
		}
	}

	std::sort(m_checkpoints.begin(), m_checkpoints.end(),
		[](const CheckpointInstance& a, const CheckpointInstance& b)
		{
			if (a.tilePosition.x != b.tilePosition.x)
				return a.tilePosition.x < b.tilePosition.x;
			return a.tilePosition.y < b.tilePosition.y;
		});

	for (std::size_t i = 0; i < m_checkpoints.size(); ++i)
		m_checkpoints[i].orderIndex = static_cast<int>(i);

	rebuildTrapRuntime();
	return true;
}

bool GAME1_Level::loadWorldFloorTextures(const std::filesystem::path& worldTilesDirectory)
{
	m_floorTextures.clear();

	const std::vector<std::pair<char, std::filesystem::path>> definitions =
		BuildWorldTileDefinitions(worldTilesDirectory);

	if (definitions.empty())
	{
		m_lastError =
			"Failed to load world floor tiles: no PNG files found in:\n" +
			worldTilesDirectory.string();
		return false;
	}

	for (const auto& definition : definitions)
	{
		sf::Texture texture;

		if (!texture.loadFromFile(definition.second.string()))
		{
			m_lastError = "Failed to load floor tile texture: " + definition.second.string();
			return false;
		}

		m_floorTextures[definition.first] = std::move(texture);
	}

	return true;
}

void GAME1_Level::loadWorldBackgroundTileTextures(const std::filesystem::path& worldTilesDirectory)
{
	namespace fs = std::filesystem;

	m_backgroundTileTextures.clear();

	if (!fs::exists(worldTilesDirectory) || !fs::is_directory(worldTilesDirectory))
		return;

	for (const auto& entry : fs::directory_iterator(worldTilesDirectory))
	{
		if (!entry.is_regular_file() || !IsPngFile(entry.path()))
			continue;

		if (!IsBackgroundTileStem(entry.path().stem().string()))
			continue;

		const std::optional<char> code =
			KnownBackgroundTileCodeForStem(entry.path().stem().string());

		if (!code.has_value())
			continue;

		sf::Texture texture;
		if (texture.loadFromFile(entry.path().string()))
			m_backgroundTileTextures[code.value()] = std::move(texture);
	}
}

void GAME1_Level::loadSpecialTileTextures(const std::filesystem::path& resourcesDirectory)
{
	namespace fs = std::filesystem;

	m_specialTileTextures.clear();

	const fs::path tilesDirectory = resourcesDirectory / "Tiles";

	// Spikes are expected at Resources/Tiles/Traps/Spikes_0.png, but this also
	// searches the Tiles folder recursively for any PNG with "spike" in its name.
	{
		sf::Texture spikeTexture;
		const fs::path preferredSpikePath = tilesDirectory / "Traps" / "Spikes_0.png";

		if (LoadTextureIfFileExists(spikeTexture, preferredSpikePath))
		{
			m_specialTileTextures[SpikeTrapTile] = std::move(spikeTexture);
		}
		else
		{
			const std::vector<fs::path> spikePaths = FindSpecialPngFiles(tilesDirectory, { "spike" });

			if (!spikePaths.empty() && spikeTexture.loadFromFile(spikePaths.front().string()))
			{
				m_specialTileTextures[SpikeTrapTile] = std::move(spikeTexture);
			}
		}
	}

	// One-way platforms are three separate parts, not an animation.
	// Preferred layout:
	// Resources/Tiles/Platform_1/Platform_Left.png
	// Resources/Tiles/Platform_1/Platform_Center.png
	// Resources/Tiles/Platform_1/Platform_Right.png
	// Falls back to recursive discovery for older asset layouts.
	std::vector<fs::path> platformPaths;
	const fs::path platformDirectory = tilesDirectory / "Platform_1";
	const std::vector<fs::path> preferredPlatformPaths =
	{
		platformDirectory / "Platform_Left.png",
		platformDirectory / "Platform_Center.png",
		platformDirectory / "Platform_Right.png"
	};

	const bool hasPreferredPlatformSet =
		std::all_of(
			preferredPlatformPaths.begin(),
			preferredPlatformPaths.end(),
			[](const fs::path& path)
			{
				return fs::exists(path) && fs::is_regular_file(path);
			});

	if (hasPreferredPlatformSet)
	{
		platformPaths = preferredPlatformPaths;
	}
	else
	{
		platformPaths = FindSpecialPngFiles(
			tilesDirectory,
			{ "platform", "oneway", "one_way", "one-way" });
	}

	if (!platformPaths.empty())
	{
		const char platformCodes[3] =
		{
			OneWayPlatformLeftTile,
			OneWayPlatformMiddleTile,
			OneWayPlatformRightTile
		};

		for (int i = 0; i < 3; ++i)
		{
			const std::size_t sourceIndex = std::min<std::size_t>(
				static_cast<std::size_t>(i),
				platformPaths.size() - 1);

			sf::Texture platformTexture;
			if (platformTexture.loadFromFile(platformPaths[sourceIndex].string()))
			{
				m_specialTileTextures[platformCodes[i]] = std::move(platformTexture);
			}
		}
	}
}

void GAME1_Level::loadCheckpointTextures(const std::filesystem::path& resourcesDirectory)
{
	namespace fs = std::filesystem;

	m_hasCheckpointNoFlagTexture = false;
	m_checkpointActivationFrames.clear();
	m_checkpointLoopFrames.clear();

	const fs::path checkpointDirectory = resourcesDirectory / "Checkpoint";

	if (!fs::exists(checkpointDirectory) || !fs::is_directory(checkpointDirectory))
		return;

	const fs::path noFlagDirectory = checkpointDirectory / "NoFlag";

	if (fs::exists(noFlagDirectory) && fs::is_directory(noFlagDirectory))
	{
		std::vector<fs::path> noFlagPaths;

		for (const auto& entry : fs::directory_iterator(noFlagDirectory))
		{
			if (entry.is_regular_file() && IsPngFile(entry.path()))
				noFlagPaths.push_back(entry.path());
		}

		std::sort(noFlagPaths.begin(), noFlagPaths.end(), NaturalFrameSort);

		if (!noFlagPaths.empty())
		{
			if (m_checkpointNoFlagTexture.loadFromFile(noFlagPaths.front().string()))
				m_hasCheckpointNoFlagTexture = true;
		}
	}

	auto loadFrames = [](const fs::path& directory, std::vector<sf::Texture>& outFrames)
		{
			if (!fs::exists(directory) || !fs::is_directory(directory))
				return;

			std::vector<fs::path> paths;

			for (const auto& entry : fs::directory_iterator(directory))
			{
				if (entry.is_regular_file() && IsPngFile(entry.path()))
					paths.push_back(entry.path());
			}

			std::sort(paths.begin(), paths.end(), NaturalFrameSort);

			for (const fs::path& path : paths)
			{
				sf::Texture texture;
				if (texture.loadFromFile(path.string()))
					outFrames.push_back(std::move(texture));
			}
		};

	loadFrames(checkpointDirectory / "Idle", m_checkpointActivationFrames);
	loadFrames(checkpointDirectory / "FlagOut", m_checkpointLoopFrames);
}

void GAME1_Level::loadGoalTileTextures(const std::filesystem::path& resourcesDirectory)
{
	namespace fs = std::filesystem;

	m_hasStartIdleTexture = false;
	m_hasEndIdleTexture = false;
	m_startMovingFrames.clear();
	m_endLoopFrames.clear();

	const fs::path startDirectory = resourcesDirectory / "Tiles" / "Start";
	const fs::path endDirectory = resourcesDirectory / "Tiles" / "End";

	auto loadTextureWithWarning = [](sf::Texture& texture, const fs::path& path)
		{
			if (!fs::exists(path) || !fs::is_regular_file(path))
			{
				WarnLevelAsset("SurfersQuest goal tile texture missing: " + path.string());
				return false;
			}

			if (!texture.loadFromFile(path.string()))
			{
				WarnLevelAsset("SurfersQuest goal tile texture failed to load: " + path.string());
				return false;
			}

			return true;
		};

	m_hasStartIdleTexture =
		loadTextureWithWarning(m_startIdleTexture, startDirectory / "StartIdle.png");

	for (int i = 0; i <= 16; ++i)
	{
		const fs::path framePath =
			startDirectory / ("Start_(Moving)_" + std::to_string(i) + ".png");

		sf::Texture texture;
		if (loadTextureWithWarning(texture, framePath))
			m_startMovingFrames.push_back(std::move(texture));
	}

	m_hasEndIdleTexture =
		loadTextureWithWarning(m_endIdleTexture, endDirectory / "EndIdle.png");

	for (int i = 0; i <= 7; ++i)
	{
		const fs::path framePath =
			endDirectory / ("frame_" + std::to_string(i) + ".png");

		sf::Texture texture;
		if (loadTextureWithWarning(texture, framePath))
			m_endLoopFrames.push_back(std::move(texture));
	}
}

void GAME1_Level::loadWorldBackgroundTexture(const std::filesystem::path& resourcesDirectory)
{
	namespace fs = std::filesystem;

	m_hasBackgroundTexture = false;

	const fs::path worldBackgroundDirectory =
		resourcesDirectory / "Background" / ("World" + std::to_string(m_worldNumber));

	const fs::path backgroundFile = worldBackgroundDirectory / "bg.png";

	if (!fs::exists(backgroundFile) || !fs::is_regular_file(backgroundFile))
		return;

	if (m_backgroundTexture.loadFromFile(backgroundFile.string()))
		m_hasBackgroundTexture = true;
}

void GAME1_Level::loadParallaxBackground(const std::filesystem::path& resourcesDirectory)
{
	// Parallax assets live in a flat folder shared by every Surfers Quest
	// level. Future work can pick a per-world set via loadSet().
	const std::filesystem::path parallaxDirectory = resourcesDirectory / "Parallax";
	m_parallaxBackground.loadFromDirectory(parallaxDirectory);
}

void GAME1_Level::drawBackground(sf::RenderWindow& window,
	sf::Vector2f cameraCenter) const
{
	if (m_parallaxBackground.hasAnyLayer())
	{
		m_parallaxBackground.draw(window, cameraCenter);
		return;
	}

	drawBackground(window);
}

void GAME1_Level::drawBackground(sf::RenderWindow& window) const
{
	if (m_parallaxBackground.hasAnyLayer())
	{
		m_parallaxBackground.draw(window, { 0.f, 0.f });
		return;
	}

	if (!m_hasBackgroundTexture)
		return;

	const sf::View previousView = window.getView();

	const sf::Vector2u windowSize = window.getSize();
	if (windowSize.x == 0 || windowSize.y == 0)
		return;

	const sf::FloatRect visibleArea(
		{ 0.f, 0.f },
		{ static_cast<float>(windowSize.x), static_cast<float>(windowSize.y) });

	window.setView(sf::View(visibleArea));

	sf::Sprite backgroundSprite(m_backgroundTexture);

	const sf::FloatRect localBounds = backgroundSprite.getLocalBounds();
	if (localBounds.size.x > 0.f && localBounds.size.y > 0.f)
	{
		backgroundSprite.setScale({
			static_cast<float>(windowSize.x) / localBounds.size.x,
			static_cast<float>(windowSize.y) / localBounds.size.y
			});
		backgroundSprite.setPosition({ 0.f, 0.f });
		window.draw(backgroundSprite);
	}

	window.setView(previousView);
}

void GAME1_Level::updateCheckpoints(float deltaTime)
{
	for (CheckpointInstance& checkpoint : m_checkpoints)
	{
		if (checkpoint.phase == CheckpointPhase::NoFlag)
			continue;

		checkpoint.frameTimer += deltaTime;

		while (checkpoint.frameTimer >= m_checkpointFrameDuration)
		{
			checkpoint.frameTimer -= m_checkpointFrameDuration;
			checkpoint.frameIndex += 1;

			if (checkpoint.phase == CheckpointPhase::Activating)
			{
				if (m_checkpointActivationFrames.empty() ||
					checkpoint.frameIndex >= m_checkpointActivationFrames.size())
				{
					checkpoint.phase = CheckpointPhase::Looping;
					checkpoint.frameIndex = 0;
				}
			}
			else if (checkpoint.phase == CheckpointPhase::Looping)
			{
				if (m_checkpointLoopFrames.empty())
				{
					checkpoint.frameIndex = 0;
				}
				else
				{
					checkpoint.frameIndex %= m_checkpointLoopFrames.size();
				}
			}
		}
	}
}

void GAME1_Level::resetTraps()
{
	rebuildTrapRuntime();
}

void GAME1_Level::updateGoalTiles(float deltaTime)
{
	for (StartTileInstance& startTile : m_startTiles)
	{
		if (startTile.phase == StartTilePhase::Idle)
			continue;

		if (m_startMovingFrames.empty())
		{
			startTile.phase = StartTilePhase::Idle;
			startTile.frameIndex = 0;
			startTile.frameTimer = 0.f;
			continue;
		}

		startTile.frameTimer += deltaTime;

		while (startTile.frameTimer >= m_goalTileFrameDuration)
		{
			startTile.frameTimer -= m_goalTileFrameDuration;

			if (startTile.frameIndex + 1 < m_startMovingFrames.size())
			{
				++startTile.frameIndex;
			}
			else
			{
				startTile.phase = StartTilePhase::Idle;
				startTile.frameIndex = 0;
				startTile.frameTimer = 0.f;
				break;
			}
		}
	}

	for (EndTileInstance& endTile : m_endTiles)
	{
		if (!endTile.triggered || m_endLoopFrames.empty())
			continue;

		endTile.frameTimer += deltaTime;

		while (endTile.frameTimer >= m_goalTileFrameDuration)
		{
			endTile.frameTimer -= m_goalTileFrameDuration;
			++endTile.frameIndex;

			if (endTile.frameIndex >= m_endLoopFrames.size())
			{
				endTile.frameIndex = 0;
				++endTile.completedAnimationCycles;
			}
		}
	}
}

void GAME1_Level::resetGoalTiles()
{
	for (StartTileInstance& startTile : m_startTiles)
	{
		startTile.phase = StartTilePhase::Moving;
		startTile.frameIndex = 0;
		startTile.frameTimer = 0.f;
	}

	for (EndTileInstance& endTile : m_endTiles)
	{
		endTile.triggered = false;
		endTile.frameIndex = 0;
		endTile.frameTimer = 0.f;
		endTile.completedAnimationCycles = 0;
	}
}

int GAME1_Level::getCheckpointCount() const
{
	return static_cast<int>(m_checkpoints.size());
}

sf::FloatRect GAME1_Level::getCheckpointBounds(int index) const
{
	if (index < 0 || index >= static_cast<int>(m_checkpoints.size()))
		return sf::FloatRect({ 0.f, 0.f }, { 0.f, 0.f });

	const sf::Vector2f anchor = m_checkpoints[index].tilePosition;
	const float doubleTileSize = static_cast<float>(TileSize) * 2.f;

	return sf::FloatRect(
		{ anchor.x, anchor.y - static_cast<float>(TileSize) },
		{ doubleTileSize, doubleTileSize });
}

sf::Vector2f GAME1_Level::getCheckpointSpawnPosition(int index) const
{
	if (index < 0 || index >= static_cast<int>(m_checkpoints.size()))
		return { 0.f, 0.f };

	return m_checkpoints[index].tilePosition;
}

int GAME1_Level::getCheckpointOrderIndex(int index) const
{
	if (index < 0 || index >= static_cast<int>(m_checkpoints.size()))
		return -1;

	return m_checkpoints[index].orderIndex;
}

bool GAME1_Level::isCheckpointTriggered(int index) const
{
	if (index < 0 || index >= static_cast<int>(m_checkpoints.size()))
		return false;

	return m_checkpoints[index].phase != CheckpointPhase::NoFlag;
}

void GAME1_Level::triggerCheckpoint(int index)
{
	if (index < 0 || index >= static_cast<int>(m_checkpoints.size()))
		return;

	CheckpointInstance& checkpoint = m_checkpoints[index];

	if (checkpoint.phase != CheckpointPhase::NoFlag)
		return;

	checkpoint.phase = CheckpointPhase::Activating;
	checkpoint.frameIndex = 0;
	checkpoint.frameTimer = 0.f;
}

int GAME1_Level::getEndTileCount() const
{
	return static_cast<int>(m_endTiles.size());
}

sf::FloatRect GAME1_Level::getEndTileBounds(int index) const
{
	if (index < 0 || index >= static_cast<int>(m_endTiles.size()))
		return sf::FloatRect({ 0.f, 0.f }, { 0.f, 0.f });

	const float doubleTileSize = static_cast<float>(TileSize) * 2.f;
	return sf::FloatRect(
		m_endTiles[static_cast<std::size_t>(index)].tilePosition,
		{ doubleTileSize, doubleTileSize });
}

bool GAME1_Level::isEndTileTriggered(int index) const
{
	if (index < 0 || index >= static_cast<int>(m_endTiles.size()))
		return false;

	return m_endTiles[static_cast<std::size_t>(index)].triggered;
}

bool GAME1_Level::hasEndTileCompletedAnimationCycles(int index, int cycleCount) const
{
	if (index < 0 || index >= static_cast<int>(m_endTiles.size()))
		return true;

	const EndTileInstance& endTile = m_endTiles[static_cast<std::size_t>(index)];

	if (!endTile.triggered)
		return false;

	if (m_endLoopFrames.empty())
		return true;

	return endTile.completedAnimationCycles >= cycleCount;
}

void GAME1_Level::triggerEndTile(int index)
{
	if (index < 0 || index >= static_cast<int>(m_endTiles.size()))
		return;

	EndTileInstance& endTile = m_endTiles[static_cast<std::size_t>(index)];

	if (endTile.triggered)
		return;

	endTile.triggered = true;
	endTile.frameIndex = 0;
	endTile.frameTimer = 0.f;
	endTile.completedAnimationCycles = 0;
}

const sf::Texture* GAME1_Level::getCheckpointCurrentTexture(std::size_t checkpointIndex) const
{
	if (checkpointIndex >= m_checkpoints.size())
		return nullptr;

	const CheckpointInstance& checkpoint = m_checkpoints[checkpointIndex];

	if (checkpoint.phase == CheckpointPhase::NoFlag)
	{
		return m_hasCheckpointNoFlagTexture ? &m_checkpointNoFlagTexture : nullptr;
	}

	if (checkpoint.phase == CheckpointPhase::Activating)
	{
		if (m_checkpointActivationFrames.empty())
			return nullptr;

		const std::size_t safeIndex = std::min(
			checkpoint.frameIndex,
			m_checkpointActivationFrames.size() - 1);

		return &m_checkpointActivationFrames[safeIndex];
	}

	if (m_checkpointLoopFrames.empty())
		return nullptr;

	const std::size_t safeIndex = checkpoint.frameIndex % m_checkpointLoopFrames.size();
	return &m_checkpointLoopFrames[safeIndex];
}

const sf::Texture* GAME1_Level::getStartTileCurrentTexture(std::size_t startIndex) const
{
	if (startIndex >= m_startTiles.size())
		return nullptr;

	const StartTileInstance& startTile = m_startTiles[startIndex];

	if (startTile.phase == StartTilePhase::Moving && !m_startMovingFrames.empty())
	{
		const std::size_t safeIndex =
			std::min(startTile.frameIndex, m_startMovingFrames.size() - 1);
		return &m_startMovingFrames[safeIndex];
	}

	return m_hasStartIdleTexture ? &m_startIdleTexture : nullptr;
}

const sf::Texture* GAME1_Level::getEndTileCurrentTexture(std::size_t endIndex) const
{
	if (endIndex >= m_endTiles.size())
		return nullptr;

	const EndTileInstance& endTile = m_endTiles[endIndex];

	if (endTile.triggered && !m_endLoopFrames.empty())
		return &m_endLoopFrames[endTile.frameIndex % m_endLoopFrames.size()];

	return m_hasEndIdleTexture ? &m_endIdleTexture : nullptr;
}

void GAME1_Level::draw(sf::RenderWindow& window) const
{
	// Background decoration first: above the parallax (drawn separately by the
	// caller) but behind every foreground/solid tile drawn below.
	drawBackgroundTiles(window);

	for (int row = 0; row < static_cast<int>(m_rows.size()); ++row)
	{
		for (int col = 0; col < static_cast<int>(m_rows[row].size()); ++col)
		{
			const char tile = m_rows[row][col];
			const sf::Texture* texture = getTextureForTile(tile);

			if (texture == nullptr)
				continue;

			sf::Sprite sprite(*texture);

			const sf::FloatRect localBounds = sprite.getLocalBounds();
			if (localBounds.size.x <= 0.f || localBounds.size.y <= 0.f)
				continue;

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

	drawTraps(window);

	for (std::size_t i = 0; i < m_checkpoints.size(); ++i)
	{
		const sf::Texture* texture = getCheckpointCurrentTexture(i);

		if (texture == nullptr)
			continue;

		sf::Sprite sprite(*texture);

		const sf::FloatRect localBounds = sprite.getLocalBounds();
		if (localBounds.size.x <= 0.f || localBounds.size.y <= 0.f)
			continue;

		const float doubleTileSize = static_cast<float>(TileSize) * 2.f;

		sprite.setScale({
			doubleTileSize / localBounds.size.x,
			doubleTileSize / localBounds.size.y
			});

		sprite.setPosition({
			m_checkpoints[i].tilePosition.x,
			m_checkpoints[i].tilePosition.y - static_cast<float>(TileSize)
			});

		window.draw(sprite);
	}

	drawGoalTiles(window);
}

bool GAME1_Level::isSolidTile(int col, int row) const
{
	// Fire base tiles are treated as solid floor cells so the player (and
	// enemies) can stand on / collide with the block.  The flame in the
	// forward tile remains a damage-only trigger and is NOT solid.
	const long long key =
		static_cast<long long>(col) * 65536LL + static_cast<long long>(row);
	if (m_fireBaseTileCells.find(key) != m_fireBaseTileCells.end())
		return true;

	if (!isInside(col, row))
		return false;

	const char tile = m_rows[row][col];
	return isFloorTile(tile);
}

bool GAME1_Level::isSpikeTrapTile(int col, int row) const
{
	if (!isInside(col, row))
		return false;

	return isSpikeTrapCode(m_rows[row][col]);
}

bool GAME1_Level::isOneWayPlatformTile(int col, int row) const
{
	if (!isInside(col, row))
		return false;

	return isOneWayPlatformCode(m_rows[row][col]);
}

char GAME1_Level::getTile(int col, int row) const
{
	if (!isInside(col, row))
		return 'O';

	return m_rows[row][col];
}

GAME1_SurfaceTag GAME1_Level::getSurfaceTagForTile(int col, int row) const
{
	if (!isInside(col, row))
		return GAME1_SurfaceTag::Grass;

	const char tile = m_rows[row][col];

	if (isOneWayPlatformCode(tile))
		return GAME1_SurfaceTag::Floor;

	if (isFloorTile(tile))
	{
		// Surface tag follows the logical tile code (stable across all worlds,
		// see KnownTileCodeForStem). Grass-topped tiles keep grass footsteps,
		// Floor_* tiles use the Floor tag, and the Solid* terrain blocks use
		// the Rock tag. Anything else falls back to Grass.
		switch (tile)
		{
		case 'G': case 'H': case 'J': case 'Q': case 'Y':
			return GAME1_SurfaceTag::Grass;

		case 'X': case 'D': case 'Z': case 'C': case 'L': case 'R':
			return GAME1_SurfaceTag::Floor;

		case 'S': case 'T': case 'U': case 'V': case 'W':
		case 'A': case 'E': case 'F': case 'I': case 'M': case 'N':
			return GAME1_SurfaceTag::Rock;

		default:
			return GAME1_SurfaceTag::Grass;
		}
	}

	return GAME1_SurfaceTag::Grass;
}

GAME1_SurfaceTag GAME1_Level::getSurfaceTagAtWorldPosition(sf::Vector2f worldPosition) const
{
	const int col = static_cast<int>(std::floor(worldPosition.x / static_cast<float>(TileSize)));
	const int row = static_cast<int>(std::floor(worldPosition.y / static_cast<float>(TileSize)));

	return getSurfaceTagForTile(col, row);
}

int GAME1_Level::getWorldNumber() const
{
	return m_worldNumber;
}

sf::Vector2f GAME1_Level::getPlayerSpawnPosition() const
{
	return m_playerSpawnPosition;
}

const std::vector<GAME1_LevelEnemySpawn>& GAME1_Level::getEnemySpawns() const
{
	return m_enemySpawns;
}

const std::vector<GAME1_PickupSpawn>& GAME1_Level::getPickupSpawns() const
{
	return m_pickupSpawns;
}

const std::vector<GAME1_TrapSpawn>& GAME1_Level::getTrapSpawns() const
{
	return m_trapSpawns;
}

void GAME1_Level::updateTraps(float deltaTime)
{
	for (GAME1_Trap& trap : m_traps)
	{
		trap.update(deltaTime);

		if (trap.getType() != GAME1_TrapType::FallingPlatform)
			continue;

		// Clamp falling platforms so they cannot pass through solid floor
		// tiles below them.  Without this, a platform falls indefinitely
		// down through the level geometry.
		if (trap.getDelta().y <= 0.f)
			continue;

		const sf::FloatRect bounds = trap.getCollisionBounds();
		if (bounds.size.x <= 0.f || bounds.size.y <= 0.f)
			continue;

		const int leftTile = static_cast<int>(
			std::floor(bounds.position.x / static_cast<float>(TileSize)));
		const int rightTile = static_cast<int>(std::floor(
			(bounds.position.x + bounds.size.x - 0.1f) /
			static_cast<float>(TileSize)));
		const int bottomTile = static_cast<int>(std::floor(
			(bounds.position.y + bounds.size.y - 0.1f) /
			static_cast<float>(TileSize)));

		for (int col = leftTile; col <= rightTile; ++col)
		{
			if (!isSolidTile(col, bottomTile + 1))
				continue;

			const float floorTopY =
				static_cast<float>((bottomTile + 1) * TileSize);
			const float boundsBottomY = bounds.position.y + bounds.size.y;

			if (boundsBottomY <= floorTopY)
				continue;

			const float overshoot = boundsBottomY - floorTopY;
			trap.setPositionY(trap.getPosition().y - overshoot);
			break;
		}
	}
}

std::optional<GAME1_TrapPlatformContact> GAME1_Level::findPlatformContact(
	const sf::FloatRect& playerBounds,
	float previousPlayerBottom) const
{
	// Pick whichever candidate platform has the largest horizontal overlap
	// with the player.  When two falling platforms sit side-by-side and the
	// player straddles them, the platform under the larger share of the
	// player wins — the player can only stand on one platform at a time,
	// so the other one is not marked as occupied and will not start to
	// fall, preventing the jitter that arose when each platform fell at a
	// slightly different cadence.
	std::optional<GAME1_TrapPlatformContact> bestContact;
	float bestOverlap = 0.f;

	const float playerLeft = playerBounds.position.x;
	const float playerRight = playerBounds.position.x + playerBounds.size.x;

	for (std::size_t i = 0; i < m_traps.size(); ++i)
	{
		const std::optional<GAME1_TrapPlatformContact> contact =
			m_traps[i].getPlatformContact(i, playerBounds, previousPlayerBottom);

		if (!contact.has_value())
			continue;

		const float platLeft = contact->bounds.position.x;
		const float platRight = contact->bounds.position.x + contact->bounds.size.x;
		const float overlap =
			std::max(0.f, std::min(playerRight, platRight) - std::max(playerLeft, platLeft));

		if (overlap <= 0.f)
			continue;

		if (!bestContact.has_value() || overlap > bestOverlap)
		{
			bestContact = contact;
			bestOverlap = overlap;
		}
	}

	return bestContact;
}

void GAME1_Level::markTrapPlayerStanding(std::size_t trapIndex)
{
	if (trapIndex >= m_traps.size())
		return;

	m_traps[trapIndex].markPlayerStanding();
}

sf::Vector2f GAME1_Level::getFanForceForBounds(const sf::FloatRect& playerBounds) const
{
	sf::Vector2f force{ 0.f, 0.f };

	for (const GAME1_Trap& trap : m_traps)
	{
		const std::optional<sf::FloatRect> forceBounds = trap.getFanForceBounds();

		if (!forceBounds.has_value())
			continue;

		if (!rectsIntersect(playerBounds, forceBounds.value()))
			continue;

		const sf::Vector2f direction = trap.getFanDirection();
		force.x += direction.x;
		force.y += direction.y;
	}

	const float length = std::sqrt(force.x * force.x + force.y * force.y);
	if (length > 0.001f)
	{
		force.x /= length;
		force.y /= length;
	}

	return force;
}

std::optional<sf::FloatRect> GAME1_Level::getActiveFireDamageBounds(const sf::FloatRect& playerBounds) const
{
	for (const GAME1_Trap& trap : m_traps)
	{
		const std::optional<sf::FloatRect> damageBounds = trap.getFireDamageBounds();

		if (!damageBounds.has_value())
			continue;

		if (rectsIntersect(playerBounds, damageBounds.value()))
			return damageBounds;
	}

	return std::nullopt;
}

int GAME1_Level::getWidthInTiles() const
{
	if (m_rows.empty())
		return 0;

	return static_cast<int>(m_rows[0].size());
}

int GAME1_Level::getHeightInTiles() const
{
	return static_cast<int>(m_rows.size());
}

float GAME1_Level::getPixelWidth() const
{
	return static_cast<float>(getWidthInTiles() * TileSize);
}

float GAME1_Level::getPixelHeight() const
{
	return static_cast<float>(getHeightInTiles() * TileSize);
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

bool GAME1_Level::isFloorTile(char tile) const
{
	return m_floorTextures.find(tile) != m_floorTextures.end();
}

bool GAME1_Level::isSpikeTrapCode(char tile) const
{
	return tile == SpikeTrapTile;
}

bool GAME1_Level::isOneWayPlatformCode(char tile) const
{
	return tile == OneWayPlatformLeftTile ||
		tile == OneWayPlatformMiddleTile ||
		tile == OneWayPlatformRightTile;
}

bool GAME1_Level::isSupportedSpecialTileCode(char tile) const
{
	return isSpikeTrapCode(tile) || isOneWayPlatformCode(tile);
}

void GAME1_Level::rebuildTrapRuntime()
{
	m_traps.clear();
	m_traps.reserve(m_trapSpawns.size());

	for (const GAME1_TrapSpawn& spawn : m_trapSpawns)
		m_traps.emplace_back(spawn);

	for (GAME1_Trap& trap : m_traps)
		trap.setAssets(&m_trapAssets);

	m_fireBaseTileCells.clear();
	for (const GAME1_Trap& trap : m_traps)
	{
		if (trap.getType() != GAME1_TrapType::Fire)
			continue;

		const sf::Vector2i grid = trap.getGridPosition();
		const long long key =
			static_cast<long long>(grid.x) * 65536LL +
			static_cast<long long>(grid.y);
		m_fireBaseTileCells.insert(key);
	}

	configureMovingPlatformPaths();
}

void GAME1_Level::configureMovingPlatformPaths()
{
	for (GAME1_Trap& trap : m_traps)
	{
		if (!trap.isMovingPlatform())
			continue;

		const sf::Vector2i start = trap.getGridPosition();

		if (!hasChainAt(start))
		{
			trap.configureChainPath(start, start, true, false);
			continue;
		}

		sf::Vector2i left = start;
		while (hasChainAt({ left.x - 1, left.y }))
			--left.x;

		sf::Vector2i right = start;
		while (hasChainAt({ right.x + 1, right.y }))
			++right.x;

		if (left.x != right.x)
		{
			trap.configureChainPath(left, right, true, true);
			continue;
		}

		sf::Vector2i top = start;
		while (hasChainAt({ top.x, top.y - 1 }))
			--top.y;

		sf::Vector2i bottom = start;
		while (hasChainAt({ bottom.x, bottom.y + 1 }))
			++bottom.y;

		if (top.y != bottom.y)
		{
			trap.configureChainPath(top, bottom, false, true);
			continue;
		}

		trap.configureChainPath(start, start, true, false);
	}
}

bool GAME1_Level::hasChainAt(sf::Vector2i gridPosition) const
{
	for (const GAME1_Trap& trap : m_traps)
	{
		if (trap.isChain() && trap.getGridPosition() == gridPosition)
			return true;
	}

	return false;
}

void GAME1_Level::drawBackgroundTiles(sf::RenderWindow& window) const
{
	for (int row = 0; row < static_cast<int>(m_bgRows.size()); ++row)
	{
		for (int col = 0; col < static_cast<int>(m_bgRows[row].size()); ++col)
		{
			const char tile = m_bgRows[row][col];

			if (tile == 'O')
				continue;

			const auto found = m_backgroundTileTextures.find(tile);
			if (found == m_backgroundTileTextures.end())
				continue;

			sf::Sprite sprite(found->second);

			const sf::FloatRect localBounds = sprite.getLocalBounds();
			if (localBounds.size.x <= 0.f || localBounds.size.y <= 0.f)
				continue;

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

void GAME1_Level::drawTraps(sf::RenderWindow& window) const
{
	for (const GAME1_Trap& trap : m_traps)
	{
		if (trap.isChain())
			trap.draw(window, m_trapAssets);
	}

	for (const GAME1_Trap& trap : m_traps)
	{
		if (!trap.isChain())
			trap.draw(window, m_trapAssets);
	}
}

void GAME1_Level::drawGoalTiles(sf::RenderWindow& window) const
{
	const float doubleTileSize = static_cast<float>(TileSize) * 2.f;

	auto drawGoalSprite = [&](const sf::Texture* texture,
		sf::Vector2f position,
		sf::Color fallbackColor)
		{
			const sf::FloatRect bounds(position, { doubleTileSize, doubleTileSize });

			if (texture != nullptr)
			{
				sf::Sprite sprite(*texture);
				const sf::FloatRect localBounds = sprite.getLocalBounds();

				if (localBounds.size.x > 0.f && localBounds.size.y > 0.f)
				{
					sprite.setScale({
						bounds.size.x / localBounds.size.x,
						bounds.size.y / localBounds.size.y
						});
					sprite.setPosition(bounds.position);
					window.draw(sprite);
					return;
				}
			}

			sf::RectangleShape fallback;
			fallback.setPosition(bounds.position);
			fallback.setSize(bounds.size);
			fallback.setFillColor(fallbackColor);
			fallback.setOutlineColor(sf::Color::White);
			fallback.setOutlineThickness(2.f);
			window.draw(fallback);
		};

	for (std::size_t i = 0; i < m_startTiles.size(); ++i)
	{
		drawGoalSprite(
			getStartTileCurrentTexture(i),
			m_startTiles[i].tilePosition,
			sf::Color(70, 150, 255, 210));
	}

	for (std::size_t i = 0; i < m_endTiles.size(); ++i)
	{
		drawGoalSprite(
			getEndTileCurrentTexture(i),
			m_endTiles[i].tilePosition,
			sf::Color(60, 210, 110, 210));
	}
}

bool GAME1_Level::rectsIntersect(const sf::FloatRect& a, const sf::FloatRect& b)
{
	return a.position.x < b.position.x + b.size.x &&
		a.position.x + a.size.x > b.position.x &&
		a.position.y < b.position.y + b.size.y &&
		a.position.y + a.size.y > b.position.y;
}

const sf::Texture* GAME1_Level::getTextureForTile(char tile) const
{
	const auto floorFound = m_floorTextures.find(tile);
	if (floorFound != m_floorTextures.end())
		return &floorFound->second;

	const auto specialFound = m_specialTileTextures.find(tile);
	if (specialFound != m_specialTileTextures.end())
		return &specialFound->second;

	return nullptr;
}
