#include "GAME1_BombermanLevelEditor.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
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
				outWorldNumber = parsedWorld;
		}
		catch (...)
		{
		}

		return true;
	}

	std::optional<int> TryExtractWorldNumberFromFolder(const std::filesystem::path& path)
	{
		const std::string name = path.filename().string();
		const std::string lowerName = ToLower(name);
		const std::string prefix = "world";

		if (lowerName.rfind(prefix, 0) != 0)
			return std::nullopt;

		if (lowerName.size() <= prefix.size())
			return std::nullopt;

		for (std::size_t i = prefix.size(); i < lowerName.size(); ++i)
		{
			if (!std::isdigit(static_cast<unsigned char>(lowerName[i])))
				return std::nullopt;
		}

		try
		{
			return std::stoi(lowerName.substr(prefix.size()));
		}
		catch (...)
		{
			return std::nullopt;
		}
	}

	std::optional<int> TryExtractWorldNumberFromTemplate(const std::filesystem::path& path)
	{
		const std::string stem = ToLower(path.stem().string());
		const std::string prefix = "leveltemplate";

		if (stem == "leveltemplate")
			return 1;

		if (stem.rfind(prefix, 0) != 0)
			return std::nullopt;

		if (stem.size() <= prefix.size())
			return std::nullopt;

		for (std::size_t i = prefix.size(); i < stem.size(); ++i)
		{
			if (!std::isdigit(static_cast<unsigned char>(stem[i])))
				return std::nullopt;
		}

		try
		{
			return std::stoi(stem.substr(prefix.size()));
		}
		catch (...)
		{
			return std::nullopt;
		}
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

bool GAME1_BombermanLevelEditor::load(const std::string& fontPath, const std::string& bombermanRootDirectory)
{
	namespace fs = std::filesystem;

	m_lastError.clear();
	m_lastSavedPath.clear();

	m_bombermanRootDirectory = bombermanRootDirectory;
	m_resourcesDirectory = (fs::path(m_bombermanRootDirectory) / "Resources").string();
	m_mapsDirectory = (fs::path(m_bombermanRootDirectory) / "Maps").string();

	if (!m_font.openFromFile(fontPath))
	{
		m_lastError = "Failed to load Bomberman editor font: " + fontPath;
		return false;
	}

	refreshSavedLevelList();

	if (!resetFromTemplate())
		return false;

	return true;
}

void GAME1_BombermanLevelEditor::reset()
{
	resetFromTemplate();
}

bool GAME1_BombermanLevelEditor::resetFromTemplate()
{
	m_lastError.clear();
	m_lastSavedPath.clear();

	const int requestedWorldNumber = m_worldNumber;

	if (!loadRowsFromFile(getTemplatePath().string()))
		return false;

	m_worldNumber = requestedWorldNumber;

	buildTools();

	m_selectedToolIndex = 0;
	m_hotbarPage = 0;
	rebuildVisibleToolbar();
	refreshSavedLevelList();

	return true;
}

void GAME1_BombermanLevelEditor::buildTools()
{
	namespace fs = std::filesystem;

	m_tools.clear();

	const fs::path resourcesPath = fs::path(m_resourcesDirectory);
	const fs::path tilesPath = resourcesPath / "Tiles";
	const fs::path worldTilesPath = getCurrentWorldTilesDirectory();
	const fs::path breakablePath = GetWorldAnimationDirectory(tilesPath, "Breakable", m_worldNumber);
	const fs::path enemiesPath = resourcesPath / "Enemies";
	const fs::path playerPath = resourcesPath / "Player" / "Blue";

	addFixedTool(
		' ',
		"EMPTY",
		"Empty floor / erase",
		(worldTilesPath / "floor.png").string(),
		sf::Color(55, 120, 55)
	);

	addFixedDirectoryTool(
		'B',
		"B",
		"World " + std::to_string(m_worldNumber) + " breakable block",
		breakablePath.string(),
		sf::Color(150, 90, 40)
	);

	addFixedDirectoryTool(
		'P',
		"P",
		"Player spawn",
		(playerPath / "Front").string(),
		sf::Color(70, 150, 255)
	);

	addFixedDirectoryTool(
		'O',
		"O",
		"Copter enemy",
		(enemiesPath / "Copter" / "Front").string(),
		sf::Color(255, 80, 80)
	);

	addFixedDirectoryTool(
		'A',
		"A",
		"Lamp enemy",
		(enemiesPath / "Lamp").string(),
		sf::Color(255, 220, 70)
	);

	addFixedDirectoryTool(
		't',
		"t",
		"Tree enemy",
		(enemiesPath / "Tree" / "Front").string(),
		sf::Color(120, 255, 120)
	);

	addFixedDirectoryTool(
		'k',
		"k",
		"Bomber enemy",
		(enemiesPath / "Bomber" / "Front").string(),
		sf::Color(255, 140, 70)
	);

	addFixedDirectoryTool(
		'E',
		"E",
		"Exit marker",
		(tilesPath / "Exit").string(),
		sf::Color(255, 230, 80)
	);

	if (m_worldNumber == 3)
	{
		addWorldTool('M', "M", "World 3 solid block", (worldTilesPath / "solidblock_0.png").string(), sf::Color(120, 120, 120));

		addWorldTool('S', "S", "World 3 bottom wall", (worldTilesPath / "solidwall_bot.png").string(), sf::Color(130, 130, 130));
		addWorldTool('T', "T", "World 3 top wall", (worldTilesPath / "solidwall_top.png").string(), sf::Color(130, 130, 130));
		addWorldTool('L', "L", "World 3 left wall", (worldTilesPath / "solidwall_left.png").string(), sf::Color(130, 130, 130));
		addWorldTool('R', "R", "World 3 right wall", (worldTilesPath / "solidwall_right.png").string(), sf::Color(130, 130, 130));

		addWorldTool('Q', "Q", "World 3 top-left", (worldTilesPath / "solidwall_topleft.png").string(), sf::Color(130, 130, 130));
		addWorldTool('Y', "Y", "World 3 top-right", (worldTilesPath / "solidwall_topright.png").string(), sf::Color(130, 130, 130));
		addWorldTool('Z', "Z", "World 3 bottom-left", (worldTilesPath / "solidwall_botleft.png").string(), sf::Color(130, 130, 130));
		addWorldTool('C', "C", "World 3 bottom-right", (worldTilesPath / "solidwall_botright.png").string(), sf::Color(130, 130, 130));

		addWorldTool('U', "U", "World 3 back-left 0", (worldTilesPath / "solidwall_backleft_0.png").string(), sf::Color(130, 130, 130));
		addWorldTool('D', "D", "World 3 back-right 0", (worldTilesPath / "solidwall_backright_0.png").string(), sf::Color(130, 130, 130));

		addWorldTool('F', "F", "World 3 back-left 1", (worldTilesPath / "solidwall_backleft_1.png").string(), sf::Color(130, 130, 130));
		addWorldTool('G', "G", "World 3 back-left 2", (worldTilesPath / "solidwall_backleft_2.png").string(), sf::Color(130, 130, 130));
		addWorldTool('H', "H", "World 3 back-left 3", (worldTilesPath / "solidwall_backleft_3.png").string(), sf::Color(130, 130, 130));
		addWorldTool('I', "I", "World 3 back-left 4", (worldTilesPath / "solidwall_backleft_4.png").string(), sf::Color(130, 130, 130));

		addWorldTool('J', "J", "World 3 back-right 1", (worldTilesPath / "solidwall_backright_1.png").string(), sf::Color(130, 130, 130));
		addWorldTool('N', "N", "World 3 back-right 2", (worldTilesPath / "solidwall_backright_2.png").string(), sf::Color(130, 130, 130));
		addWorldTool('V', "V", "World 3 back-right 3", (worldTilesPath / "solidwall_backright_3.png").string(), sf::Color(130, 130, 130));
		addWorldTool('W', "W", "World 3 back-right 4", (worldTilesPath / "solidwall_backright_4.png").string(), sf::Color(130, 130, 130));
	}
	else if (m_worldNumber == 2)
	{
		addWorldTool('M', "M", "World 2 solid block", (worldTilesPath / "solidblock.png").string(), sf::Color(120, 120, 120));
		addWorldTool('S', "S", "World 2 bottom wall", (worldTilesPath / "solidwall_bot.png").string(), sf::Color(130, 130, 130));
		addWorldTool('T', "T", "World 2 top wall", (worldTilesPath / "solidwall_top.png").string(), sf::Color(130, 130, 130));

		addWorldTool('Q', "Q", "World 2 top-left", (worldTilesPath / "solidwall_topleft_0.png").string(), sf::Color(130, 130, 130));
		addWorldTool('Y', "Y", "World 2 top-right", (worldTilesPath / "solidwall_topright_0.png").string(), sf::Color(130, 130, 130));

		addWorldTool('Z', "Z", "World 2 bottom-left 0", (worldTilesPath / "solidwall_botleft_0.png").string(), sf::Color(130, 130, 130));
		addWorldTool('C', "C", "World 2 bottom-right 0", (worldTilesPath / "solidwall_botright_0.png").string(), sf::Color(130, 130, 130));

		addWorldTool('L', "L", "World 2 left 0", (worldTilesPath / "solidwall_left_0.png").string(), sf::Color(130, 130, 130));
		addWorldTool('R', "R", "World 2 right 0", (worldTilesPath / "solidwall_right_0.png").string(), sf::Color(130, 130, 130));

		addWorldTool('U', "U", "World 2 back-left 0", (worldTilesPath / "solidwall_backleft_0.png").string(), sf::Color(130, 130, 130));
		addWorldTool('D', "D", "World 2 back-right 0", (worldTilesPath / "solidwall_backright_0.png").string(), sf::Color(130, 130, 130));

		addWorldTool('F', "F", "World 2 left 1", (worldTilesPath / "solidwall_left_1.png").string(), sf::Color(130, 130, 130));
		addWorldTool('G', "G", "World 2 left 2", (worldTilesPath / "solidwall_left_2.png").string(), sf::Color(130, 130, 130));

		addWorldTool('H', "H", "World 2 right 1", (worldTilesPath / "solidwall_right_1.png").string(), sf::Color(130, 130, 130));
		addWorldTool('I', "I", "World 2 right 2", (worldTilesPath / "solidwall_right_2.png").string(), sf::Color(130, 130, 130));

		addWorldTool('J', "J", "World 2 back-left 1", (worldTilesPath / "solidwall_backleft_1.png").string(), sf::Color(130, 130, 130));
		addWorldTool('N', "N", "World 2 back-right 1", (worldTilesPath / "solidwall_backright_1.png").string(), sf::Color(130, 130, 130));

		addWorldTool('V', "V", "World 2 bottom-left 1", (worldTilesPath / "solidwall_botleft_1.png").string(), sf::Color(130, 130, 130));
		addWorldTool('W', "W", "World 2 bottom-right 1", (worldTilesPath / "solidwall_botright_1.png").string(), sf::Color(130, 130, 130));
	}
	else
	{
		addWorldTool('M', "M", "Solid block", (worldTilesPath / "solidblock.png").string(), sf::Color(120, 120, 120));
		addWorldTool('U', "U", "Solid wall up", (worldTilesPath / "solidwall_up.png").string(), sf::Color(130, 130, 130));
		addWorldTool('D', "D", "Solid wall down", (worldTilesPath / "solidwall_down.png").string(), sf::Color(130, 130, 130));
		addWorldTool('L', "L", "Solid wall left", (worldTilesPath / "solidwall_left.png").string(), sf::Color(130, 130, 130));
		addWorldTool('R', "R", "Solid wall right", (worldTilesPath / "solidwall_right.png").string(), sf::Color(130, 130, 130));
		addWorldTool('T', "T", "Solid wall top", (worldTilesPath / "solidwall_top.png").string(), sf::Color(130, 130, 130));
		addWorldTool('Q', "Q", "Solid wall top-left", (worldTilesPath / "solidwall_topleft.png").string(), sf::Color(130, 130, 130));
		addWorldTool('Y', "Y", "Solid wall top-right", (worldTilesPath / "solidwall_topright.png").string(), sf::Color(130, 130, 130));
		addWorldTool('Z', "Z", "Solid wall bottom-left", (worldTilesPath / "solidwall_botleft.png").string(), sf::Color(130, 130, 130));
		addWorldTool('C', "C", "Solid wall bottom-right", (worldTilesPath / "solidwall_botright.png").string(), sf::Color(130, 130, 130));
	}

	m_fixedToolCount = 8;
	rebuildVisibleToolbar();
}

void GAME1_BombermanLevelEditor::addFixedTool(char tile,
	const std::string& label,
	const std::string& description,
	const std::string& texturePath,
	sf::Color fallback)
{
	Tool tool;
	tool.tile = tile;
	tool.label = label;
	tool.description = description;
	tool.fallbackColor = fallback;
	tool.isWorldTile = false;
	tool.isFixedTool = true;

	if (!texturePath.empty())
		tool.hasTexture = loadTexture(tool.texture, texturePath);

	m_tools.push_back(std::move(tool));
}

void GAME1_BombermanLevelEditor::addFixedDirectoryTool(char tile,
	const std::string& label,
	const std::string& description,
	const std::string& directoryPath,
	sf::Color fallback)
{
	Tool tool;
	tool.tile = tile;
	tool.label = label;
	tool.description = description;
	tool.fallbackColor = fallback;
	tool.isWorldTile = false;
	tool.isFixedTool = true;
	tool.hasTexture = loadFirstTextureFromDirectory(tool.texture, directoryPath);

	m_tools.push_back(std::move(tool));
}

void GAME1_BombermanLevelEditor::addWorldTool(char tile,
	const std::string& label,
	const std::string& description,
	const std::string& texturePath,
	sf::Color fallback)
{
	Tool tool;
	tool.tile = tile;
	tool.label = label;
	tool.description = description;
	tool.fallbackColor = fallback;
	tool.isWorldTile = true;
	tool.isFixedTool = false;

	if (!texturePath.empty())
		tool.hasTexture = loadTexture(tool.texture, texturePath);

	m_tools.push_back(std::move(tool));
}

void GAME1_BombermanLevelEditor::rebuildVisibleToolbar()
{
	m_visibleToolbarToolIndices.clear();

	const int totalTools = static_cast<int>(m_tools.size());

	for (int i = 0; i < std::min(m_fixedToolCount, totalTools); ++i)
	{
		m_visibleToolbarToolIndices.push_back(i);
	}

	const int worldToolStart = m_fixedToolCount;
	const int worldToolCount = std::max(0, totalTools - worldToolStart);
	const int maxPage = worldToolCount <= 0
		? 0
		: static_cast<int>((worldToolCount - 1) / m_worldToolsPerPage);

	if (m_hotbarPage > maxPage)
		m_hotbarPage = maxPage;

	if (m_hotbarPage < 0)
		m_hotbarPage = 0;

	const int firstWorldTool = worldToolStart + m_hotbarPage * m_worldToolsPerPage;
	const int lastWorldToolExclusive = std::min(totalTools, firstWorldTool + m_worldToolsPerPage);

	for (int i = firstWorldTool; i < lastWorldToolExclusive; ++i)
	{
		m_visibleToolbarToolIndices.push_back(i);
	}
}

bool GAME1_BombermanLevelEditor::loadRowsFromFile(const std::string& mapPath)
{
	m_rows.clear();

	std::ifstream file(mapPath);

	if (!file.is_open())
	{
		m_lastError =
			"Failed to open Bomberman level file:\n" +
			mapPath +
			"\n\nCheck that this file exists inside the Bomberman Maps folder.";
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
		m_lastError = "Bomberman level file is empty: " + mapPath;
		return false;
	}

	for (std::size_t row = 0; row < rawLines.size(); ++row)
	{
		std::string paddedLine = rawLines[row];
		paddedLine.resize(widestLine, ' ');

		for (std::size_t col = 0; col < paddedLine.size(); ++col)
		{
			if (paddedLine[col] == 'X')
				paddedLine[col] = 'T';

			if (!validateTileCharacter(paddedLine[col]))
			{
				m_lastError =
					"Bomberman level file error: unsupported character '" +
					std::string(1, paddedLine[col]) +
					"' at row " +
					std::to_string(row + 1) +
					", column " +
					std::to_string(col + 1) +
					".";
				return false;
			}
		}

		m_rows.push_back(paddedLine);
	}

	return true;
}

bool GAME1_BombermanLevelEditor::validateTileCharacter(char tile) const
{
	switch (tile)
	{
	case ' ':
	case 'B':
	case 'P':
	case 'O':
	case 'A':
	case 't':
	case 'k':
	case 'E':
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
	case 'X':
		return true;

	default:
		return false;
	}
}

bool GAME1_BombermanLevelEditor::loadTexture(sf::Texture& texture, const std::string& texturePath)
{
	return texture.loadFromFile(texturePath);
}

bool GAME1_BombermanLevelEditor::loadFirstTextureFromDirectory(sf::Texture& texture, const std::string& directoryPath)
{
	namespace fs = std::filesystem;

	const fs::path directory(directoryPath);

	if (!fs::exists(directory) || !fs::is_directory(directory))
		return false;

	std::vector<fs::path> paths;

	for (const auto& entry : fs::directory_iterator(directory))
	{
		if (!entry.is_regular_file())
			continue;

		if (IsPngFile(entry.path()))
			paths.push_back(entry.path());
	}

	std::sort(paths.begin(), paths.end(), NaturalFrameSort);

	if (paths.empty())
		return false;

	return texture.loadFromFile(paths.front().string());
}

void GAME1_BombermanLevelEditor::layout(const sf::RenderWindow& window)
{
	rebuildVisibleToolbar();

	m_lastWindowSize = window.getSize();

	const float windowWidth = static_cast<float>(m_lastWindowSize.x);
	const float windowHeight = static_cast<float>(m_lastWindowSize.y);

	const int rows = static_cast<int>(m_rows.size());
	const int cols = rows > 0 ? static_cast<int>(m_rows[0].size()) : 0;

	const float maxTileSize = 48.f;
	const float horizontalMargin = 70.f;
	const float topReserved = 92.f;
	const float bottomReserved = 96.f;

	if (rows > 0 && cols > 0)
	{
		const float maxGridWidth = std::max(120.f, windowWidth - horizontalMargin * 2.f);
		const float maxGridHeight = std::max(120.f, windowHeight - topReserved - bottomReserved);

		const float fitX = maxGridWidth / static_cast<float>(cols);
		const float fitY = maxGridHeight / static_cast<float>(rows);

		m_tileSize = std::floor(std::min({ maxTileSize, fitX, fitY }));

		if (m_tileSize < 16.f)
			m_tileSize = 16.f;
	}
	else
	{
		m_tileSize = maxTileSize;
	}

	const float gridWidth = static_cast<float>(cols) * m_tileSize;
	const float gridHeight = static_cast<float>(rows) * m_tileSize;

	m_gridOrigin = {
		(windowWidth - gridWidth) * 0.5f,
		topReserved
	};

	m_saveButtonBounds = sf::FloatRect(
		{ windowWidth - 174.f, 22.f },
		{ 150.f, 44.f }
	);

	m_loadButtonBounds = sf::FloatRect(
		{ windowWidth - 174.f, 74.f },
		{ 150.f, 38.f }
	);

	const float loadArrowSize = 38.f;
	const float loadSelectorWidth = 108.f;
	const float loadGap = 4.f;
	const float loadTotalWidth = loadArrowSize + loadGap + loadSelectorWidth + loadGap + loadArrowSize;
	const float loadStartX = windowWidth - loadTotalWidth - 24.f;
	const float loadY = 118.f;

	m_loadPreviousButtonBounds = sf::FloatRect(
		{ loadStartX, loadY },
		{ loadArrowSize, loadArrowSize }
	);

	m_loadLevelSelectorBounds = sf::FloatRect(
		{ loadStartX + loadArrowSize + loadGap, loadY },
		{ loadSelectorWidth, loadArrowSize }
	);

	m_loadNextButtonBounds = sf::FloatRect(
		{ loadStartX + loadArrowSize + loadGap + loadSelectorWidth + loadGap, loadY },
		{ loadArrowSize, loadArrowSize }
	);

	m_worldPreviousButtonBounds = sf::FloatRect(
		{ 14.f, 22.f },
		{ 44.f, 44.f }
	);

	m_worldSelectorBounds = sf::FloatRect(
		{ 62.f, 22.f },
		{ 150.f, 44.f }
	);

	m_worldNextButtonBounds = sf::FloatRect(
		{ 216.f, 22.f },
		{ 44.f, 44.f }
	);

	const float hotbarArrowWidth = 40.f;
	const float visibleToolbarWidth =
		static_cast<float>(m_visibleToolbarToolIndices.size()) * m_toolbarSlotSize +
		static_cast<float>(m_visibleToolbarToolIndices.size() > 0 ? m_visibleToolbarToolIndices.size() - 1 : 0) * m_toolbarSlotGap;

	const float totalToolbarWidth =
		hotbarArrowWidth +
		m_toolbarSlotGap +
		visibleToolbarWidth +
		m_toolbarSlotGap +
		hotbarArrowWidth;

	float toolbarY = m_gridOrigin.y + gridHeight + 12.f;

	if (toolbarY + m_toolbarSlotSize > windowHeight - 12.f)
		toolbarY = windowHeight - m_toolbarSlotSize - 12.f;

	const float toolbarStartX = (windowWidth - totalToolbarWidth) * 0.5f;

	m_previousHotbarPageButtonBounds = sf::FloatRect(
		{
			toolbarStartX,
			toolbarY
		},
		{
			hotbarArrowWidth,
			m_toolbarSlotSize
		}
	);

	m_toolbarOrigin = {
		toolbarStartX + hotbarArrowWidth + m_toolbarSlotGap,
		toolbarY
	};

	m_nextHotbarPageButtonBounds = sf::FloatRect(
		{
			m_toolbarOrigin.x + visibleToolbarWidth + m_toolbarSlotGap,
			m_toolbarOrigin.y
		},
		{
			hotbarArrowWidth,
			m_toolbarSlotSize
		}
	);
}

void GAME1_BombermanLevelEditor::update(float deltaTime, sf::Vector2u windowSize)
{
	(void)deltaTime;
	m_lastWindowSize = windowSize;
}

void GAME1_BombermanLevelEditor::draw(sf::RenderWindow& window)
{
	draw(window, sf::Mouse::getPosition(window));
}

void GAME1_BombermanLevelEditor::draw(sf::RenderWindow& window, sf::Vector2i mousePixelPosition)
{
	layout(window);

	const float windowWidth = static_cast<float>(window.getSize().x);
	const float windowHeight = static_cast<float>(window.getSize().y);

	sf::RectangleShape background;
	background.setPosition({ 0.f, 0.f });
	background.setSize({ windowWidth, windowHeight });
	background.setFillColor(sf::Color(22, 24, 32));
	window.draw(background);

	auto drawTextCentered = [this, &window](
		const std::string& value,
		unsigned int size,
		const sf::FloatRect& rect,
		sf::Color fill,
		float outlineThickness = 2.f)
		{
			sf::Text text(m_font);
			text.setString(value);
			text.setCharacterSize(size);
			text.setFillColor(fill);
			text.setOutlineColor(sf::Color::Black);
			text.setOutlineThickness(outlineThickness);

			const sf::FloatRect bounds = text.getLocalBounds();

			text.setPosition({
				rect.position.x + (rect.size.x - bounds.size.x) * 0.5f - bounds.position.x,
				rect.position.y + (rect.size.y - bounds.size.y) * 0.5f - bounds.position.y - 1.f
				});

			window.draw(text);
		};

	drawTextCentered(
		"BOMBERMAN LEVEL EDITOR",
		28,
		sf::FloatRect({ 0.f, 14.f }, { windowWidth, 36.f }),
		sf::Color::White
	);

	sf::RectangleShape worldPreviousButton;
	worldPreviousButton.setPosition(m_worldPreviousButtonBounds.position);
	worldPreviousButton.setSize(m_worldPreviousButtonBounds.size);
	worldPreviousButton.setFillColor(sf::Color(45, 45, 70));
	worldPreviousButton.setOutlineColor(sf::Color::White);
	worldPreviousButton.setOutlineThickness(2.f);
	window.draw(worldPreviousButton);

	drawTextCentered("<", 28, m_worldPreviousButtonBounds, sf::Color::White);

	sf::RectangleShape worldBox;
	worldBox.setPosition(m_worldSelectorBounds.position);
	worldBox.setSize(m_worldSelectorBounds.size);
	worldBox.setFillColor(sf::Color(45, 45, 65));
	worldBox.setOutlineColor(sf::Color::White);
	worldBox.setOutlineThickness(2.f);
	window.draw(worldBox);

	drawTextCentered(
		"<World " + std::to_string(m_worldNumber) + ">",
		19,
		m_worldSelectorBounds,
		sf::Color(255, 230, 120)
	);

	sf::RectangleShape worldNextButton;
	worldNextButton.setPosition(m_worldNextButtonBounds.position);
	worldNextButton.setSize(m_worldNextButtonBounds.size);
	worldNextButton.setFillColor(sf::Color(45, 45, 70));
	worldNextButton.setOutlineColor(sf::Color::White);
	worldNextButton.setOutlineThickness(2.f);
	window.draw(worldNextButton);

	drawTextCentered(">", 28, m_worldNextButtonBounds, sf::Color::White);

	sf::RectangleShape saveButton;
	saveButton.setPosition(m_saveButtonBounds.position);
	saveButton.setSize(m_saveButtonBounds.size);
	saveButton.setFillColor(sf::Color(40, 120, 60));
	saveButton.setOutlineColor(sf::Color::White);
	saveButton.setOutlineThickness(2.f);
	window.draw(saveButton);

	drawTextCentered("SAVE", 22, m_saveButtonBounds, sf::Color::White);

	sf::RectangleShape loadButton;
	loadButton.setPosition(m_loadButtonBounds.position);
	loadButton.setSize(m_loadButtonBounds.size);
	loadButton.setFillColor(sf::Color(45, 70, 120));
	loadButton.setOutlineColor(sf::Color::White);
	loadButton.setOutlineThickness(2.f);
	window.draw(loadButton);

	drawTextCentered("<Load>", 19, m_loadButtonBounds, sf::Color::White);

	sf::RectangleShape loadPreviousButton;
	loadPreviousButton.setPosition(m_loadPreviousButtonBounds.position);
	loadPreviousButton.setSize(m_loadPreviousButtonBounds.size);
	loadPreviousButton.setFillColor(sf::Color(45, 45, 70));
	loadPreviousButton.setOutlineColor(sf::Color::White);
	loadPreviousButton.setOutlineThickness(2.f);
	window.draw(loadPreviousButton);

	drawTextCentered("<", 24, m_loadPreviousButtonBounds, sf::Color::White);

	sf::RectangleShape loadSelectorBox;
	loadSelectorBox.setPosition(m_loadLevelSelectorBounds.position);
	loadSelectorBox.setSize(m_loadLevelSelectorBounds.size);
	loadSelectorBox.setFillColor(sf::Color(38, 38, 52));
	loadSelectorBox.setOutlineColor(sf::Color::White);
	loadSelectorBox.setOutlineThickness(2.f);
	window.draw(loadSelectorBox);

	drawTextCentered(getSelectedLoadLevelName(), 16, m_loadLevelSelectorBounds, sf::Color(255, 230, 120));

	sf::RectangleShape loadNextButton;
	loadNextButton.setPosition(m_loadNextButtonBounds.position);
	loadNextButton.setSize(m_loadNextButtonBounds.size);
	loadNextButton.setFillColor(sf::Color(45, 45, 70));
	loadNextButton.setOutlineColor(sf::Color::White);
	loadNextButton.setOutlineThickness(2.f);
	window.draw(loadNextButton);

	drawTextCentered(">", 24, m_loadNextButtonBounds, sf::Color::White);

	const Tool* selectedTool = nullptr;

	if (m_selectedToolIndex >= 0 && m_selectedToolIndex < static_cast<int>(m_tools.size()))
		selectedTool = &m_tools[m_selectedToolIndex];

	if (selectedTool != nullptr)
	{
		drawTextCentered(
			"Selected: " + selectedTool->label + " - " + selectedTool->description,
			18,
			sf::FloatRect({ 0.f, 50.f }, { windowWidth, 28.f }),
			sf::Color(255, 230, 120)
		);
	}

	const int rows = static_cast<int>(m_rows.size());
	const int cols = rows > 0 ? static_cast<int>(m_rows[0].size()) : 0;

	for (int row = 0; row < rows; ++row)
	{
		for (int col = 0; col < cols; ++col)
		{
			const sf::FloatRect tileRect(
				{
					m_gridOrigin.x + static_cast<float>(col) * m_tileSize,
					m_gridOrigin.y + static_cast<float>(row) * m_tileSize
				},
				{
					m_tileSize,
					m_tileSize
				}
			);

			drawTilePreview(window, ' ', tileRect);

			if (row < static_cast<int>(m_rows.size()) &&
				col < static_cast<int>(m_rows[row].size()))
			{
				const char tile = m_rows[row][col];

				if (tile != ' ')
				{
					drawTilePreview(window, tile, tileRect);
				}
			}

			sf::RectangleShape gridLine;
			gridLine.setPosition(tileRect.position);
			gridLine.setSize(tileRect.size);
			gridLine.setFillColor(sf::Color::Transparent);
			gridLine.setOutlineColor(sf::Color(120, 30, 30, 120));
			gridLine.setOutlineThickness(-1.f);
			window.draw(gridLine);
		}
	}

	if (rows > 0 && cols > 0)
	{
		sf::RectangleShape border;
		border.setPosition(m_gridOrigin);
		border.setSize({
			static_cast<float>(cols) * m_tileSize,
			static_cast<float>(rows) * m_tileSize
			});
		border.setFillColor(sf::Color::Transparent);
		border.setOutlineColor(sf::Color::White);
		border.setOutlineThickness(3.f);
		window.draw(border);
	}

	if (const std::optional<sf::Vector2i> hoveredTile = getTileAtPixel(mousePixelPosition))
	{
		sf::RectangleShape hoverRect;
		hoverRect.setPosition({
			m_gridOrigin.x + static_cast<float>(hoveredTile->x) * m_tileSize,
			m_gridOrigin.y + static_cast<float>(hoveredTile->y) * m_tileSize
			});
		hoverRect.setSize({ m_tileSize, m_tileSize });
		hoverRect.setFillColor(sf::Color::Transparent);
		hoverRect.setOutlineColor(sf::Color::Yellow);
		hoverRect.setOutlineThickness(3.f);
		window.draw(hoverRect);
	}

	sf::RectangleShape previousPageButton;
	previousPageButton.setPosition(m_previousHotbarPageButtonBounds.position);
	previousPageButton.setSize(m_previousHotbarPageButtonBounds.size);
	previousPageButton.setFillColor(sf::Color(45, 45, 70));
	previousPageButton.setOutlineColor(sf::Color::White);
	previousPageButton.setOutlineThickness(2.f);
	window.draw(previousPageButton);

	drawTextCentered("<", 28, m_previousHotbarPageButtonBounds, sf::Color::White);

	for (std::size_t visibleIndex = 0; visibleIndex < m_visibleToolbarToolIndices.size(); ++visibleIndex)
	{
		const int toolIndex = m_visibleToolbarToolIndices[visibleIndex];

		if (toolIndex < 0 || toolIndex >= static_cast<int>(m_tools.size()))
			continue;

		const sf::FloatRect slotRect(
			{
				m_toolbarOrigin.x + static_cast<float>(visibleIndex) * (m_toolbarSlotSize + m_toolbarSlotGap),
				m_toolbarOrigin.y
			},
			{
				m_toolbarSlotSize,
				m_toolbarSlotSize
			}
		);

		const Tool& tool = m_tools[toolIndex];

		sf::RectangleShape slot;
		slot.setPosition(slotRect.position);
		slot.setSize(slotRect.size);
		slot.setFillColor(sf::Color(42, 42, 50));

		if (toolIndex == m_selectedToolIndex)
		{
			slot.setOutlineColor(sf::Color::Yellow);
			slot.setOutlineThickness(3.f);
		}
		else
		{
			slot.setOutlineColor(sf::Color(160, 160, 160));
			slot.setOutlineThickness(2.f);
		}

		window.draw(slot);

		const sf::FloatRect previewRect(
			{ slotRect.position.x + 5.f, slotRect.position.y + 5.f },
			{ slotRect.size.x - 10.f, slotRect.size.y - 10.f }
		);

		drawToolPreview(window, tool, previewRect);

		drawTextCentered(
			tool.label,
			13,
			slotRect,
			sf::Color::White,
			1.5f
		);
	}

	sf::RectangleShape nextPageButton;
	nextPageButton.setPosition(m_nextHotbarPageButtonBounds.position);
	nextPageButton.setSize(m_nextHotbarPageButtonBounds.size);
	nextPageButton.setFillColor(sf::Color(45, 45, 70));
	nextPageButton.setOutlineColor(sf::Color::White);
	nextPageButton.setOutlineThickness(2.f);
	window.draw(nextPageButton);

	drawTextCentered(">", 28, m_nextHotbarPageButtonBounds, sf::Color::White);

	const float controlsY = std::min(windowHeight - 28.f, m_toolbarOrigin.y + m_toolbarSlotSize + 8.f);

	drawTextCentered(
		"Controls: Left Click = place/select    Right Click = erase    Middle Mouse = pick tile/tool    Mouse Wheel = cycle visible tools    T = Tree    K = Bomber    Enter = save    Backspace = reset",
		14,
		sf::FloatRect({ 0.f, controlsY }, { windowWidth, 24.f }),
		sf::Color(220, 220, 220),
		1.5f
	);
}

void GAME1_BombermanLevelEditor::handleMousePressed(sf::Mouse::Button button, sf::Vector2i mousePixelPosition)
{
	const sf::Vector2f mousePosition{
		static_cast<float>(mousePixelPosition.x),
		static_cast<float>(mousePixelPosition.y)
	};

	if (button == sf::Mouse::Button::Left)
	{
		if (containsPoint(m_saveButtonBounds, mousePosition))
		{
			saveToNextLevelFile();
			return;
		}

		if (containsPoint(m_loadButtonBounds, mousePosition))
		{
			loadSelectedLevelIntoEditor();
			return;
		}

		if (containsPoint(m_loadPreviousButtonBounds, mousePosition))
		{
			selectPreviousLoadLevel();
			return;
		}

		if (containsPoint(m_loadNextButtonBounds, mousePosition))
		{
			selectNextLoadLevel();
			return;
		}

		if (containsPoint(m_worldPreviousButtonBounds, mousePosition))
		{
			selectPreviousWorld();
			return;
		}

		if (containsPoint(m_worldNextButtonBounds, mousePosition) ||
			containsPoint(m_worldSelectorBounds, mousePosition))
		{
			selectNextWorld();
			return;
		}

		if (containsPoint(m_previousHotbarPageButtonBounds, mousePosition))
		{
			selectPreviousHotbarPage();
			return;
		}

		if (containsPoint(m_nextHotbarPageButtonBounds, mousePosition))
		{
			selectNextHotbarPage();
			return;
		}

		const std::optional<int> toolbarIndex = getToolbarIndexAtPixel(mousePixelPosition);

		if (toolbarIndex.has_value())
		{
			m_selectedToolIndex = *toolbarIndex;
			return;
		}

		paintAtPixel(mousePixelPosition);
	}
	else if (button == sf::Mouse::Button::Right)
	{
		eraseAtPixel(mousePixelPosition);
	}
	else if (button == sf::Mouse::Button::Middle)
	{
		pickAtPixel(mousePixelPosition);
	}
}

void GAME1_BombermanLevelEditor::handleMouseWheelScrolled(float delta)
{
	if (delta < 0.f)
	{
		selectNextTool();
	}
	else if (delta > 0.f)
	{
		selectPreviousTool();
	}
}

void GAME1_BombermanLevelEditor::handleKeyReleased(sf::Keyboard::Key key)
{
	switch (key)
	{
	case sf::Keyboard::Key::Enter:
		saveToNextLevelFile();
		break;

	case sf::Keyboard::Key::Backspace:
		resetFromTemplate();
		break;

	case sf::Keyboard::Key::Space:
		selectToolByTile(' ');
		break;

	case sf::Keyboard::Key::Tab:
		selectNextHotbarPage();
		break;

	default:
		selectToolByHotkey(key);
		break;
	}
}

void GAME1_BombermanLevelEditor::paintAtPixel(sf::Vector2i pixelPosition)
{
	if (m_selectedToolIndex < 0 || m_selectedToolIndex >= static_cast<int>(m_tools.size()))
		return;

	const std::optional<sf::Vector2i> tilePosition = getTileAtPixel(pixelPosition);

	if (!tilePosition.has_value())
		return;

	placeTileAt(tilePosition->x, tilePosition->y, m_tools[m_selectedToolIndex].tile);
}

void GAME1_BombermanLevelEditor::eraseAtPixel(sf::Vector2i pixelPosition)
{
	const std::optional<sf::Vector2i> tilePosition = getTileAtPixel(pixelPosition);

	if (!tilePosition.has_value())
		return;

	placeTileAt(tilePosition->x, tilePosition->y, ' ');
}

void GAME1_BombermanLevelEditor::pickAtPixel(sf::Vector2i pixelPosition)
{
	const std::optional<int> toolbarIndex = getToolbarIndexAtPixel(pixelPosition);

	if (toolbarIndex.has_value())
	{
		m_selectedToolIndex = *toolbarIndex;
		ensureSelectedToolVisible();
		return;
	}

	const std::optional<sf::Vector2i> tilePosition = getTileAtPixel(pixelPosition);

	if (!tilePosition.has_value())
		return;

	const int col = tilePosition->x;
	const int row = tilePosition->y;

	if (row < 0 || row >= static_cast<int>(m_rows.size()))
		return;

	if (col < 0 || col >= static_cast<int>(m_rows[row].size()))
		return;

	selectToolByTile(m_rows[row][col]);
}

void GAME1_BombermanLevelEditor::selectToolbarSlot(int slotNumber)
{
	const int visibleIndex = slotNumber - 1;

	if (visibleIndex < 0 || visibleIndex >= static_cast<int>(m_visibleToolbarToolIndices.size()))
		return;

	m_selectedToolIndex = m_visibleToolbarToolIndices[visibleIndex];
}

std::optional<sf::Vector2i> GAME1_BombermanLevelEditor::getTileAtPixel(sf::Vector2i pixelPosition) const
{
	const sf::Vector2f mousePosition{
		static_cast<float>(pixelPosition.x),
		static_cast<float>(pixelPosition.y)
	};

	const int rows = static_cast<int>(m_rows.size());
	const int cols = rows > 0 ? static_cast<int>(m_rows[0].size()) : 0;

	if (rows <= 0 || cols <= 0)
		return std::nullopt;

	const sf::FloatRect gridBounds(
		m_gridOrigin,
		{
			static_cast<float>(cols) * m_tileSize,
			static_cast<float>(rows) * m_tileSize
		}
	);

	if (!containsPoint(gridBounds, mousePosition))
		return std::nullopt;

	const int col = static_cast<int>((mousePosition.x - m_gridOrigin.x) / m_tileSize);
	const int row = static_cast<int>((mousePosition.y - m_gridOrigin.y) / m_tileSize);

	if (col < 0 || col >= cols || row < 0 || row >= rows)
		return std::nullopt;

	return sf::Vector2i{ col, row };
}

std::optional<int> GAME1_BombermanLevelEditor::getToolbarIndexAtPixel(sf::Vector2i pixelPosition) const
{
	const sf::Vector2f mousePosition{
		static_cast<float>(pixelPosition.x),
		static_cast<float>(pixelPosition.y)
	};

	for (std::size_t visibleIndex = 0; visibleIndex < m_visibleToolbarToolIndices.size(); ++visibleIndex)
	{
		const sf::FloatRect slotRect(
			{
				m_toolbarOrigin.x + static_cast<float>(visibleIndex) * (m_toolbarSlotSize + m_toolbarSlotGap),
				m_toolbarOrigin.y
			},
			{
				m_toolbarSlotSize,
				m_toolbarSlotSize
			}
		);

		if (containsPoint(slotRect, mousePosition))
			return m_visibleToolbarToolIndices[visibleIndex];
	}

	return std::nullopt;
}

bool GAME1_BombermanLevelEditor::containsPoint(const sf::FloatRect& bounds, sf::Vector2f point) const
{
	return point.x >= bounds.position.x &&
		point.x <= bounds.position.x + bounds.size.x &&
		point.y >= bounds.position.y &&
		point.y <= bounds.position.y + bounds.size.y;
}

void GAME1_BombermanLevelEditor::selectToolByTile(char tile)
{
	const int index = findToolIndexForTile(tile);

	if (index < 0)
		return;

	m_selectedToolIndex = index;
	ensureSelectedToolVisible();
}

void GAME1_BombermanLevelEditor::selectToolByHotkey(sf::Keyboard::Key key)
{
	switch (key)
	{
	case sf::Keyboard::Key::A: selectToolByTile('A'); break;
	case sf::Keyboard::Key::B: selectToolByTile('B'); break;
	case sf::Keyboard::Key::C: selectToolByTile('C'); break;
	case sf::Keyboard::Key::D: selectToolByTile('D'); break;
	case sf::Keyboard::Key::E: selectToolByTile('E'); break;
	case sf::Keyboard::Key::F: selectToolByTile('F'); break;
	case sf::Keyboard::Key::G: selectToolByTile('G'); break;
	case sf::Keyboard::Key::H: selectToolByTile('H'); break;
	case sf::Keyboard::Key::I: selectToolByTile('I'); break;
	case sf::Keyboard::Key::J: selectToolByTile('J'); break;
	case sf::Keyboard::Key::K: selectToolByTile('k'); break;
	case sf::Keyboard::Key::L: selectToolByTile('L'); break;
	case sf::Keyboard::Key::M: selectToolByTile('M'); break;
	case sf::Keyboard::Key::N: selectToolByTile('N'); break;
	case sf::Keyboard::Key::O: selectToolByTile('O'); break;
	case sf::Keyboard::Key::P: selectToolByTile('P'); break;
	case sf::Keyboard::Key::Q: selectToolByTile('Q'); break;
	case sf::Keyboard::Key::R: selectToolByTile('R'); break;
	case sf::Keyboard::Key::S: selectToolByTile('S'); break;
	case sf::Keyboard::Key::T: selectToolByTile('t'); break;
	case sf::Keyboard::Key::U: selectToolByTile('U'); break;
	case sf::Keyboard::Key::V: selectToolByTile('V'); break;
	case sf::Keyboard::Key::W: selectToolByTile('W'); break;
	case sf::Keyboard::Key::Y: selectToolByTile('Y'); break;
	case sf::Keyboard::Key::Z: selectToolByTile('Z'); break;

	default:
		break;
	}
}

void GAME1_BombermanLevelEditor::selectNextTool()
{
	if (m_visibleToolbarToolIndices.empty())
		return;

	int visiblePosition = findVisibleToolbarPositionForToolIndex(m_selectedToolIndex);

	if (visiblePosition < 0)
	{
		m_selectedToolIndex = m_visibleToolbarToolIndices.front();
		return;
	}

	visiblePosition = (visiblePosition + 1) % static_cast<int>(m_visibleToolbarToolIndices.size());
	m_selectedToolIndex = m_visibleToolbarToolIndices[visiblePosition];
}

void GAME1_BombermanLevelEditor::selectPreviousTool()
{
	if (m_visibleToolbarToolIndices.empty())
		return;

	int visiblePosition = findVisibleToolbarPositionForToolIndex(m_selectedToolIndex);

	if (visiblePosition < 0)
	{
		m_selectedToolIndex = m_visibleToolbarToolIndices.front();
		return;
	}

	--visiblePosition;

	if (visiblePosition < 0)
		visiblePosition = static_cast<int>(m_visibleToolbarToolIndices.size()) - 1;

	m_selectedToolIndex = m_visibleToolbarToolIndices[visiblePosition];
}

void GAME1_BombermanLevelEditor::selectNextWorld()
{
	const int previousWorld = m_worldNumber;
	const int previousSelectedToolIndex = m_selectedToolIndex;
	const int previousHotbarPage = m_hotbarPage;
	const std::vector<std::string> previousRows = m_rows;

	const int highestWorld = std::max(1, getHighestAvailableWorldNumber());

	const int targetWorld = m_worldNumber >= highestWorld ? 1 : m_worldNumber + 1;

	m_worldNumber = targetWorld;
	m_hotbarPage = 0;

	if (!loadRowsFromFile(getTemplatePath().string()))
	{
		m_worldNumber = previousWorld;
		m_selectedToolIndex = previousSelectedToolIndex;
		m_hotbarPage = previousHotbarPage;
		m_rows = previousRows;
		buildTools();
		rebuildVisibleToolbar();
		return;
	}

	m_worldNumber = targetWorld;

	buildTools();

	m_selectedToolIndex = 0;
	m_hotbarPage = 0;

	rebuildVisibleToolbar();
}

void GAME1_BombermanLevelEditor::selectPreviousWorld()
{
	const int previousWorld = m_worldNumber;
	const int previousSelectedToolIndex = m_selectedToolIndex;
	const int previousHotbarPage = m_hotbarPage;
	const std::vector<std::string> previousRows = m_rows;

	const int highestWorld = std::max(1, getHighestAvailableWorldNumber());

	const int targetWorld = m_worldNumber <= 1 ? highestWorld : m_worldNumber - 1;

	m_worldNumber = targetWorld;
	m_hotbarPage = 0;

	if (!loadRowsFromFile(getTemplatePath().string()))
	{
		m_worldNumber = previousWorld;
		m_selectedToolIndex = previousSelectedToolIndex;
		m_hotbarPage = previousHotbarPage;
		m_rows = previousRows;
		buildTools();
		rebuildVisibleToolbar();
		return;
	}

	m_worldNumber = targetWorld;

	buildTools();

	m_selectedToolIndex = 0;
	m_hotbarPage = 0;

	rebuildVisibleToolbar();
}

void GAME1_BombermanLevelEditor::selectNextHotbarPage()
{
	const int worldToolCount = std::max(0, static_cast<int>(m_tools.size()) - m_fixedToolCount);

	if (worldToolCount <= 0)
		return;

	const int maxPage = static_cast<int>((worldToolCount - 1) / m_worldToolsPerPage);

	++m_hotbarPage;

	if (m_hotbarPage > maxPage)
		m_hotbarPage = 0;

	rebuildVisibleToolbar();

	if (findVisibleToolbarPositionForToolIndex(m_selectedToolIndex) < 0 &&
		!m_visibleToolbarToolIndices.empty())
	{
		m_selectedToolIndex = m_visibleToolbarToolIndices.front();
	}
}

void GAME1_BombermanLevelEditor::selectPreviousHotbarPage()
{
	const int worldToolCount = std::max(0, static_cast<int>(m_tools.size()) - m_fixedToolCount);

	if (worldToolCount <= 0)
		return;

	const int maxPage = static_cast<int>((worldToolCount - 1) / m_worldToolsPerPage);

	--m_hotbarPage;

	if (m_hotbarPage < 0)
		m_hotbarPage = maxPage;

	rebuildVisibleToolbar();

	if (findVisibleToolbarPositionForToolIndex(m_selectedToolIndex) < 0 &&
		!m_visibleToolbarToolIndices.empty())
	{
		m_selectedToolIndex = m_visibleToolbarToolIndices.front();
	}
}

void GAME1_BombermanLevelEditor::ensureSelectedToolVisible()
{
	if (m_selectedToolIndex < 0 || m_selectedToolIndex >= static_cast<int>(m_tools.size()))
		return;

	if (m_selectedToolIndex < m_fixedToolCount)
	{
		rebuildVisibleToolbar();
		return;
	}

	const int worldToolIndex = m_selectedToolIndex - m_fixedToolCount;
	m_hotbarPage = worldToolIndex / m_worldToolsPerPage;
	rebuildVisibleToolbar();
}

void GAME1_BombermanLevelEditor::refreshSavedLevelList()
{
	namespace fs = std::filesystem;

	m_savedLevelPaths.clear();

	try
	{
		fs::create_directories(m_mapsDirectory);

		std::vector<fs::path> paths;

		for (const auto& entry : fs::directory_iterator(m_mapsDirectory))
		{
			if (entry.is_regular_file() && isValidLevelFile(entry.path()))
			{
				paths.push_back(entry.path());
			}
		}

		std::sort(paths.begin(), paths.end(),
			[this](const fs::path& a, const fs::path& b)
			{
				return extractLevelNumber(a) < extractLevelNumber(b);
			});

		for (const fs::path& path : paths)
		{
			m_savedLevelPaths.push_back(path.string());
		}
	}
	catch (...)
	{
		m_savedLevelPaths.clear();
	}

	if (m_savedLevelPaths.empty())
	{
		m_selectedLoadLevelIndex = 0;
		return;
	}

	if (m_selectedLoadLevelIndex < 0)
		m_selectedLoadLevelIndex = 0;

	if (m_selectedLoadLevelIndex >= static_cast<int>(m_savedLevelPaths.size()))
		m_selectedLoadLevelIndex = static_cast<int>(m_savedLevelPaths.size()) - 1;
}

void GAME1_BombermanLevelEditor::selectNextLoadLevel()
{
	refreshSavedLevelList();

	if (m_savedLevelPaths.empty())
		return;

	m_selectedLoadLevelIndex =
		(m_selectedLoadLevelIndex + 1) % static_cast<int>(m_savedLevelPaths.size());
}

void GAME1_BombermanLevelEditor::selectPreviousLoadLevel()
{
	refreshSavedLevelList();

	if (m_savedLevelPaths.empty())
		return;

	--m_selectedLoadLevelIndex;

	if (m_selectedLoadLevelIndex < 0)
		m_selectedLoadLevelIndex = static_cast<int>(m_savedLevelPaths.size()) - 1;
}

bool GAME1_BombermanLevelEditor::loadSelectedLevelIntoEditor()
{
	refreshSavedLevelList();

	m_lastError.clear();
	m_lastSavedPath.clear();

	if (m_savedLevelPaths.empty())
	{
		m_lastError = "No saved Bomberman levels found in:\n" + m_mapsDirectory;
		return false;
	}

	if (m_selectedLoadLevelIndex < 0 ||
		m_selectedLoadLevelIndex >= static_cast<int>(m_savedLevelPaths.size()))
	{
		m_selectedLoadLevelIndex = 0;
	}

	const std::string selectedPath = m_savedLevelPaths[m_selectedLoadLevelIndex];

	const int previousWorld = m_worldNumber;
	const int previousSelectedToolIndex = m_selectedToolIndex;
	const int previousHotbarPage = m_hotbarPage;
	const std::vector<std::string> previousRows = m_rows;

	m_worldNumber = 1;

	if (!loadRowsFromFile(selectedPath))
	{
		m_worldNumber = previousWorld;
		m_selectedToolIndex = previousSelectedToolIndex;
		m_hotbarPage = previousHotbarPage;
		m_rows = previousRows;
		buildTools();
		rebuildVisibleToolbar();
		return false;
	}

	buildTools();

	m_selectedToolIndex = 0;
	m_hotbarPage = 0;

	rebuildVisibleToolbar();

	return true;
}

std::string GAME1_BombermanLevelEditor::getSelectedLoadLevelName() const
{
	if (m_savedLevelPaths.empty())
		return "<none>";

	if (m_selectedLoadLevelIndex < 0 ||
		m_selectedLoadLevelIndex >= static_cast<int>(m_savedLevelPaths.size()))
	{
		return "<none>";
	}

	const std::filesystem::path path(m_savedLevelPaths[m_selectedLoadLevelIndex]);
	return path.stem().string();
}

int GAME1_BombermanLevelEditor::findToolIndexForTile(char tile) const
{
	for (int i = 0; i < static_cast<int>(m_tools.size()); ++i)
	{
		if (m_tools[i].tile == tile)
			return i;
	}

	return -1;
}

int GAME1_BombermanLevelEditor::findVisibleToolbarPositionForToolIndex(int toolIndex) const
{
	for (int i = 0; i < static_cast<int>(m_visibleToolbarToolIndices.size()); ++i)
	{
		if (m_visibleToolbarToolIndices[i] == toolIndex)
			return i;
	}

	return -1;
}

void GAME1_BombermanLevelEditor::placeTileAt(int col, int row, char tile)
{
	if (!validateTileCharacter(tile))
		return;

	if (row < 0 || row >= static_cast<int>(m_rows.size()))
		return;

	if (col < 0 || col >= static_cast<int>(m_rows[row].size()))
		return;

	if (tile == 'P')
	{
		for (std::string& mapRow : m_rows)
		{
			for (char& cell : mapRow)
			{
				if (cell == 'P')
					cell = ' ';
			}
		}
	}

	m_rows[row][col] = tile;
}

bool GAME1_BombermanLevelEditor::saveToNextLevelFile()
{
	namespace fs = std::filesystem;

	m_lastError.clear();
	m_lastSavedPath.clear();

	bool hasPlayerSpawn = false;

	for (const std::string& row : m_rows)
	{
		for (char tile : row)
		{
			if (tile == 'P')
			{
				hasPlayerSpawn = true;
				break;
			}
		}

		if (hasPlayerSpawn)
			break;
	}

	if (!hasPlayerSpawn)
	{
		m_lastError = "Cannot save Bomberman level: place one player spawn tile using P.";
		return false;
	}

	try
	{
		fs::create_directories(m_mapsDirectory);

		int highestLevelNumber = 0;

		for (const auto& entry : fs::directory_iterator(m_mapsDirectory))
		{
			if (entry.is_regular_file() && isValidLevelFile(entry.path()))
			{
				highestLevelNumber = std::max(highestLevelNumber, extractLevelNumber(entry.path()));
			}
		}

		const int nextNumber = highestLevelNumber + 1;

		std::ostringstream fileNameStream;
		fileNameStream << "level" << std::setw(2) << std::setfill('0') << nextNumber << ".txt";

		const fs::path savePath = fs::path(m_mapsDirectory) / fileNameStream.str();

		std::ofstream file(savePath);

		if (!file.is_open())
		{
			m_lastError = "Failed to create Bomberman level file: " + savePath.string();
			return false;
		}

		file << "#WORLD=" << m_worldNumber << '\n';

		for (std::size_t row = 0; row < m_rows.size(); ++row)
		{
			file << m_rows[row];

			if (row + 1 < m_rows.size())
				file << '\n';
		}

		if (!file.good())
		{
			m_lastError = "Failed while writing Bomberman level file: " + savePath.string();
			return false;
		}

		m_lastSavedPath = savePath.string();

		refreshSavedLevelList();

		for (int i = 0; i < static_cast<int>(m_savedLevelPaths.size()); ++i)
		{
			if (std::filesystem::path(m_savedLevelPaths[i]).lexically_normal() ==
				savePath.lexically_normal())
			{
				m_selectedLoadLevelIndex = i;
				break;
			}
		}

		return true;
	}
	catch (const std::exception& e)
	{
		m_lastError = std::string("Bomberman level save failed: ") + e.what();
		return false;
	}
}

void GAME1_BombermanLevelEditor::drawText(sf::RenderTarget& target,
	const std::string& string,
	unsigned int size,
	sf::Vector2f position,
	sf::Color fill,
	float outlineThickness) const
{
	sf::Text text(m_font);
	text.setString(string);
	text.setCharacterSize(size);
	text.setFillColor(fill);
	text.setOutlineColor(sf::Color::Black);
	text.setOutlineThickness(outlineThickness);
	text.setPosition(position);

	target.draw(text);
}

void GAME1_BombermanLevelEditor::drawToolPreview(sf::RenderTarget& target,
	const Tool& tool,
	const sf::FloatRect& bounds) const
{
	if (tool.hasTexture)
	{
		drawTextureFitted(target, tool.texture, bounds);
		return;
	}

	sf::RectangleShape fallback;
	fallback.setPosition(bounds.position);
	fallback.setSize(bounds.size);
	fallback.setFillColor(tool.fallbackColor);
	fallback.setOutlineColor(sf::Color::Black);
	fallback.setOutlineThickness(1.f);

	target.draw(fallback);
}

void GAME1_BombermanLevelEditor::drawTilePreview(sf::RenderTarget& target,
	char tile,
	const sf::FloatRect& bounds) const
{
	const int toolIndex = findToolIndexForTile(tile);

	if (toolIndex >= 0)
	{
		drawToolPreview(target, m_tools[toolIndex], bounds);

		if (!m_tools[toolIndex].hasTexture ||
			tile == 'P' ||
			tile == 'O' ||
			tile == 'A' ||
			tile == 't' ||
			tile == 'k' ||
			tile == 'E')
		{
			sf::Text label(m_font);

			if (tile == ' ')
				label.setString("");
			else
				label.setString(std::string(1, tile));

			label.setCharacterSize(static_cast<unsigned int>(std::max(12.f, bounds.size.y * 0.42f)));
			label.setFillColor(sf::Color::White);
			label.setOutlineColor(sf::Color::Black);
			label.setOutlineThickness(2.f);

			const sf::FloatRect textBounds = label.getLocalBounds();

			label.setPosition({
				bounds.position.x + (bounds.size.x - textBounds.size.x) * 0.5f - textBounds.position.x,
				bounds.position.y + (bounds.size.y - textBounds.size.y) * 0.5f - textBounds.position.y - 1.f
				});

			target.draw(label);
		}

		return;
	}

	sf::RectangleShape fallback;
	fallback.setPosition(bounds.position);
	fallback.setSize(bounds.size);
	fallback.setFillColor(sf::Color(80, 80, 90));
	fallback.setOutlineColor(sf::Color::Black);
	fallback.setOutlineThickness(1.f);

	target.draw(fallback);
}

void GAME1_BombermanLevelEditor::drawTextureFitted(sf::RenderTarget& target,
	const sf::Texture& texture,
	const sf::FloatRect& bounds) const
{
	sf::Sprite sprite(texture);

	const sf::FloatRect localBounds = sprite.getLocalBounds();

	if (localBounds.size.x <= 0.f || localBounds.size.y <= 0.f)
		return;

	sprite.setScale({
		bounds.size.x / localBounds.size.x,
		bounds.size.y / localBounds.size.y
		});

	sprite.setPosition(bounds.position);

	target.draw(sprite);
}

std::filesystem::path GAME1_BombermanLevelEditor::getTemplatePath() const
{
	std::string templateName;

	if (m_worldNumber <= 1)
		templateName = "leveltemplate";
	else
		templateName = "leveltemplate" + std::to_string(m_worldNumber);

	const std::filesystem::path txtPath = getMapsDirectory() / (templateName + ".txt");
	const std::filesystem::path noExtensionPath = getMapsDirectory() / templateName;

	if (std::filesystem::exists(txtPath))
		return txtPath;

	if (std::filesystem::exists(noExtensionPath))
		return noExtensionPath;

	return txtPath;
}

std::filesystem::path GAME1_BombermanLevelEditor::getMapsDirectory() const
{
	return std::filesystem::path(m_mapsDirectory);
}

std::filesystem::path GAME1_BombermanLevelEditor::getTilesDirectory() const
{
	return std::filesystem::path(m_resourcesDirectory) / "Tiles";
}

std::filesystem::path GAME1_BombermanLevelEditor::getCurrentWorldTilesDirectory() const
{
	return getTilesDirectory() / ("World" + std::to_string(m_worldNumber));
}

int GAME1_BombermanLevelEditor::getHighestAvailableWorldNumber() const
{
	namespace fs = std::filesystem;

	int highestWorldNumber = 1;

	const fs::path tilesDirectory = getTilesDirectory();

	if (fs::exists(tilesDirectory) && fs::is_directory(tilesDirectory))
	{
		for (const auto& entry : fs::directory_iterator(tilesDirectory))
		{
			if (!entry.is_directory())
				continue;

			const std::optional<int> worldNumber = TryExtractWorldNumberFromFolder(entry.path());

			if (!worldNumber.has_value())
				continue;

			highestWorldNumber = std::max(highestWorldNumber, worldNumber.value());
		}

		for (int worldNumber = 1; worldNumber <= 20; ++worldNumber)
		{
			const fs::path worldPath = tilesDirectory / ("World" + std::to_string(worldNumber));

			if (fs::exists(worldPath) && fs::is_directory(worldPath))
				highestWorldNumber = std::max(highestWorldNumber, worldNumber);
		}
	}

	const fs::path mapsDirectory = getMapsDirectory();

	if (fs::exists(mapsDirectory) && fs::is_directory(mapsDirectory))
	{
		for (const auto& entry : fs::directory_iterator(mapsDirectory))
		{
			if (!entry.is_regular_file())
				continue;

			const std::optional<int> worldNumber = TryExtractWorldNumberFromTemplate(entry.path());

			if (!worldNumber.has_value())
				continue;

			highestWorldNumber = std::max(highestWorldNumber, worldNumber.value());
		}
	}

	return highestWorldNumber;
}

bool GAME1_BombermanLevelEditor::isValidLevelFile(const std::filesystem::path& path) const
{
	if (!path.has_filename() || path.extension() != ".txt")
		return false;

	const std::string stem = path.stem().string();

	if (stem.rfind("leveltemplate", 0) == 0)
		return false;

	if (stem.rfind("level", 0) != 0)
		return false;

	if (stem.size() <= 5)
		return false;

	for (std::size_t i = 5; i < stem.size(); ++i)
	{
		if (!std::isdigit(static_cast<unsigned char>(stem[i])))
			return false;
	}

	return true;
}

int GAME1_BombermanLevelEditor::extractLevelNumber(const std::filesystem::path& path) const
{
	const std::string stem = path.stem().string();

	if (stem.rfind("level", 0) != 0 || stem.size() <= 5)
		return 0;

	try
	{
		return std::stoi(stem.substr(5));
	}
	catch (...)
	{
		return 0;
	}
}

const std::string& GAME1_BombermanLevelEditor::getLastError() const
{
	return m_lastError;
}

const std::string& GAME1_BombermanLevelEditor::getLastSavedPath() const
{
	return m_lastSavedPath;
}