#include "BombermanLevel.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <optional>

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
		if (!path.has_extension())
			return false;

		return ToLower(path.extension().string()) == ".png";
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
		{
			--start;
		}

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
}

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

	m_breakableFrames.clear();
	m_breakableCurrentFrame = 0;
	m_breakableAnimationTimer = 0.f;

	const fs::path resourcesPath(resourcesDirectory);
	const fs::path tilesPath = resourcesPath / "Tiles";

	if (!loadTexture(m_floorTexture, (tilesPath / "floor.png").string(), "floor"))
		return false;

	if (!loadAnimationFramesFromDirectory(
		m_breakableFrames,
		(tilesPath / "Breakable").string(),
		"breakable block animation"))
	{
		return false;
	}

	if (!loadTexture(m_solidBlockTexture, (tilesPath / "solidblock.png").string(), "solid block"))
		return false;

	if (!loadTexture(m_wallUpTexture, (tilesPath / "solidwall_up.png").string(), "solid wall up"))
		return false;

	if (!loadTexture(m_wallDownTexture, (tilesPath / "solidwall_down.png").string(), "solid wall down"))
		return false;

	if (!loadTexture(m_wallLeftTexture, (tilesPath / "solidwall_left.png").string(), "solid wall left"))
		return false;

	if (!loadTexture(m_wallRightTexture, (tilesPath / "solidwall_right.png").string(), "solid wall right"))
		return false;

	if (!loadTexture(m_wallTopTexture, (tilesPath / "solidwall_top.png").string(), "solid wall top"))
		return false;

	if (!loadTexture(m_wallTopLeftTexture, (tilesPath / "solidwall_topleft.png").string(), "solid wall top-left"))
		return false;

	if (!loadTexture(m_wallTopRightTexture, (tilesPath / "solidwall_topright.png").string(), "solid wall top-right"))
		return false;

	if (!loadTexture(m_wallBotLeftTexture, (tilesPath / "solidwall_botleft.png").string(), "solid wall bottom-left"))
		return false;

	if (!loadTexture(m_wallBotRightTexture, (tilesPath / "solidwall_botright.png").string(), "solid wall bottom-right"))
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
			char tile = paddedLine[col];

			if (tile == 'X')
			{
				paddedLine[col] = 'T';
				tile = 'T';
			}

			if (isSolidWallCharacter(tile) || tile == 'B' || tile == ' ')
			{
				continue;
			}

			switch (tile)
			{
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
				paddedLine[col] = ' ';
				break;

			default:
				m_lastError =
					"Bomberman map error: unsupported character '" +
					std::string(1, tile) +
					"'. Allowed wall chars: M U D L R T Q Y Z C. Also allowed: X legacy, B, P, O, E legacy, and space.";
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

void BombermanLevel::updateAnimations(float deltaTime)
{
	if (m_breakableFrames.size() <= 1)
		return;

	m_breakableAnimationTimer += deltaTime;

	while (m_breakableAnimationTimer >= m_breakableFrameDuration)
	{
		m_breakableAnimationTimer -= m_breakableFrameDuration;
		m_breakableCurrentFrame = (m_breakableCurrentFrame + 1) % m_breakableFrames.size();
	}
}

void BombermanLevel::generateRandomBreakableBlocks(std::mt19937& rng,
	int minimumBlocks,
	float maxEmptyTileRatio,
	const std::vector<BombermanGridPosition>& forbiddenPositions)
{
	std::vector<BombermanGridPosition> candidates;

	for (int row = 0; row < getHeightInTiles(); ++row)
	{
		for (int col = 0; col < getWidthInTiles(); ++col)
		{
			if (!isInside(col, row))
				continue;

			if (m_rows[row][col] != ' ')
				continue;

			if (isForbiddenBreakablePosition(col, row, forbiddenPositions))
				continue;

			candidates.push_back({ col, row });
		}
	}

	if (candidates.empty())
		return;

	std::shuffle(candidates.begin(), candidates.end(), rng);

	const int availableEmptyTiles = static_cast<int>(candidates.size());

	const int minimumPossible = std::min(std::max(0, minimumBlocks), availableEmptyTiles);

	int maximumFromRatio = static_cast<int>(static_cast<float>(availableEmptyTiles) * maxEmptyTileRatio);

	// If 20% of the available empty tiles is lower than the requested minimum,
	// we still allow the minimum so small maps can function.
	maximumFromRatio = std::max(maximumFromRatio, minimumPossible);
	maximumFromRatio = std::clamp(maximumFromRatio, minimumPossible, availableEmptyTiles);

	std::uniform_int_distribution<int> countDistribution(minimumPossible, maximumFromRatio);
	const int blocksToPlace = countDistribution(rng);

	for (int i = 0; i < blocksToPlace; ++i)
	{
		const BombermanGridPosition position = candidates[i];

		if (isInside(position.col, position.row) && m_rows[position.row][position.col] == ' ')
		{
			m_rows[position.row][position.col] = 'B';
		}
	}
}

bool BombermanLevel::isForbiddenBreakablePosition(int col,
	int row,
	const std::vector<BombermanGridPosition>& forbiddenPositions) const
{
	for (const BombermanGridPosition& forbiddenPosition : forbiddenPositions)
	{
		if (forbiddenPosition.col == col && forbiddenPosition.row == row)
			return true;
	}

	return false;
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

bool BombermanLevel::loadAnimationFramesFromDirectory(std::vector<sf::Texture>& frames,
	const std::string& directoryPath,
	const std::string& readableName)
{
	namespace fs = std::filesystem;

	frames.clear();

	const fs::path directory(directoryPath);

	if (!fs::exists(directory) || !fs::is_directory(directory))
	{
		m_lastError = "Failed to load Bomberman " + readableName + ": folder does not exist: " + directoryPath;
		return false;
	}

	std::vector<fs::path> framePaths;

	for (const auto& entry : fs::directory_iterator(directory))
	{
		if (!entry.is_regular_file())
			continue;

		if (!IsPngFile(entry.path()))
			continue;

		framePaths.push_back(entry.path());
	}

	std::sort(framePaths.begin(), framePaths.end(), NaturalFrameSort);

	if (framePaths.empty())
	{
		m_lastError = "Failed to load Bomberman " + readableName + ": no PNG frames found in: " + directoryPath;
		return false;
	}

	frames.reserve(framePaths.size());

	for (const fs::path& framePath : framePaths)
	{
		sf::Texture texture;

		if (!texture.loadFromFile(framePath.string()))
		{
			m_lastError = "Failed to load Bomberman " + readableName + " frame: " + framePath.string();
			return false;
		}

		frames.push_back(std::move(texture));
	}

	return true;
}

bool BombermanLevel::isSolidWallCharacter(char tile) const
{
	switch (tile)
	{
	case 'M':
	case 'U':
	case 'D':
	case 'L':
	case 'R':
	case 'T':
	case 'Q':
	case 'Y':
	case 'Z':
	case 'C':
		return true;

	default:
		return false;
	}
}

const sf::Texture* BombermanLevel::getWallTextureForTile(char tile) const
{
	switch (tile)
	{
	case 'M':
		return &m_solidBlockTexture;

	case 'U':
		return &m_wallUpTexture;

	case 'D':
		return &m_wallDownTexture;

	case 'L':
		return &m_wallLeftTexture;

	case 'R':
		return &m_wallRightTexture;

	case 'T':
		return &m_wallTopTexture;

	case 'Q':
		return &m_wallTopLeftTexture;

	case 'Y':
		return &m_wallTopRightTexture;

	case 'Z':
		return &m_wallBotLeftTexture;

	case 'C':
		return &m_wallBotRightTexture;

	default:
		return nullptr;
	}
}

const sf::Texture* BombermanLevel::getCurrentBreakableTexture() const
{
	if (m_breakableFrames.empty())
		return nullptr;

	return &m_breakableFrames[m_breakableCurrentFrame % m_breakableFrames.size()];
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

void BombermanLevel::drawFloorLayer(sf::RenderTarget& target) const
{
	for (int row = 0; row < getHeightInTiles(); ++row)
	{
		for (int col = 0; col < getWidthInTiles(); ++col)
		{
			drawTextureInTile(target, m_floorTexture, col, row);
		}
	}
}

void BombermanLevel::drawWorldTileAt(sf::RenderTarget& target, int col, int row) const
{
	if (!isInside(col, row))
		return;

	const char tile = m_rows[row][col];

	if (const sf::Texture* wallTexture = getWallTextureForTile(tile))
	{
		drawTextureInTile(target, *wallTexture, col, row);
	}
	else if (tile == 'B')
	{
		if (const sf::Texture* breakableTexture = getCurrentBreakableTexture())
		{
			drawTextureInTile(target, *breakableTexture, col, row);
		}
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

void BombermanLevel::drawBaseLayer(sf::RenderTarget& target, bool includeSolidWalls) const
{
	drawFloorLayer(target);

	for (int row = 0; row < getHeightInTiles(); ++row)
	{
		for (int col = 0; col < getWidthInTiles(); ++col)
		{
			if (!includeSolidWalls && isWall(col, row))
				continue;

			drawWorldTileAt(target, col, row);
		}
	}
}

void BombermanLevel::drawSolidWallsOnly(sf::RenderTarget& target) const
{
	for (int row = 0; row < getHeightInTiles(); ++row)
	{
		for (int col = 0; col < getWidthInTiles(); ++col)
		{
			if (isWall(col, row))
			{
				drawWorldTileAt(target, col, row);
			}
		}
	}
}

void BombermanLevel::drawSolidWallAt(sf::RenderTarget& target, int col, int row) const
{
	if (!isWall(col, row))
		return;

	drawWorldTileAt(target, col, row);
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

	return isSolidWallCharacter(m_rows[row][col]);
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

	return isSolidWallCharacter(tile) || tile == 'B';
}

bool BombermanLevel::canExplosionPassThrough(int col, int row) const
{
	if (!isInside(col, row))
		return false;

	return !isSolidWallCharacter(m_rows[row][col]);
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

bool BombermanLevel::revealExitAt(int col, int row)
{
	if (!isInside(col, row))
		return false;

	m_rows[row][col] = 'E';

	m_hasExit = true;
	m_exitPosition = { col, row };

	return true;
}

std::vector<BombermanGridPosition> BombermanLevel::getBreakableBlockPositions() const
{
	std::vector<BombermanGridPosition> positions;

	for (int row = 0; row < getHeightInTiles(); ++row)
	{
		for (int col = 0; col < getWidthInTiles(); ++col)
		{
			if (isBreakableBlock(col, row))
			{
				positions.push_back({ col, row });
			}
		}
	}

	return positions;
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