#include "GAME1_Level.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <optional>
#include <random>
#include <sstream>
#include <unordered_set>
#include <utility>

namespace
{
	bool RectsIntersect(const sf::FloatRect& a, const sf::FloatRect& b)
	{
		return a.position.x < (b.position.x + b.size.x) &&
			(a.position.x + a.size.x) > b.position.x &&
			a.position.y < (b.position.y + b.size.y) &&
			(a.position.y + a.size.y) > b.position.y;
	}

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
		// P is reserved for PlayerSpawn, B for breakable, and O for empty.
		return
		{
			'X', 'A', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
			'N', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'Y', 'Z'
		};
	}

	bool IsReservedMapTileCode(char code)
	{
		return code == 'O' || code == 'B' || code == 'P';
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
	const std::string& breakTexturePath)
{
	const std::filesystem::path inferredResourcesDirectory = InferResourcesDirectoryFromOldFloorPath(floorTexturePath);
	return loadFromFileInternal(mapPath, inferredResourcesDirectory.string(), breakTexturePath);
}

bool GAME1_Level::loadFromFileInternal(const std::string& mapPath,
	const std::string& resourcesDirectory,
	const std::string& optionalBreakTexturePath)
{
	namespace fs = std::filesystem;

	m_rows.clear();
	m_lastError.clear();
	m_floorTextures.clear();
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

	if (!loadBreakTexture(resourcesPath, optionalBreakTexturePath))
		return false;

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

			if (tile == 'O' || tile == 'B' || isFloorTile(tile))
				continue;

			m_lastError =
				"Map error: unsupported tile character '" +
				std::string(1, tile) +
				"' at row " + std::to_string(row + 1) +
				", column " + std::to_string(col + 1) +
				". Use O for empty, B for breakable, P for player spawn, or a floor tile letter from the editor.";
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

bool GAME1_Level::loadBreakTexture(const std::filesystem::path& resourcesDirectory,
	const std::string& optionalBreakTexturePath)
{
	namespace fs = std::filesystem;

	std::vector<fs::path> candidatePaths;

	if (!optionalBreakTexturePath.empty())
		candidatePaths.push_back(optionalBreakTexturePath);

	candidatePaths.push_back(resourcesDirectory / "breakblock.png");
	candidatePaths.push_back(resourcesDirectory / "BreakBlock.png");
	candidatePaths.push_back(resourcesDirectory / "Tiles" / "breakblock.png");
	candidatePaths.push_back(resourcesDirectory / "Tiles" / "BreakBlock.png");

	for (const fs::path& path : candidatePaths)
	{
		if (m_breakTexture.loadFromFile(path.string()))
			return true;
	}

	m_lastError =
		"Failed to load break block texture. Tried old path plus Resources/breakblock.png and Resources/Tiles/breakblock.png.";
	return false;
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
	return tile == 'B' || isFloorTile(tile);
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
		m_rows[row][col] = 'O';
}

void GAME1_Level::spawnRandomBreakBlocks(int count, const sf::FloatRect& forbiddenArea)
{
	std::vector<std::pair<int, int>> candidates;

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
				candidates.emplace_back(col, row);
		}
	}

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

const sf::Texture* GAME1_Level::getTextureForTile(char tile) const
{
	if (tile == 'B')
		return &m_breakTexture;

	const auto found = m_floorTextures.find(tile);
	if (found != m_floorTextures.end())
		return &found->second;

	return nullptr;
}
