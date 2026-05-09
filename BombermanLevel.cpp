#include "BombermanLevel.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>

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
			{
				outWorldNumber = parsedWorld;
			}
		}
		catch (...)
		{
		}

		return true;
	}

	std::filesystem::path GetWorldAnimationDirectory(
		const std::filesystem::path& tilesPath,
		const std::string& baseFolderName,
		int worldNumber)
	{
		if (worldNumber <= 1)
			return tilesPath / baseFolderName;

		return tilesPath / (baseFolderName + std::to_string(worldNumber));
	}
}

bool BombermanLevel::loadFromFile(const std::string& mapPath, const std::string& resourcesDirectory)
{
	namespace fs = std::filesystem;

	m_lastError.clear();
	m_rows.clear();
	m_enemySpawns.clear();
	m_enemySpawnEntries.clear();
	m_playerSpawn = { 1, 1 };
	m_hasExit = false;
	m_exitPosition = { 0, 0 };
	m_worldNumber = 1;

	m_wallTextures.clear();

	m_exitFrames.clear();
	m_exitCurrentFrame = 0;
	m_exitAnimationTimer = 0.f;

	m_breakableFrames.clear();
	m_breakableCurrentFrame = 0;
	m_breakableAnimationTimer = 0.f;

	m_brokenFrames.clear();
	m_activeBrokenBlocks.clear();
	m_completedBrokenBlocks.clear();

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

		if (TryParseWorldMetadata(line, m_worldNumber))
			continue;

		if (!line.empty() && line[0] == '#')
			continue;

		widestLine = std::max(widestLine, line.size());
		rawLines.push_back(line);
	}

	if (rawLines.empty())
	{
		m_lastError = "Bomberman map file is empty: " + mapPath;
		return false;
	}

	const fs::path resourcesPath(resourcesDirectory);
	const fs::path tilesPath = resourcesPath / "Tiles";

	if (!loadWorldTileTextures(resourcesDirectory))
		return false;

	if (!loadAnimationFramesFromDirectory(m_exitFrames, (tilesPath / "Exit").string(), "exit animation"))
		return false;

	const fs::path breakableDirectory = GetWorldAnimationDirectory(tilesPath, "Breakable", m_worldNumber);
	const fs::path brokenDirectory = GetWorldAnimationDirectory(tilesPath, "Broken", m_worldNumber);

	if (!loadAnimationFramesFromDirectory(
		m_breakableFrames,
		breakableDirectory.string(),
		"world " + std::to_string(m_worldNumber) + " breakable block animation"))
	{
		return false;
	}

	if (!loadAnimationFramesFromDirectory(
		m_brokenFrames,
		brokenDirectory.string(),
		"world " + std::to_string(m_worldNumber) + " broken block animation"))
	{
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
				continue;

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
			{
				const BombermanGridPosition spawn{
					static_cast<int>(col),
					static_cast<int>(row)
				};

				m_enemySpawns.push_back(spawn);
				m_enemySpawnEntries.push_back({ BombermanEnemyType::Copter, spawn });
				paddedLine[col] = ' ';
				break;
			}

			case 'A':
			{
				const BombermanGridPosition spawn{
					static_cast<int>(col),
					static_cast<int>(row)
				};

				m_enemySpawns.push_back(spawn);
				m_enemySpawnEntries.push_back({ BombermanEnemyType::Lamp, spawn });
				paddedLine[col] = ' ';
				break;
			}

			case 't':
			{
				const BombermanGridPosition spawn{
					static_cast<int>(col),
					static_cast<int>(row)
				};

				m_enemySpawns.push_back(spawn);
				m_enemySpawnEntries.push_back({ BombermanEnemyType::Tree, spawn });
				paddedLine[col] = ' ';
				break;
			}

			case 'k':
			{
				const BombermanGridPosition spawn{
					static_cast<int>(col),
					static_cast<int>(row)
				};

				m_enemySpawns.push_back(spawn);
				m_enemySpawnEntries.push_back({ BombermanEnemyType::Bomber, spawn });
				paddedLine[col] = ' ';
				break;
			}

			case 'E':
				paddedLine[col] = ' ';
				break;

			default:
				m_lastError =
					"Bomberman map error: unsupported character '" +
					std::string(1, tile) +
					"'. Allowed wall chars include: M S U D L R T Q Y Z C F G H I J N V W. Also allowed: X legacy, B, P, O Copter, A Lamp, t Tree, k Bomber, E legacy, and space.";
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

bool BombermanLevel::loadWorldTileTextures(const std::string& resourcesDirectory)
{
	namespace fs = std::filesystem;

	const fs::path tilesPath = fs::path(resourcesDirectory) / "Tiles";

	fs::path worldTilesPath = tilesPath / ("World" + std::to_string(m_worldNumber));

	if (!fs::exists(worldTilesPath) || !fs::is_directory(worldTilesPath))
	{
		worldTilesPath = tilesPath;
	}

	if (!loadTexture(m_floorTexture, (worldTilesPath / "floor.png").string(), "world floor"))
		return false;

	if (m_worldNumber == 3)
	{
		if (!loadWorldWallTexture('M', (worldTilesPath / "solidblock_0.png").string(), "world 3 solid block")) return false;

		if (!loadWorldWallTexture('S', (worldTilesPath / "solidwall_bot.png").string(), "world 3 bottom wall")) return false;
		if (!loadWorldWallTexture('T', (worldTilesPath / "solidwall_top.png").string(), "world 3 top wall")) return false;
		if (!loadWorldWallTexture('L', (worldTilesPath / "solidwall_left.png").string(), "world 3 left wall")) return false;
		if (!loadWorldWallTexture('R', (worldTilesPath / "solidwall_right.png").string(), "world 3 right wall")) return false;

		if (!loadWorldWallTexture('Q', (worldTilesPath / "solidwall_topleft.png").string(), "world 3 top-left wall")) return false;
		if (!loadWorldWallTexture('Y', (worldTilesPath / "solidwall_topright.png").string(), "world 3 top-right wall")) return false;
		if (!loadWorldWallTexture('Z', (worldTilesPath / "solidwall_botleft.png").string(), "world 3 bottom-left wall")) return false;
		if (!loadWorldWallTexture('C', (worldTilesPath / "solidwall_botright.png").string(), "world 3 bottom-right wall")) return false;

		if (!loadWorldWallTexture('U', (worldTilesPath / "solidwall_backleft_0.png").string(), "world 3 back-left wall 0")) return false;
		if (!loadWorldWallTexture('D', (worldTilesPath / "solidwall_backright_0.png").string(), "world 3 back-right wall 0")) return false;

		if (!loadWorldWallTexture('F', (worldTilesPath / "solidwall_backleft_1.png").string(), "world 3 back-left wall 1")) return false;
		if (!loadWorldWallTexture('G', (worldTilesPath / "solidwall_backleft_2.png").string(), "world 3 back-left wall 2")) return false;
		if (!loadWorldWallTexture('H', (worldTilesPath / "solidwall_backleft_3.png").string(), "world 3 back-left wall 3")) return false;
		if (!loadWorldWallTexture('I', (worldTilesPath / "solidwall_backleft_4.png").string(), "world 3 back-left wall 4")) return false;

		if (!loadWorldWallTexture('J', (worldTilesPath / "solidwall_backright_1.png").string(), "world 3 back-right wall 1")) return false;
		if (!loadWorldWallTexture('N', (worldTilesPath / "solidwall_backright_2.png").string(), "world 3 back-right wall 2")) return false;
		if (!loadWorldWallTexture('V', (worldTilesPath / "solidwall_backright_3.png").string(), "world 3 back-right wall 3")) return false;
		if (!loadWorldWallTexture('W', (worldTilesPath / "solidwall_backright_4.png").string(), "world 3 back-right wall 4")) return false;

		return true;
	}

	if (m_worldNumber == 2)
	{
		if (!loadWorldWallTexture('M', (worldTilesPath / "solidblock.png").string(), "world 2 solid block")) return false;
		if (!loadWorldWallTexture('S', (worldTilesPath / "solidwall_bot.png").string(), "world 2 bottom wall")) return false;
		if (!loadWorldWallTexture('T', (worldTilesPath / "solidwall_top.png").string(), "world 2 top wall")) return false;

		if (!loadWorldWallTexture('Q', (worldTilesPath / "solidwall_topleft_0.png").string(), "world 2 top-left wall")) return false;
		if (!loadWorldWallTexture('Y', (worldTilesPath / "solidwall_topright_0.png").string(), "world 2 top-right wall")) return false;

		if (!loadWorldWallTexture('Z', (worldTilesPath / "solidwall_botleft_0.png").string(), "world 2 bottom-left wall 0")) return false;
		if (!loadWorldWallTexture('C', (worldTilesPath / "solidwall_botright_0.png").string(), "world 2 bottom-right wall 0")) return false;

		if (!loadWorldWallTexture('L', (worldTilesPath / "solidwall_left_0.png").string(), "world 2 left wall 0")) return false;
		if (!loadWorldWallTexture('R', (worldTilesPath / "solidwall_right_0.png").string(), "world 2 right wall 0")) return false;

		if (!loadWorldWallTexture('U', (worldTilesPath / "solidwall_backleft_0.png").string(), "world 2 back-left wall 0")) return false;
		if (!loadWorldWallTexture('D', (worldTilesPath / "solidwall_backright_0.png").string(), "world 2 back-right wall 0")) return false;

		if (!loadWorldWallTexture('F', (worldTilesPath / "solidwall_left_1.png").string(), "world 2 left wall 1")) return false;
		if (!loadWorldWallTexture('G', (worldTilesPath / "solidwall_left_2.png").string(), "world 2 left wall 2")) return false;

		if (!loadWorldWallTexture('H', (worldTilesPath / "solidwall_right_1.png").string(), "world 2 right wall 1")) return false;
		if (!loadWorldWallTexture('I', (worldTilesPath / "solidwall_right_2.png").string(), "world 2 right wall 2")) return false;

		if (!loadWorldWallTexture('J', (worldTilesPath / "solidwall_backleft_1.png").string(), "world 2 back-left wall 1")) return false;
		if (!loadWorldWallTexture('N', (worldTilesPath / "solidwall_backright_1.png").string(), "world 2 back-right wall 1")) return false;

		if (!loadWorldWallTexture('V', (worldTilesPath / "solidwall_botleft_1.png").string(), "world 2 bottom-left wall 1")) return false;
		if (!loadWorldWallTexture('W', (worldTilesPath / "solidwall_botright_1.png").string(), "world 2 bottom-right wall 1")) return false;

		return true;
	}

	if (!loadWorldWallTexture('M', (worldTilesPath / "solidblock.png").string(), "solid block")) return false;
	if (!loadWorldWallTexture('U', (worldTilesPath / "solidwall_up.png").string(), "solid wall up")) return false;
	if (!loadWorldWallTexture('D', (worldTilesPath / "solidwall_down.png").string(), "solid wall down")) return false;
	if (!loadWorldWallTexture('L', (worldTilesPath / "solidwall_left.png").string(), "solid wall left")) return false;
	if (!loadWorldWallTexture('R', (worldTilesPath / "solidwall_right.png").string(), "solid wall right")) return false;
	if (!loadWorldWallTexture('T', (worldTilesPath / "solidwall_top.png").string(), "solid wall top")) return false;
	if (!loadWorldWallTexture('Q', (worldTilesPath / "solidwall_topleft.png").string(), "solid wall top-left")) return false;
	if (!loadWorldWallTexture('Y', (worldTilesPath / "solidwall_topright.png").string(), "solid wall top-right")) return false;
	if (!loadWorldWallTexture('Z', (worldTilesPath / "solidwall_botleft.png").string(), "solid wall bottom-left")) return false;
	if (!loadWorldWallTexture('C', (worldTilesPath / "solidwall_botright.png").string(), "solid wall bottom-right")) return false;

	return true;
}

bool BombermanLevel::loadWorldWallTexture(char tile, const std::string& texturePath, const std::string& readableName)
{
	sf::Texture texture;

	if (!loadTexture(texture, texturePath, readableName))
		return false;

	m_wallTextures[tile] = std::move(texture);
	return true;
}

void BombermanLevel::updateAnimations(float deltaTime)
{
	if (m_exitFrames.size() > 1)
	{
		m_exitAnimationTimer += deltaTime;

		while (m_exitAnimationTimer >= m_exitFrameDuration)
		{
			m_exitAnimationTimer -= m_exitFrameDuration;
			m_exitCurrentFrame = (m_exitCurrentFrame + 1) % m_exitFrames.size();
		}
	}

	if (m_breakableFrames.size() > 1)
	{
		m_breakableAnimationTimer += deltaTime;

		while (m_breakableAnimationTimer >= m_breakableFrameDuration)
		{
			m_breakableAnimationTimer -= m_breakableFrameDuration;
			m_breakableCurrentFrame = (m_breakableCurrentFrame + 1) % m_breakableFrames.size();
		}
	}

	if (!m_activeBrokenBlocks.empty())
	{
		const float brokenDuration = getBrokenAnimationDuration();

		for (ActiveBrokenBlock& brokenBlock : m_activeBrokenBlocks)
			brokenBlock.timer += deltaTime;

		m_activeBrokenBlocks.erase(
			std::remove_if(
				m_activeBrokenBlocks.begin(),
				m_activeBrokenBlocks.end(),
				[this, brokenDuration](const ActiveBrokenBlock& brokenBlock)
				{
					if (brokenBlock.timer >= brokenDuration)
					{
						const BombermanGridPosition position = brokenBlock.gridPosition;

						if (isInside(position.col, position.row) &&
							m_rows[position.row][position.col] == 'K')
						{
							m_rows[position.row][position.col] = ' ';
						}

						m_completedBrokenBlocks.push_back(position);
						return true;
					}

					return false;
				}),
			m_activeBrokenBlocks.end());
	}
}

std::vector<BombermanGridPosition> BombermanLevel::consumeCompletedBrokenBlocks()
{
	std::vector<BombermanGridPosition> completed = std::move(m_completedBrokenBlocks);
	m_completedBrokenBlocks.clear();
	return completed;
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
	maximumFromRatio = std::max(maximumFromRatio, minimumPossible);
	maximumFromRatio = std::clamp(maximumFromRatio, minimumPossible, availableEmptyTiles);

	std::uniform_int_distribution<int> countDistribution(minimumPossible, maximumFromRatio);
	const int blocksToPlace = countDistribution(rng);

	for (int i = 0; i < blocksToPlace; ++i)
	{
		const BombermanGridPosition position = candidates[i];

		if (isInside(position.col, position.row) && m_rows[position.row][position.col] == ' ')
			m_rows[position.row][position.col] = 'B';
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
	case 'S':
	case 'U':
	case 'D':
	case 'L':
	case 'R':
	case 'T':
	case 'Q':
	case 'Y':
	case 'Z':
	case 'C':
	case 'F':
	case 'G':
	case 'H':
	case 'I':
	case 'J':
	case 'N':
	case 'V':
	case 'W':
		return true;

	default:
		return false;
	}
}

const sf::Texture* BombermanLevel::getWallTextureForTile(char tile) const
{
	const auto found = m_wallTextures.find(tile);

	if (found == m_wallTextures.end())
		return nullptr;

	return &found->second;
}

const sf::Texture* BombermanLevel::getCurrentBreakableTexture() const
{
	if (m_breakableFrames.empty())
		return nullptr;

	return &m_breakableFrames[m_breakableCurrentFrame % m_breakableFrames.size()];
}

const sf::Texture* BombermanLevel::getCurrentExitTexture() const
{
	if (m_exitFrames.empty())
		return nullptr;

	return &m_exitFrames[m_exitCurrentFrame % m_exitFrames.size()];
}

const BombermanLevel::ActiveBrokenBlock* BombermanLevel::findActiveBrokenBlock(int col, int row) const
{
	for (const ActiveBrokenBlock& brokenBlock : m_activeBrokenBlocks)
	{
		if (brokenBlock.gridPosition.col == col && brokenBlock.gridPosition.row == row)
			return &brokenBlock;
	}

	return nullptr;
}

std::size_t BombermanLevel::getBrokenAnimationFrameIndex(const ActiveBrokenBlock& brokenBlock) const
{
	if (m_brokenFrames.empty())
		return 0;

	if (m_brokenFrames.size() == 1)
		return 0;

	std::size_t frameIndex = static_cast<std::size_t>(brokenBlock.timer / m_brokenFrameDuration);

	if (frameIndex >= m_brokenFrames.size())
		frameIndex = m_brokenFrames.size() - 1;

	return frameIndex;
}

float BombermanLevel::getBrokenAnimationDuration() const
{
	if (m_brokenFrames.empty())
		return 0.f;

	return static_cast<float>(m_brokenFrames.size()) * m_brokenFrameDuration;
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
			drawTextureInTile(target, m_floorTexture, col, row);
	}

	if (m_hasExit)
	{
		const sf::Texture* exitTexture = getCurrentExitTexture();

		if (exitTexture != nullptr && isInside(m_exitPosition.col, m_exitPosition.row))
			drawTextureInTile(target, *exitTexture, m_exitPosition.col, m_exitPosition.row);
	}
}

