#include "GAME1_Level.h"

#include <algorithm>
#include <cctype>
#include <fstream>
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

		return std::nullopt;
	}

	std::vector<char> FallbackTileCodes()
	{
		// P is reserved for PlayerSpawn, O is empty, and special trap/platform
		// characters are handled separately.
		return
		{
			'X', 'A', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
			'N', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'Y', 'Z'
		};
	}

	bool IsReservedMapTileCode(char code)
	{
		return code == 'O' ||
			code == 'B' ||
			code == 'P' ||
			code == GAME1_Level::SpikeTrapTile ||
			code == GAME1_Level::OneWayPlatformLeftTile ||
			code == GAME1_Level::OneWayPlatformMiddleTile ||
			code == GAME1_Level::OneWayPlatformRightTile;
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
	m_lastError.clear();
	m_floorTextures.clear();
	m_specialTileTextures.clear();
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
	std::string line;
	std::size_t expectedWidth = 0;

	while (std::getline(file, line))
	{
		if (!line.empty() && line.back() == '\r')
			line.pop_back();

		if (line.empty())
			continue;

		if (TryParseWorldMetadata(line, m_worldNumber))
			continue;

		if (!line.empty() && line[0] == '#')
			continue;

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

	loadSpecialTileTextures(resourcesPath);

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

			if (tile == 'O' || isFloorTile(tile) || isSupportedSpecialTileCode(tile))
				continue;

			m_lastError =
				"Map error: unsupported tile character '" +
				std::string(1, tile) +
				"' at row " + std::to_string(row + 1) +
				", column " + std::to_string(col + 1) +
				". Use O for empty, P for player spawn, a floor tile letter from the editor, ^ for spikes, or [/= /] for one-way platforms.";
			return false;
		}
	}

	m_rows = std::move(rawRows);
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
	// Common layouts supported:
	// Resources/Tiles/Platforms/Platform_0.png, _1, _2
	// Resources/Tiles/Platform/Platform_0.png, _1, _2
	// Any PNG under Tiles with platform / oneway / one_way in the file/folder name.
	const std::vector<fs::path> platformPaths = FindSpecialPngFiles(
		tilesDirectory,
		{ "platform", "oneway", "one_way", "one-way" });

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

void GAME1_Level::draw(sf::RenderWindow& window) const
{
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
}

bool GAME1_Level::isSolidTile(int col, int row) const
{
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

int GAME1_Level::getWorldNumber() const
{
	return m_worldNumber;
}

sf::Vector2f GAME1_Level::getPlayerSpawnPosition() const
{
	return m_playerSpawnPosition;
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