void BombermanLevel::drawWorldTileAt(sf::RenderTarget& target, int col, int row) const
{
	if (!isInside(col, row))
		return;

	if (const ActiveBrokenBlock* brokenBlock = findActiveBrokenBlock(col, row))
	{
		if (!m_brokenFrames.empty())
		{
			const std::size_t frameIndex = getBrokenAnimationFrameIndex(*brokenBlock);
			drawTextureInTile(target, m_brokenFrames[frameIndex], col, row);
		}

		return;
	}

	const char tile = m_rows[row][col];

	if (const sf::Texture* wallTexture = getWallTextureForTile(tile))
	{
		drawTextureInTile(target, *wallTexture, col, row);
	}
	else if (tile == 'B')
	{
		if (const sf::Texture* breakableTexture = getCurrentBreakableTexture())
			drawTextureInTile(target, *breakableTexture, col, row);
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
				drawWorldTileAt(target, col, row);
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

	return isSolidWallCharacter(tile) || tile == 'B' || tile == 'K';
}

bool BombermanLevel::canExplosionPassThrough(int col, int row) const
{
	if (!isInside(col, row))
		return false;

	return !isSolidWallCharacter(m_rows[row][col]);
}

bool BombermanLevel::startBreakingBlock(int col, int row)
{
	if (!isBreakableBlock(col, row))
		return false;

	m_rows[row][col] = 'K';

	if (m_brokenFrames.empty())
	{
		m_rows[row][col] = ' ';
		m_completedBrokenBlocks.push_back({ col, row });
		return true;
	}

	ActiveBrokenBlock brokenBlock;
	brokenBlock.gridPosition = { col, row };
	brokenBlock.timer = 0.f;

	m_activeBrokenBlocks.push_back(brokenBlock);
	return true;
}

void BombermanLevel::destroyBreakableBlock(int col, int row)
{
	if (!isInside(col, row))
		return;

	if (m_rows[row][col] == 'B')
		m_rows[row][col] = ' ';
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
				positions.push_back({ col, row });
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

const std::vector<BombermanEnemySpawn>& BombermanLevel::getEnemySpawnEntries() const
{
	return m_enemySpawnEntries;
}

bool BombermanLevel::hasExit() const
{
	return m_hasExit;
}

BombermanGridPosition BombermanLevel::getExitPosition() const
{
	return m_exitPosition;
}

int BombermanLevel::getWorldNumber() const
{
	return m_worldNumber;
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