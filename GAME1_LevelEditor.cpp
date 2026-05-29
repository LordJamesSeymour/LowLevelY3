#include "GAME1_LevelEditor.h"

#include "GAME1_Pickup.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <optional>
#include <sstream>
#include <unordered_set>

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

	std::optional<int> TryExtractWorldNumberFromFolder(const std::filesystem::path& path)
	{
		const std::string name = ToLower(path.filename().string());
		const std::string prefix = "world";

		if (name.rfind(prefix, 0) != 0)
			return std::nullopt;

		if (name.size() <= prefix.size())
			return std::nullopt;

		for (std::size_t i = prefix.size(); i < name.size(); ++i)
		{
			if (!std::isdigit(static_cast<unsigned char>(name[i])))
				return std::nullopt;
		}

		try
		{
			return std::stoi(name.substr(prefix.size()));
		}
		catch (...)
		{
			return std::nullopt;
		}
	}

	std::optional<char> KnownTileCodeForStem(const std::string& rawStem)
	{
		const std::string stem = ToLower(rawStem);

		if (stem == "floor_center_0" ||
			stem == "floor_center" ||
			stem == "floor" ||
			stem == "floortile")
		{
			return 'X';
		}

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
		// O is empty, P is player spawn, B is intentionally unused now,
		// and lowercase fruit pickup codes are reserved.
		return
		{
			'X', 'A', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
			'N', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'Y', 'Z'
		};
	}

	bool IsReservedEditorTileCode(char code)
	{
		GAME1_FruitType ignoredFruitType;

		return code == 'O' ||
			code == 'P' ||
			code == 'e' ||
			code == 'B' ||
			code == '^' ||
			code == '[' ||
			code == '=' ||
			code == ']' ||
			code == 'K' ||
			GAME1_TryGetFruitTypeForMapCode(code, ignoredFruitType);
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

	std::string LabelFromFileStem(std::string stem)
	{
		for (char& c : stem)
		{
			if (c == '_')
				c = ' ';
		}

		return stem;
	}

	std::filesystem::path InferRootDirectoryFromOldFloorPath(const std::string& floorTexturePath)
	{
		namespace fs = std::filesystem;

		const fs::path preferred = fs::path("assets") / "Game#1" / "SurfersQuest";

		if (fs::exists(preferred) && fs::is_directory(preferred))
			return preferred;

		const fs::path floorPath(floorTexturePath);
		fs::path current = floorPath.parent_path();

		for (int i = 0; i < 6 && !current.empty(); ++i)
		{
			if (ToLower(current.filename().string()) == "surfersquest")
				return current;

			current = current.parent_path();
		}

		current = floorPath.parent_path();

		if (ToLower(current.filename().string()) == "resources")
			return current.parent_path();

		return preferred;
	}

	std::optional<char> KeyToCharacter(sf::Keyboard::Key key)
	{
		switch (key)
		{
		case sf::Keyboard::Key::A: return 'A';
		case sf::Keyboard::Key::B: return 'B';
		case sf::Keyboard::Key::C: return 'C';
		case sf::Keyboard::Key::D: return 'D';
		case sf::Keyboard::Key::E: return 'E';
		case sf::Keyboard::Key::F: return 'F';
		case sf::Keyboard::Key::G: return 'G';
		case sf::Keyboard::Key::H: return 'H';
		case sf::Keyboard::Key::I: return 'I';
		case sf::Keyboard::Key::J: return 'J';
		case sf::Keyboard::Key::K: return 'K';
		case sf::Keyboard::Key::L: return 'L';
		case sf::Keyboard::Key::M: return 'M';
		case sf::Keyboard::Key::N: return 'N';
		case sf::Keyboard::Key::O: return 'O';
		case sf::Keyboard::Key::P: return 'P';
		case sf::Keyboard::Key::Q: return 'Q';
		case sf::Keyboard::Key::R: return 'R';
		case sf::Keyboard::Key::S: return 'S';
		case sf::Keyboard::Key::T: return 'T';
		case sf::Keyboard::Key::U: return 'U';
		case sf::Keyboard::Key::V: return 'V';
		case sf::Keyboard::Key::W: return 'W';
		case sf::Keyboard::Key::X: return 'X';
		case sf::Keyboard::Key::Y: return 'Y';
		case sf::Keyboard::Key::Z: return 'Z';
		default: return std::nullopt;
		}
	}

	std::optional<int> KeyToNumberSlot(sf::Keyboard::Key key)
	{
		switch (key)
		{
		case sf::Keyboard::Key::Num1:
		case sf::Keyboard::Key::Numpad1:
			return 1;

		case sf::Keyboard::Key::Num2:
		case sf::Keyboard::Key::Numpad2:
			return 2;

		case sf::Keyboard::Key::Num3:
		case sf::Keyboard::Key::Numpad3:
			return 3;

		case sf::Keyboard::Key::Num4:
		case sf::Keyboard::Key::Numpad4:
			return 4;

		case sf::Keyboard::Key::Num5:
		case sf::Keyboard::Key::Numpad5:
			return 5;

		case sf::Keyboard::Key::Num6:
		case sf::Keyboard::Key::Numpad6:
			return 6;

		case sf::Keyboard::Key::Num7:
		case sf::Keyboard::Key::Numpad7:
			return 7;

		case sf::Keyboard::Key::Num8:
		case sf::Keyboard::Key::Numpad8:
			return 8;

		case sf::Keyboard::Key::Num9:
		case sf::Keyboard::Key::Numpad9:
			return 9;

		default:
			return std::nullopt;
		}
	}
}

bool GAME1_LevelEditor::load(const std::string& fontPath,
	const std::string& surfersQuestRootDirectory)
{
	return initialise(fontPath, surfersQuestRootDirectory, "");
}

bool GAME1_LevelEditor::load(const std::string& floorTexturePath,
	const std::string& ignoredLegacyTexturePath,
	const std::string& fontPath)
{
	(void)ignoredLegacyTexturePath;

	const std::filesystem::path inferredRoot = InferRootDirectoryFromOldFloorPath(floorTexturePath);
	return initialise(fontPath, inferredRoot, "");
}

bool GAME1_LevelEditor::initialise(const std::string& fontPath,
	const std::filesystem::path& rootDirectory,
	const std::string& ignoredLegacyTexturePath)
{
	namespace fs = std::filesystem;

	(void)ignoredLegacyTexturePath;

	m_lastError.clear();
	m_lastSavedPath.clear();
	m_popupMessage.clear();
	m_popupTimer = 0.f;
	m_popupIsError = false;

	m_rootDirectory = rootDirectory.string();
	m_resourcesDirectory = (rootDirectory / "Resources").string();
	m_mapsDirectory = (rootDirectory / "Maps").string();

	if (!m_font.openFromFile(fontPath))
	{
		m_lastError = "Failed to load SurfersQuest editor font: " + fontPath;
		return false;
	}

	if (!fs::exists(getResourcesDirectory()) || !fs::is_directory(getResourcesDirectory()))
	{
		m_lastError =
			"Failed to load SurfersQuest editor: Resources folder does not exist:\n" +
			getResourcesDirectory().string();
		return false;
	}

	m_trapAssets.load(getResourcesDirectory().string());

	refreshSavedLevelList();
	resetEmpty();
	loadWorldBackgroundTexture();
	return true;
}

bool GAME1_LevelEditor::loadTexture(sf::Texture& texture, const std::string& texturePath)
{
	return texture.loadFromFile(texturePath);
}

bool GAME1_LevelEditor::loadFirstTextureFromDirectory(sf::Texture& texture, const std::string& directoryPath)
{
	namespace fs = std::filesystem;

	const fs::path directory(directoryPath);

	if (!fs::exists(directory) || !fs::is_directory(directory))
		return false;

	std::vector<fs::path> paths;

	for (const auto& entry : fs::directory_iterator(directory))
	{
		if (entry.is_regular_file() && IsPngFile(entry.path()))
			paths.push_back(entry.path());
	}

	std::sort(paths.begin(), paths.end(), NaturalFrameSort);

	if (paths.empty())
		return false;

	return texture.loadFromFile(paths.front().string());
}

void GAME1_LevelEditor::resetEmpty()
{
	m_lastError.clear();
	m_lastSavedPath.clear();
	m_popupMessage.clear();
	m_popupTimer = 0.f;
	m_popupIsError = false;

	if (m_worldNumber < 1)
		m_worldNumber = 1;

	m_rows.assign(TotalRows, std::string(TotalCols, 'O'));
	m_objects.clear();
	m_viewStartCol = 0;
	m_viewStartRow = std::max(0, TotalRows - VisibleRows);
	m_hotbarPage = 0;
	m_objectPreviewOrientation = GAME1_TrapOrientation::Up;

	buildTools();
	m_selectedToolIndex = m_tools.empty() ? -1 : 0;
	rebuildVisibleToolbar();
}

void GAME1_LevelEditor::buildTools()
{
	namespace fs = std::filesystem;

	m_tools.clear();

	Tool emptyTool;
	emptyTool.tile = 'O';
	emptyTool.label = "EMPTY";
	emptyTool.description = "Empty / erase tile";
	emptyTool.fallbackColor = sf::Color(90, 160, 230);
	emptyTool.isEraser = true;
	addTool(std::move(emptyTool));

	Tool playerSpawnTool;
	playerSpawnTool.tile = 'P';
	playerSpawnTool.label = "P";
	playerSpawnTool.description = "Player spawn";
	playerSpawnTool.fallbackColor = sf::Color(70, 150, 255);

	const fs::path playerIdleDirectory = getResourcesDirectory() / "Player" / "PlayerIdle";
	const fs::path preferredPlayerIdleFrame = playerIdleDirectory / "PlayerIdle_0.png";

	if (fs::exists(preferredPlayerIdleFrame))
		playerSpawnTool.hasTexture = loadTexture(playerSpawnTool.texture, preferredPlayerIdleFrame.string());
	else
		playerSpawnTool.hasTexture = loadFirstTextureFromDirectory(playerSpawnTool.texture, playerIdleDirectory.string());

	addTool(std::move(playerSpawnTool));

	Tool enemyTool;
	enemyTool.tile = 'e';
	enemyTool.label = "e";
	enemyTool.description = "Enemy spawn";
	enemyTool.fallbackColor = sf::Color(240, 90, 90);

	const fs::path enemyIdleDirectory = getResourcesDirectory() / "Enemies" / "EnemyIdle";
	const fs::path preferredEnemyIdleFrame = enemyIdleDirectory / "EnemyIdle_0.png";

	if (fs::exists(preferredEnemyIdleFrame))
		enemyTool.hasTexture = loadTexture(enemyTool.texture, preferredEnemyIdleFrame.string());
	else
		enemyTool.hasTexture = loadFirstTextureFromDirectory(enemyTool.texture, enemyIdleDirectory.string());

	addTool(std::move(enemyTool));

	const fs::path tilesDirectory = getTilesDirectory();

	auto addSpecialTool = [this](char tile,
		const std::string& label,
		const std::string& description,
		const std::vector<fs::path>& candidatePaths,
		sf::Color fallbackColor)
		{
			Tool specialTool;
			specialTool.tile = tile;
			specialTool.label = label;
			specialTool.description = description;
			specialTool.fallbackColor = fallbackColor;

			for (const fs::path& candidatePath : candidatePaths)
			{
				if (candidatePath.empty())
					continue;

				if (!fs::exists(candidatePath) || !fs::is_regular_file(candidatePath))
					continue;

				if (loadTexture(specialTool.texture, candidatePath.string()))
				{
					specialTool.hasTexture = true;
					break;
				}
			}

			addTool(std::move(specialTool));
		};

	std::vector<fs::path> spikePaths;
	const fs::path preferredSpikePath = tilesDirectory / "Traps" / "Spikes_0.png";
	if (fs::exists(preferredSpikePath))
		spikePaths.push_back(preferredSpikePath);

	const std::vector<fs::path> discoveredSpikePaths =
		FindSpecialPngFiles(tilesDirectory, { "spike" });
	spikePaths.insert(spikePaths.end(), discoveredSpikePaths.begin(), discoveredSpikePaths.end());

	addSpecialTool(
		'^',
		"^",
		"Spike trap - damages and knocks back the player",
		spikePaths,
		sf::Color(210, 210, 220));

	std::vector<fs::path> platformLeftPaths;
	std::vector<fs::path> platformMiddlePaths;
	std::vector<fs::path> platformRightPaths;

	const fs::path platformDirectory = tilesDirectory / "Platform_1";
	const fs::path preferredPlatformLeftPath = platformDirectory / "Platform_Left.png";
	const fs::path preferredPlatformMiddlePath = platformDirectory / "Platform_Center.png";
	const fs::path preferredPlatformRightPath = platformDirectory / "Platform_Right.png";

	if (fs::exists(preferredPlatformLeftPath))
		platformLeftPaths.push_back(preferredPlatformLeftPath);
	if (fs::exists(preferredPlatformMiddlePath))
		platformMiddlePaths.push_back(preferredPlatformMiddlePath);
	if (fs::exists(preferredPlatformRightPath))
		platformRightPaths.push_back(preferredPlatformRightPath);

	const std::vector<fs::path> platformPaths =
		FindSpecialPngFiles(tilesDirectory, { "platform", "oneway", "one_way", "one-way" });

	if (!platformPaths.empty())
	{
		platformLeftPaths.push_back(platformPaths[0]);
		platformMiddlePaths.push_back(platformPaths[std::min<std::size_t>(1, platformPaths.size() - 1)]);
		platformRightPaths.push_back(platformPaths[std::min<std::size_t>(2, platformPaths.size() - 1)]);
	}

	addSpecialTool(
		'[',
		"[",
		"One-way platform left - jump through from below",
		platformLeftPaths,
		sf::Color(185, 150, 90));

	addSpecialTool(
		'=',
		"=",
		"One-way platform middle - hold Left Shift to drop through",
		platformMiddlePaths,
		sf::Color(190, 155, 95));

	addSpecialTool(
		']',
		"]",
		"One-way platform right - jump through from below",
		platformRightPaths,
		sf::Color(185, 150, 90));

	std::vector<fs::path> checkpointIconPaths;
	const fs::path checkpointIconPath =
		getResourcesDirectory() / "Checkpoint" / "FlagOut" / "sprite_09.png";

	if (fs::exists(checkpointIconPath))
		checkpointIconPaths.push_back(checkpointIconPath);

	addSpecialTool(
		'K',
		"K",
		"Checkpoint - updates player respawn point (no backtracking)",
		checkpointIconPaths,
		sf::Color(230, 90, 90));

	for (GAME1_TrapType trapType : GAME1_GetAllTrapTypes())
	{
		Tool trapTool;
		trapTool.kind = ToolKind::Object;
		trapTool.objectType = trapType;
		trapTool.label = GAME1_GetTrapEditorLabel(trapType);
		trapTool.fallbackColor = GAME1_GetTrapFallbackColor(trapType);

		switch (trapType)
		{
		case GAME1_TrapType::FallingPlatform:
			trapTool.description = "Falling Platform - supports player, falls while stood on";
			break;

		case GAME1_TrapType::Fan:
			trapTool.description = "Fan - SHIFT+scroll rotates, pushes 3 tiles";
			break;

		case GAME1_TrapType::Fire:
			trapTool.description = "Fire - SHIFT+scroll rotates, active flame damages";
			break;

		case GAME1_TrapType::Chain:
			trapTool.description = "Chain rail - required for moving platforms";
			break;

		case GAME1_TrapType::BrownMovingPlatform:
			trapTool.description = "Brown moving platform - slow, place on Chain";
			break;

		case GAME1_TrapType::GreyMovingPlatform:
			trapTool.description = "Grey moving platform - fast, place on Chain";
			break;

		default:
			trapTool.description = "Trap object";
			break;
		}

		const sf::Texture* iconTexture = m_trapAssets.getEditorIconTexture(trapType);
		if (iconTexture != nullptr)
		{
			trapTool.texture = *iconTexture;
			trapTool.hasTexture = true;
		}

		addTool(std::move(trapTool));
	}

	for (GAME1_FruitType fruitType : GAME1_GetAllFruitTypes())
	{
		Tool fruitTool;
		fruitTool.tile = GAME1_GetFruitMapCode(fruitType);
		fruitTool.label = std::string(1, fruitTool.tile);
		fruitTool.description =
			GAME1_GetFruitName(fruitType) +
			" pickup - " +
			std::to_string(GAME1_GetFruitPointValue(fruitType)) +
			" points";
		fruitTool.fallbackColor = GAME1_GetFruitFallbackColor(fruitType);

		const fs::path fruitDirectory =
			getResourcesDirectory() / "Pickups" / GAME1_GetFruitName(fruitType);

		fruitTool.hasTexture =
			loadFirstTextureFromDirectory(fruitTool.texture, fruitDirectory.string());

		addTool(std::move(fruitTool));
	}

	const fs::path worldTilesDirectory = getCurrentWorldTilesDirectory();

	std::vector<fs::path> pngPaths;

	if (fs::exists(worldTilesDirectory) && fs::is_directory(worldTilesDirectory))
	{
		for (const auto& entry : fs::directory_iterator(worldTilesDirectory))
		{
			if (entry.is_regular_file() && IsPngFile(entry.path()))
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

			if (IsReservedEditorTileCode(finalCode) ||
				usedCodes.find(finalCode) != usedCodes.end())
			{
				for (char fallback : FallbackTileCodes())
				{
					if (!IsReservedEditorTileCode(fallback) &&
						usedCodes.find(fallback) == usedCodes.end())
					{
						finalCode = fallback;
						break;
					}
				}
			}

			if (IsReservedEditorTileCode(finalCode) ||
				usedCodes.find(finalCode) != usedCodes.end())
			{
				return;
			}

			usedCodes.insert(finalCode);
			usedFiles.insert(path.string());
			definitions.push_back({ finalCode, path });
		};

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
			(IsReservedEditorTileCode(fallbackCodes[fallbackIndex]) ||
				usedCodes.find(fallbackCodes[fallbackIndex]) != usedCodes.end()))
		{
			++fallbackIndex;
		}

		if (fallbackIndex >= fallbackCodes.size())
			break;

		addDefinition(fallbackCodes[fallbackIndex], path);
		++fallbackIndex;
	}

	if (definitions.empty() && !pngPaths.empty())
		definitions.push_back({ 'X', pngPaths.front() });

	for (const auto& definition : definitions)
	{
		Tool tileTool;
		tileTool.tile = definition.first;
		tileTool.label = std::string(1, definition.first);
		tileTool.description =
			"World " +
			std::to_string(m_worldNumber) +
			" - " +
			LabelFromFileStem(definition.second.stem().string());

		tileTool.fallbackColor = sf::Color(90, 180, 90);
		tileTool.hasTexture = loadTexture(tileTool.texture, definition.second.string());

		addTool(std::move(tileTool));
	}

	rebuildVisibleToolbar();
}

void GAME1_LevelEditor::addTool(Tool tool)
{
	m_tools.push_back(std::move(tool));
}

void GAME1_LevelEditor::rebuildVisibleToolbar()
{
	m_visibleToolbarToolIndices.clear();

	const int totalTools = static_cast<int>(m_tools.size());

	const int maxPage = totalTools <= 0
		? 0
		: static_cast<int>((totalTools - 1) / ToolbarSlotCount);

	if (m_hotbarPage < 0)
		m_hotbarPage = 0;

	if (m_hotbarPage > maxPage)
		m_hotbarPage = maxPage;

	const int firstTool = m_hotbarPage * ToolbarSlotCount;
	const int lastToolExclusive = std::min(totalTools, firstTool + ToolbarSlotCount);

	for (int i = firstTool; i < lastToolExclusive; ++i)
	{
		m_visibleToolbarToolIndices.push_back(i);
	}
}

void GAME1_LevelEditor::layout(const sf::RenderWindow& window)
{
	rebuildVisibleToolbar();
	clampViewStartColumn();
	clampViewStartRow();

	m_lastWindowSize = window.getSize();

	const float windowWidth = static_cast<float>(m_lastWindowSize.x);
	const float windowHeight = static_cast<float>(m_lastWindowSize.y);

	const int rows = static_cast<int>(m_rows.size());
	const int totalCols = rows > 0 ? static_cast<int>(m_rows[0].size()) : TotalCols;
	const int visibleCols = std::min(VisibleCols, std::max(1, totalCols));
	const int visibleRows = std::min(VisibleRows, std::max(1, rows));

	const float horizontalMargin = 90.f;
	const float topReserved = 126.f;
	const float bottomReserved = 88.f;

	const float maxGridWidth = std::max(120.f, windowWidth - horizontalMargin * 2.f);
	const float maxGridHeight = std::max(120.f, windowHeight - topReserved - bottomReserved);

	if (rows > 0 && visibleCols > 0)
	{
		const float fitX = maxGridWidth / static_cast<float>(visibleCols);
		const float fitY = maxGridHeight / static_cast<float>(visibleRows);

		m_tileSize = std::floor(std::min({
			static_cast<float>(GameplayTileSize),
			fitX,
			fitY
			}));

		if (m_tileSize < 16.f)
			m_tileSize = 16.f;
	}
	else
	{
		m_tileSize = 42.f;
	}

	const float gridWidth = static_cast<float>(visibleCols) * m_tileSize;
	const float gridHeight = static_cast<float>(visibleRows) * m_tileSize;

	m_gridOrigin = {
		(windowWidth - gridWidth) * 0.5f,
		topReserved
	};

	m_scrollHandleWidth = std::clamp(m_tileSize * 0.35f, 12.f, 22.f);

	m_worldPreviousButtonBounds = sf::FloatRect({ 12.f, 20.f }, { 36.f, 34.f });
	m_worldSelectorBounds = sf::FloatRect({ 52.f, 20.f }, { 126.f, 34.f });
	m_worldNextButtonBounds = sf::FloatRect({ 182.f, 20.f }, { 36.f, 34.f });

	const float rightButtonX = windowWidth - 150.f;

	m_saveButtonBounds = sf::FloatRect({ rightButtonX, 18.f }, { 132.f, 34.f });
	m_loadButtonBounds = sf::FloatRect({ rightButtonX, 58.f }, { 132.f, 30.f });

	const float loadArrowSize = 30.f;
	const float loadSelectorWidth = 76.f;
	const float loadGap = 4.f;
	const float loadTotalWidth =
		loadArrowSize +
		loadGap +
		loadSelectorWidth +
		loadGap +
		loadArrowSize;

	const float loadStartX = windowWidth - loadTotalWidth - 18.f;
	const float loadY = 94.f;

	m_loadPreviousButtonBounds = sf::FloatRect({ loadStartX, loadY }, { loadArrowSize, loadArrowSize });
	m_loadLevelSelectorBounds = sf::FloatRect({ loadStartX + loadArrowSize + loadGap, loadY }, { loadSelectorWidth, loadArrowSize });
	m_loadNextButtonBounds = sf::FloatRect({ loadStartX + loadArrowSize + loadGap + loadSelectorWidth + loadGap, loadY }, { loadArrowSize, loadArrowSize });

	m_toolbarSlotSize = 48.f;
	m_toolbarSlotGap = 6.f;

	const float hotbarArrowWidth = 40.f;
	const float visibleToolbarWidth =
		static_cast<float>(ToolbarSlotCount) * m_toolbarSlotSize +
		static_cast<float>(ToolbarSlotCount - 1) * m_toolbarSlotGap;

	const float totalToolbarWidth =
		hotbarArrowWidth +
		m_toolbarSlotGap +
		visibleToolbarWidth +
		m_toolbarSlotGap +
		hotbarArrowWidth;

	float toolbarY = m_gridOrigin.y + gridHeight + 12.f;

	if (toolbarY + m_toolbarSlotSize > windowHeight - 12.f)
		toolbarY = windowHeight - m_toolbarSlotSize - 12.f;

	float toolbarStartX = m_gridOrigin.x + (gridWidth - totalToolbarWidth) * 0.5f;
	toolbarStartX = std::clamp(
		toolbarStartX,
		8.f,
		std::max(8.f, windowWidth - totalToolbarWidth - 8.f));

	m_previousHotbarPageButtonBounds = sf::FloatRect(
		{ toolbarStartX, toolbarY },
		{ hotbarArrowWidth, m_toolbarSlotSize });

	m_toolbarOrigin = {
		toolbarStartX + hotbarArrowWidth + m_toolbarSlotGap,
		toolbarY
	};

	m_nextHotbarPageButtonBounds = sf::FloatRect(
		{ m_toolbarOrigin.x + visibleToolbarWidth + m_toolbarSlotGap, toolbarY },
		{ hotbarArrowWidth, m_toolbarSlotSize });
}

void GAME1_LevelEditor::update(float deltaTime, sf::Vector2u windowSize)
{
	m_lastWindowSize = windowSize;

	if (m_popupTimer > 0.f)
	{
		m_popupTimer = std::max(0.f, m_popupTimer - deltaTime);

		if (m_popupTimer <= 0.f)
		{
			m_popupMessage.clear();
			m_popupIsError = false;
		}
	}
}

void GAME1_LevelEditor::handleMousePressed(sf::Mouse::Button button, sf::Vector2i mousePixelPosition)
{
	const sf::Vector2f mousePosition(
		static_cast<float>(mousePixelPosition.x),
		static_cast<float>(mousePixelPosition.y));

	if (button == sf::Mouse::Button::Right)
	{
		eraseAtPixel(mousePixelPosition);
		return;
	}

	if (button == sf::Mouse::Button::Middle)
	{
		pickAtPixel(mousePixelPosition);
		return;
	}

	if (button != sf::Mouse::Button::Left)
		return;

	if (containsPoint(m_saveButtonBounds, mousePosition))
	{
		saveToNextLevelFile();
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

	if (containsPoint(m_loadButtonBounds, mousePosition) ||
		containsPoint(m_loadLevelSelectorBounds, mousePosition))
	{
		loadSelectedLevelIntoEditor();
		return;
	}

	if (containsPoint(m_worldPreviousButtonBounds, mousePosition))
	{
		selectPreviousWorld();
		return;
	}

	if (containsPoint(m_worldNextButtonBounds, mousePosition))
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

	if (const std::optional<int> toolbarIndex = getToolbarIndexAtPixel(mousePixelPosition))
	{
		if (toolbarIndex.value() >= 0 &&
			toolbarIndex.value() < static_cast<int>(m_visibleToolbarToolIndices.size()))
		{
			m_selectedToolIndex = m_visibleToolbarToolIndices[toolbarIndex.value()];
		}

		return;
	}

	if (isInsideLeftHandle(mousePixelPosition))
	{
		scrollLeft();
		return;
	}

	if (isInsideRightHandle(mousePixelPosition))
	{
		scrollRight();
		return;
	}

	if (isInsideTopHandle(mousePixelPosition))
	{
		scrollUp();
		return;
	}

	if (isInsideBottomHandle(mousePixelPosition))
	{
		scrollDown();
		return;
	}

	paintAtPixel(mousePixelPosition);
}

void GAME1_LevelEditor::handleMouseWheelScrolled(float delta)
{
	const bool shiftHeld =
		sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift) ||
		sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift);

	if (shiftHeld)
	{
		// SHIFT + wheel rotates the selected rotatable object tool and
		// never scrolls the hotbar.  Main negates the raw delta before
		// calling this, so a wheel-down event arrives here as a positive
		// delta and rotates clockwise.
		const Tool* selectedTool = getSelectedTool();
		const bool rotatable =
			selectedTool != nullptr &&
			selectedTool->kind == ToolKind::Object &&
			GAME1_IsRotatableTrapType(selectedTool->objectType);

		if (!rotatable)
			return;

		if (delta > 0.f)
			rotateSelectedObjectTool(1);
		else if (delta < 0.f)
			rotateSelectedObjectTool(-1);

		return;
	}

	if (delta > 0.f)
	{
		selectNextTool();
	}
	else if (delta < 0.f)
	{
		selectPreviousTool();
	}
}

void GAME1_LevelEditor::handleKeyReleased(sf::Keyboard::Key key)
{
	if (key == sf::Keyboard::Key::Left)
	{
		scrollLeft();
		return;
	}

	if (key == sf::Keyboard::Key::Right)
	{
		scrollRight();
		return;
	}

	if (key == sf::Keyboard::Key::Up)
	{
		scrollUp();
		return;
	}

	if (key == sf::Keyboard::Key::Down)
	{
		scrollDown();
		return;
	}

	if (key == sf::Keyboard::Key::PageUp)
	{
		selectPreviousHotbarPage();
		return;
	}

	if (key == sf::Keyboard::Key::PageDown)
	{
		selectNextHotbarPage();
		return;
	}

	if (key == sf::Keyboard::Key::F5)
	{
		saveToNextLevelFile();
		return;
	}

	if (key == sf::Keyboard::Key::F9)
	{
		loadSelectedLevelIntoEditor();
		return;
	}

	if (key == sf::Keyboard::Key::Delete)
	{
		resetEmpty();
		return;
	}

	if (const std::optional<int> slot = KeyToNumberSlot(key))
	{
		selectToolbarSlot(slot.value());
		return;
	}

	selectToolByHotkey(key);
}

void GAME1_LevelEditor::selectToolbarSlot(int oneBasedVisibleSlot)
{
	if (oneBasedVisibleSlot < 1 || oneBasedVisibleSlot > ToolbarSlotCount)
		return;

	const int visibleIndex = oneBasedVisibleSlot - 1;

	if (visibleIndex < 0 ||
		visibleIndex >= static_cast<int>(m_visibleToolbarToolIndices.size()))
	{
		return;
	}

	m_selectedToolIndex = m_visibleToolbarToolIndices[visibleIndex];
}

void GAME1_LevelEditor::paintAtPixel(sf::Vector2i mousePixelPosition)
{
	const sf::Vector2f mousePosition(
		static_cast<float>(mousePixelPosition.x),
		static_cast<float>(mousePixelPosition.y));

	if (containsPoint(m_saveButtonBounds, mousePosition))
	{
		saveToNextLevelFile();
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

	if (containsPoint(m_loadButtonBounds, mousePosition) ||
		containsPoint(m_loadLevelSelectorBounds, mousePosition))
	{
		loadSelectedLevelIntoEditor();
		return;
	}

	if (containsPoint(m_worldPreviousButtonBounds, mousePosition))
	{
		selectPreviousWorld();
		return;
	}

	if (containsPoint(m_worldNextButtonBounds, mousePosition))
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

	if (const std::optional<int> toolbarIndex = getToolbarIndexAtPixel(mousePixelPosition))
	{
		if (toolbarIndex.value() >= 0 &&
			toolbarIndex.value() < static_cast<int>(m_visibleToolbarToolIndices.size()))
		{
			m_selectedToolIndex = m_visibleToolbarToolIndices[toolbarIndex.value()];
		}

		return;
	}

	if (isInsideLeftHandle(mousePixelPosition))
	{
		scrollLeft();
		return;
	}

	if (isInsideRightHandle(mousePixelPosition))
	{
		scrollRight();
		return;
	}

	if (isInsideTopHandle(mousePixelPosition))
	{
		scrollUp();
		return;
	}

	if (isInsideBottomHandle(mousePixelPosition))
	{
		scrollDown();
		return;
	}

	const std::optional<sf::Vector2i> tilePosition = getTileAtPixel(mousePixelPosition);

	if (!tilePosition.has_value())
		return;

	const Tool* tool = getSelectedTool();

	if (tool == nullptr)
		return;

	if (tool->kind == ToolKind::Object)
	{
		placeObjectAt(tilePosition->x, tilePosition->y, tool->objectType);
	}
	else
	{
		placeTileAt(tilePosition->x, tilePosition->y, tool->tile);
	}
}

void GAME1_LevelEditor::eraseAtPixel(sf::Vector2i mousePixelPosition)
{
	const std::optional<sf::Vector2i> tilePosition = getTileAtPixel(mousePixelPosition);

	if (!tilePosition.has_value())
		return;

	eraseObjectAt(tilePosition->x, tilePosition->y);
	placeTileAt(tilePosition->x, tilePosition->y, 'O');
}

void GAME1_LevelEditor::pickAtPixel(sf::Vector2i mousePixelPosition)
{
	const std::optional<sf::Vector2i> tilePosition = getTileAtPixel(mousePixelPosition);

	if (!tilePosition.has_value())
		return;

	if (const EditorObject* object = getTopObjectAt(tilePosition->x, tilePosition->y))
	{
		if (const Tool* tool = getToolForObject(object->type))
		{
			for (int i = 0; i < static_cast<int>(m_tools.size()); ++i)
			{
				if (&m_tools[i] == tool)
				{
					m_selectedToolIndex = i;
					m_objectPreviewOrientation = object->orientation;
					ensureSelectedToolVisible();
					return;
				}
			}
		}
	}

	selectToolByTile(m_rows[tilePosition->y][tilePosition->x]);
}

bool GAME1_LevelEditor::saveToNextLevelFile()
{
	namespace fs = std::filesystem;

	m_lastError.clear();
	m_lastSavedPath.clear();

	try
	{
		fs::create_directories(getMapsDirectory());

		int highestLevelNumber = 0;

		for (const auto& entry : fs::directory_iterator(getMapsDirectory()))
		{
			if (entry.is_regular_file() && isValidLevelFile(entry.path()))
			{
				highestLevelNumber = std::max(highestLevelNumber, extractLevelNumber(entry.path()));
			}
		}

		const int nextNumber = highestLevelNumber + 1;

		std::ostringstream fileNameStream;
		fileNameStream << "level" << std::setw(2) << std::setfill('0') << nextNumber << ".txt";

		const fs::path savePath = getMapsDirectory() / fileNameStream.str();

		std::ofstream file(savePath);

		if (!file.is_open())
		{
			m_lastError = "Failed to create file: " + savePath.string();
			m_popupMessage = "Save failed";
			m_popupTimer = m_popupDuration;
			m_popupIsError = true;
			return false;
		}

		file << "#WORLD=" << m_worldNumber << '\n';

		for (std::size_t row = 0; row < m_rows.size(); ++row)
		{
			file << m_rows[row];

			if (row + 1 < m_rows.size())
				file << '\n';
		}

		if (!m_objects.empty())
		{
			file << '\n' << "#OBJECTS" << '\n';

			for (const EditorObject& object : m_objects)
			{
				GAME1_TrapSpawn spawn;
				spawn.type = object.type;
				spawn.gridPosition = object.gridPosition;
				spawn.orientation = object.orientation;
				file << GAME1_BuildTrapObjectLine(spawn) << '\n';
			}
		}

		if (!file.good())
		{
			m_lastError = "Failed while writing file: " + savePath.string();
			m_popupMessage = "Save failed";
			m_popupTimer = m_popupDuration;
			m_popupIsError = true;
			return false;
		}

		m_lastSavedPath = savePath.string();
		m_popupMessage = "Level saved: " + savePath.filename().string();
		m_popupTimer = m_popupDuration;
		m_popupIsError = false;

		refreshSavedLevelList();
		return true;
	}
	catch (const std::exception& e)
	{
		m_lastError = std::string("Save failed: ") + e.what();
		m_popupMessage = "Save failed";
		m_popupTimer = m_popupDuration;
		m_popupIsError = true;
		return false;
	}
}

bool GAME1_LevelEditor::loadRowsFromFile(const std::string& mapPath)
{
	m_lastError.clear();

	std::ifstream file(mapPath);

	if (!file.is_open())
	{
		m_lastError = "Failed to open SurfersQuest level file:\n" + mapPath;
		return false;
	}

	std::vector<std::string> rawLines;
	std::vector<EditorObject> loadedObjects;
	std::string line;
	std::size_t widestLine = 0;
	int loadedWorldNumber = 1;
	bool objectSectionStarted = false;

	while (std::getline(file, line))
	{
		if (!line.empty() && line.back() == '\r')
			line.pop_back();

		if (line.empty())
			continue;

		if (TryParseWorldMetadata(line, loadedWorldNumber))
			continue;

		if (line == "#OBJECTS")
		{
			objectSectionStarted = true;
			continue;
		}

		if (!line.empty() && line[0] == '#')
			continue;

		if (objectSectionStarted || line.rfind("OBJECT ", 0) == 0)
		{
			EditorObject object;
			if (!parseObjectLine(line, object))
			{
				m_lastError = "SurfersQuest level file error: invalid object line: " + line;
				return false;
			}

			loadedObjects.push_back(object);
			continue;
		}

		widestLine = std::max(widestLine, line.size());
		rawLines.push_back(line);
	}

	if (rawLines.empty())
	{
		m_lastError = "SurfersQuest level file is empty: " + mapPath;
		return false;
	}

	m_worldNumber = std::max(1, loadedWorldNumber);
	buildTools();
	loadWorldBackgroundTexture();

	std::vector<std::string> loadedRows;
	loadedRows.reserve(rawLines.size());

	for (std::size_t row = 0; row < rawLines.size(); ++row)
	{
		std::string paddedLine = rawLines[row];
		paddedLine.resize(widestLine, 'O');

		for (std::size_t col = 0; col < paddedLine.size(); ++col)
		{
			if (paddedLine[col] == 'B')
			{
				paddedLine[col] = 'O';
				continue;
			}

			if (!validateTileCharacter(paddedLine[col]))
			{
				m_lastError =
					"SurfersQuest level file error: unsupported character '" +
					std::string(1, paddedLine[col]) +
					"' at row " +
					std::to_string(row + 1) +
					", column " +
					std::to_string(col + 1) +
					".";
				return false;
			}
		}

		loadedRows.push_back(std::move(paddedLine));
	}

	m_rows = std::move(loadedRows);
	m_objects = std::move(loadedObjects);
	m_viewStartCol = 0;
	m_viewStartRow = std::max(0, static_cast<int>(m_rows.size()) - VisibleRows);
	m_hotbarPage = 0;
	m_selectedToolIndex = m_tools.empty() ? -1 : 0;

	rebuildVisibleToolbar();
	return true;
}

bool GAME1_LevelEditor::validateTileCharacter(char tile) const
{
	return findToolIndexForTile(tile) >= 0;
}

bool GAME1_LevelEditor::parseObjectLine(const std::string& line, EditorObject& outObject) const
{
	GAME1_TrapSpawn spawn;
	if (!GAME1_TryParseTrapObjectLine(line, spawn))
		return false;

	outObject.type = spawn.type;
	outObject.gridPosition = spawn.gridPosition;
	outObject.orientation = spawn.orientation;
	return true;
}

void GAME1_LevelEditor::refreshSavedLevelList()
{
	namespace fs = std::filesystem;

	m_savedLevelPaths.clear();

	try
	{
		fs::create_directories(getMapsDirectory());

		std::vector<fs::path> paths;

		for (const auto& entry : fs::directory_iterator(getMapsDirectory()))
		{
			if (entry.is_regular_file() && isValidLevelFile(entry.path()))
				paths.push_back(entry.path());
		}

		std::sort(paths.begin(), paths.end(),
			[this](const fs::path& a, const fs::path& b)
			{
				return extractLevelNumber(a) < extractLevelNumber(b);
			});

		for (const fs::path& path : paths)
			m_savedLevelPaths.push_back(path.string());

		if (m_selectedLoadLevelIndex >= static_cast<int>(m_savedLevelPaths.size()))
			m_selectedLoadLevelIndex = static_cast<int>(m_savedLevelPaths.size()) - 1;

		if (m_selectedLoadLevelIndex < 0)
			m_selectedLoadLevelIndex = 0;
	}
	catch (...)
	{
		m_selectedLoadLevelIndex = 0;
	}
}

bool GAME1_LevelEditor::loadSelectedLevelIntoEditor()
{
	m_lastError.clear();
	m_lastSavedPath.clear();

	refreshSavedLevelList();

	if (m_savedLevelPaths.empty())
	{
		m_lastError = "No SurfersQuest level files found in:\n" + getMapsDirectory().string();
		m_popupMessage = "No levels found";
		m_popupTimer = m_popupDuration;
		m_popupIsError = true;
		return false;
	}

	if (m_selectedLoadLevelIndex < 0 ||
		m_selectedLoadLevelIndex >= static_cast<int>(m_savedLevelPaths.size()))
	{
		m_selectedLoadLevelIndex = 0;
	}

	if (!loadRowsFromFile(m_savedLevelPaths[m_selectedLoadLevelIndex]))
	{
		m_popupMessage = "Load failed";
		m_popupTimer = m_popupDuration;
		m_popupIsError = true;
		return false;
	}

	m_popupMessage =
		"Loaded: " +
		std::filesystem::path(m_savedLevelPaths[m_selectedLoadLevelIndex]).filename().string();

	m_popupTimer = m_popupDuration;
	m_popupIsError = false;
	return true;
}

void GAME1_LevelEditor::selectPreviousLoadLevel()
{
	refreshSavedLevelList();

	if (m_savedLevelPaths.empty())
		return;

	if (m_selectedLoadLevelIndex <= 0)
		m_selectedLoadLevelIndex = static_cast<int>(m_savedLevelPaths.size()) - 1;
	else
		--m_selectedLoadLevelIndex;
}

void GAME1_LevelEditor::selectNextLoadLevel()
{
	refreshSavedLevelList();

	if (m_savedLevelPaths.empty())
		return;

	m_selectedLoadLevelIndex =
		(m_selectedLoadLevelIndex + 1) %
		static_cast<int>(m_savedLevelPaths.size());
}

std::string GAME1_LevelEditor::getSelectedLoadLevelName() const
{
	if (m_savedLevelPaths.empty())
		return "<none>";

	if (m_selectedLoadLevelIndex < 0 ||
		m_selectedLoadLevelIndex >= static_cast<int>(m_savedLevelPaths.size()))
	{
		return "<none>";
	}

	return std::filesystem::path(m_savedLevelPaths[m_selectedLoadLevelIndex]).stem().string();
}

void GAME1_LevelEditor::selectPreviousWorld()
{
	const int highestWorld = getHighestAvailableWorldNumber();

	if (highestWorld <= 1)
	{
		m_worldNumber = 1;
	}
	else if (m_worldNumber <= 1)
	{
		m_worldNumber = highestWorld;
	}
	else
	{
		--m_worldNumber;
	}

	m_hotbarPage = 0;
	buildTools();
	m_selectedToolIndex = m_tools.empty() ? -1 : 0;
	rebuildVisibleToolbar();
	loadWorldBackgroundTexture();
}

void GAME1_LevelEditor::selectNextWorld()
{
	const int highestWorld = getHighestAvailableWorldNumber();

	if (highestWorld <= 1)
	{
		m_worldNumber = 1;
	}
	else
	{
		m_worldNumber = (m_worldNumber % highestWorld) + 1;
	}

	m_hotbarPage = 0;
	buildTools();
	m_selectedToolIndex = m_tools.empty() ? -1 : 0;
	rebuildVisibleToolbar();
	loadWorldBackgroundTexture();
}

void GAME1_LevelEditor::loadWorldBackgroundTexture()
{
	namespace fs = std::filesystem;

	m_hasBackgroundTexture = false;

	const fs::path backgroundFile =
		getResourcesDirectory() / "Background" /
		("World" + std::to_string(m_worldNumber)) / "bg.png";

	if (!fs::exists(backgroundFile) || !fs::is_regular_file(backgroundFile))
		return;

	if (m_backgroundTexture.loadFromFile(backgroundFile.string()))
		m_hasBackgroundTexture = true;
}

int GAME1_LevelEditor::getHighestAvailableWorldNumber() const
{
	namespace fs = std::filesystem;

	int highestWorld = 1;
	const fs::path tilesDirectory = getTilesDirectory();

	if (!fs::exists(tilesDirectory) || !fs::is_directory(tilesDirectory))
		return highestWorld;

	for (const auto& entry : fs::directory_iterator(tilesDirectory))
	{
		if (!entry.is_directory())
			continue;

		const std::optional<int> worldNumber = TryExtractWorldNumberFromFolder(entry.path());

		if (worldNumber.has_value())
			highestWorld = std::max(highestWorld, worldNumber.value());
	}

	return highestWorld;
}

void GAME1_LevelEditor::selectPreviousHotbarPage()
{
	const int totalTools = static_cast<int>(m_tools.size());

	if (totalTools <= 0)
		return;

	const int pageCount = static_cast<int>((totalTools - 1) / ToolbarSlotCount) + 1;

	if (pageCount <= 1)
		return;

	--m_hotbarPage;

	if (m_hotbarPage < 0)
		m_hotbarPage = pageCount - 1;

	rebuildVisibleToolbar();
}

void GAME1_LevelEditor::selectNextHotbarPage()
{
	const int totalTools = static_cast<int>(m_tools.size());

	if (totalTools <= 0)
		return;

	const int pageCount = static_cast<int>((totalTools - 1) / ToolbarSlotCount) + 1;

	if (pageCount <= 1)
		return;

	m_hotbarPage = (m_hotbarPage + 1) % pageCount;
	rebuildVisibleToolbar();
}

void GAME1_LevelEditor::selectNextTool()
{
	if (m_tools.empty())
		return;

	m_selectedToolIndex = (m_selectedToolIndex + 1) % static_cast<int>(m_tools.size());
	ensureSelectedToolVisible();
}

void GAME1_LevelEditor::selectPreviousTool()
{
	if (m_tools.empty())
		return;

	--m_selectedToolIndex;

	if (m_selectedToolIndex < 0)
		m_selectedToolIndex = static_cast<int>(m_tools.size()) - 1;

	ensureSelectedToolVisible();
}

void GAME1_LevelEditor::ensureSelectedToolVisible()
{
	if (m_selectedToolIndex < 0)
		return;

	m_hotbarPage = m_selectedToolIndex / ToolbarSlotCount;
	rebuildVisibleToolbar();
}

void GAME1_LevelEditor::selectToolByTile(char tile)
{
	const int toolIndex = findToolIndexForTile(tile);

	if (toolIndex < 0)
		return;

	m_selectedToolIndex = toolIndex;
	ensureSelectedToolVisible();
}

void GAME1_LevelEditor::selectToolByHotkey(sf::Keyboard::Key key)
{
	const std::optional<char> keyCharacter = KeyToCharacter(key);

	if (!keyCharacter.has_value())
		return;

	selectToolByTile(keyCharacter.value());
}

int GAME1_LevelEditor::findToolIndexForTile(char tile) const
{
	for (int i = 0; i < static_cast<int>(m_tools.size()); ++i)
	{
		if (m_tools[i].kind == ToolKind::Tile && m_tools[i].tile == tile)
			return i;
	}

	return -1;
}

void GAME1_LevelEditor::placeTileAt(int col, int row, char tile)
{
	if (row < 0 || row >= static_cast<int>(m_rows.size()))
		return;

	if (col < 0 || col >= static_cast<int>(m_rows[row].size()))
		return;

	if (!validateTileCharacter(tile))
		return;

	if (tile == 'O')
		eraseObjectAt(col, row);

	if (tile == 'P')
	{
		for (std::string& levelRow : m_rows)
		{
			std::replace(levelRow.begin(), levelRow.end(), 'P', 'O');
		}
	}

	m_rows[row][col] = tile;
}

void GAME1_LevelEditor::placeObjectAt(int col, int row, GAME1_TrapType type)
{
	if (row < 0 || row >= static_cast<int>(m_rows.size()))
		return;

	if (col < 0 || col >= static_cast<int>(m_rows[row].size()))
		return;

	if (GAME1_IsMovingPlatformTrapType(type) && !hasChainAt(col, row))
	{
		m_popupMessage = "Place Chain first";
		m_popupTimer = m_popupDuration;
		m_popupIsError = true;
		return;
	}

	if (type == GAME1_TrapType::Chain)
	{
		if (findObjectIndexAt(col, row, GAME1_TrapType::Chain) < 0)
		{
			EditorObject object;
			object.type = type;
			object.gridPosition = { col, row };
			object.orientation = GAME1_TrapOrientation::Up;
			m_objects.push_back(object);
		}

		return;
	}

	if (GAME1_IsMovingPlatformTrapType(type))
	{
		const int brownIndex = findObjectIndexAt(col, row, GAME1_TrapType::BrownMovingPlatform);
		if (brownIndex >= 0)
			m_objects.erase(m_objects.begin() + brownIndex);

		const int greyIndex = findObjectIndexAt(col, row, GAME1_TrapType::GreyMovingPlatform);
		if (greyIndex >= 0)
			m_objects.erase(m_objects.begin() + greyIndex);
	}
	else
	{
		for (int i = static_cast<int>(m_objects.size()) - 1; i >= 0; --i)
		{
			const EditorObject& object = m_objects[static_cast<std::size_t>(i)];

			if (object.gridPosition.x != col || object.gridPosition.y != row)
				continue;

			if (!GAME1_IsMovingPlatformTrapType(object.type) &&
				object.type != GAME1_TrapType::Chain)
			{
				m_objects.erase(m_objects.begin() + i);
			}
		}
	}

	EditorObject object;
	object.type = type;
	object.gridPosition = { col, row };
	object.orientation = GAME1_IsRotatableTrapType(type)
		? m_objectPreviewOrientation
		: GAME1_TrapOrientation::Up;

	m_objects.push_back(object);
}

void GAME1_LevelEditor::eraseObjectAt(int col, int row)
{
	for (int i = static_cast<int>(m_objects.size()) - 1; i >= 0; --i)
	{
		const EditorObject& object = m_objects[static_cast<std::size_t>(i)];

		if (object.gridPosition.x == col && object.gridPosition.y == row)
			m_objects.erase(m_objects.begin() + i);
	}
}

void GAME1_LevelEditor::rotateSelectedObjectTool(int quarterTurnsClockwise)
{
	const Tool* selectedTool = getSelectedTool();

	if (selectedTool == nullptr ||
		selectedTool->kind != ToolKind::Object ||
		!GAME1_IsRotatableTrapType(selectedTool->objectType))
	{
		return;
	}

	m_objectPreviewOrientation =
		GAME1_RotateTrapOrientation(m_objectPreviewOrientation, quarterTurnsClockwise);
}

bool GAME1_LevelEditor::hasChainAt(int col, int row) const
{
	return findObjectIndexAt(col, row, GAME1_TrapType::Chain) >= 0;
}

int GAME1_LevelEditor::findObjectIndexAt(int col,
	int row,
	std::optional<GAME1_TrapType> type) const
{
	for (int i = 0; i < static_cast<int>(m_objects.size()); ++i)
	{
		const EditorObject& object = m_objects[static_cast<std::size_t>(i)];

		if (object.gridPosition.x != col || object.gridPosition.y != row)
			continue;

		if (type.has_value() && object.type != type.value())
			continue;

		return i;
	}

	return -1;
}

const GAME1_LevelEditor::EditorObject* GAME1_LevelEditor::getObjectAt(int col, int row) const
{
	const int index = findObjectIndexAt(col, row);

	if (index < 0)
		return nullptr;

	return &m_objects[static_cast<std::size_t>(index)];
}

const GAME1_LevelEditor::EditorObject* GAME1_LevelEditor::getTopObjectAt(int col, int row) const
{
	for (int i = static_cast<int>(m_objects.size()) - 1; i >= 0; --i)
	{
		const EditorObject& object = m_objects[static_cast<std::size_t>(i)];

		if (object.gridPosition.x == col && object.gridPosition.y == row)
			return &object;
	}

	return nullptr;
}

void GAME1_LevelEditor::scrollLeft()
{
	if (m_viewStartCol > 0)
		--m_viewStartCol;
}

void GAME1_LevelEditor::scrollRight()
{
	const int rows = static_cast<int>(m_rows.size());
	const int cols = rows > 0 ? static_cast<int>(m_rows[0].size()) : 0;
	const int maxStartCol = std::max(0, cols - VisibleCols);

	if (m_viewStartCol < maxStartCol)
		++m_viewStartCol;
}

void GAME1_LevelEditor::clampViewStartColumn()
{
	const int rows = static_cast<int>(m_rows.size());
	const int cols = rows > 0 ? static_cast<int>(m_rows[0].size()) : 0;
	const int maxStartCol = std::max(0, cols - VisibleCols);

	m_viewStartCol = std::clamp(m_viewStartCol, 0, maxStartCol);
}

void GAME1_LevelEditor::scrollUp()
{
	if (m_viewStartRow > 0)
		--m_viewStartRow;
}

void GAME1_LevelEditor::scrollDown()
{
	const int totalRows = static_cast<int>(m_rows.size());
	const int maxStartRow = std::max(0, totalRows - VisibleRows);

	if (m_viewStartRow < maxStartRow)
		++m_viewStartRow;
}

void GAME1_LevelEditor::clampViewStartRow()
{
	const int totalRows = static_cast<int>(m_rows.size());
	const int maxStartRow = std::max(0, totalRows - VisibleRows);

	m_viewStartRow = std::clamp(m_viewStartRow, 0, maxStartRow);
}

bool GAME1_LevelEditor::isInsideLeftHandle(sf::Vector2i mousePixelPosition) const
{
	const sf::Vector2f point(
		static_cast<float>(mousePixelPosition.x),
		static_cast<float>(mousePixelPosition.y));

	const int rows = static_cast<int>(m_rows.size());
	const int visibleRows = std::min(VisibleRows, std::max(1, rows));
	const float gridHeight = static_cast<float>(visibleRows) * m_tileSize;

	const sf::FloatRect bounds(
		m_gridOrigin,
		{ m_scrollHandleWidth, gridHeight });

	return containsPoint(bounds, point);
}

bool GAME1_LevelEditor::isInsideRightHandle(sf::Vector2i mousePixelPosition) const
{
	const sf::Vector2f point(
		static_cast<float>(mousePixelPosition.x),
		static_cast<float>(mousePixelPosition.y));

	const int rows = static_cast<int>(m_rows.size());
	const int totalCols = rows > 0 ? static_cast<int>(m_rows[0].size()) : 0;
	const int visibleCols = std::min(VisibleCols, std::max(1, totalCols));
	const int visibleRows = std::min(VisibleRows, std::max(1, rows));

	const float gridHeight = static_cast<float>(visibleRows) * m_tileSize;
	const float gridWidth = static_cast<float>(visibleCols) * m_tileSize;

	const sf::FloatRect bounds(
		{ m_gridOrigin.x + gridWidth - m_scrollHandleWidth, m_gridOrigin.y },
		{ m_scrollHandleWidth, gridHeight });

	return containsPoint(bounds, point);
}

bool GAME1_LevelEditor::isInsideTopHandle(sf::Vector2i mousePixelPosition) const
{
	const sf::Vector2f point(
		static_cast<float>(mousePixelPosition.x),
		static_cast<float>(mousePixelPosition.y));

	const int rows = static_cast<int>(m_rows.size());
	const int totalCols = rows > 0 ? static_cast<int>(m_rows[0].size()) : 0;
	const int visibleCols = std::min(VisibleCols, std::max(1, totalCols));

	const float gridWidth = static_cast<float>(visibleCols) * m_tileSize;

	const sf::FloatRect bounds(
		m_gridOrigin,
		{ gridWidth, m_scrollHandleWidth });

	return containsPoint(bounds, point);
}

bool GAME1_LevelEditor::isInsideBottomHandle(sf::Vector2i mousePixelPosition) const
{
	const sf::Vector2f point(
		static_cast<float>(mousePixelPosition.x),
		static_cast<float>(mousePixelPosition.y));

	const int rows = static_cast<int>(m_rows.size());
	const int totalCols = rows > 0 ? static_cast<int>(m_rows[0].size()) : 0;
	const int visibleCols = std::min(VisibleCols, std::max(1, totalCols));
	const int visibleRows = std::min(VisibleRows, std::max(1, rows));

	const float gridWidth = static_cast<float>(visibleCols) * m_tileSize;
	const float gridHeight = static_cast<float>(visibleRows) * m_tileSize;

	const sf::FloatRect bounds(
		{ m_gridOrigin.x, m_gridOrigin.y + gridHeight - m_scrollHandleWidth },
		{ gridWidth, m_scrollHandleWidth });

	return containsPoint(bounds, point);
}

std::optional<sf::Vector2i> GAME1_LevelEditor::getTileAtPixel(sf::Vector2i mousePixelPosition) const
{
	if (m_rows.empty())
		return std::nullopt;

	if (isInsideLeftHandle(mousePixelPosition) || isInsideRightHandle(mousePixelPosition) ||
		isInsideTopHandle(mousePixelPosition) || isInsideBottomHandle(mousePixelPosition))
		return std::nullopt;

	const float localX = static_cast<float>(mousePixelPosition.x) - m_gridOrigin.x;
	const float localY = static_cast<float>(mousePixelPosition.y) - m_gridOrigin.y;

	if (localX < 0.f || localY < 0.f)
		return std::nullopt;

	const int visibleCol = static_cast<int>(std::floor(localX / m_tileSize));
	const int visibleRow = static_cast<int>(std::floor(localY / m_tileSize));

	const int totalCols = static_cast<int>(m_rows[0].size());
	const int visibleCols = std::min(VisibleCols, std::max(1, totalCols));

	const int totalRows = static_cast<int>(m_rows.size());
	const int visibleRows = std::min(VisibleRows, std::max(1, totalRows));

	if (visibleCol < 0 ||
		visibleCol >= visibleCols ||
		visibleRow < 0 ||
		visibleRow >= visibleRows)
	{
		return std::nullopt;
	}

	const int worldCol = m_viewStartCol + visibleCol;
	const int worldRow = m_viewStartRow + visibleRow;

	if (worldCol < 0 || worldCol >= totalCols)
		return std::nullopt;

	if (worldRow < 0 || worldRow >= totalRows)
		return std::nullopt;

	return sf::Vector2i{ worldCol, worldRow };
}

std::optional<int> GAME1_LevelEditor::getToolbarIndexAtPixel(sf::Vector2i mousePixelPosition) const
{
	const sf::Vector2f point(
		static_cast<float>(mousePixelPosition.x),
		static_cast<float>(mousePixelPosition.y));

	for (int i = 0; i < ToolbarSlotCount; ++i)
	{
		const sf::FloatRect bounds(
			{
				m_toolbarOrigin.x + static_cast<float>(i) * (m_toolbarSlotSize + m_toolbarSlotGap),
				m_toolbarOrigin.y
			},
			{
				m_toolbarSlotSize,
				m_toolbarSlotSize
			});

			if (containsPoint(bounds, point))
				return i;
	}

	return std::nullopt;
}

bool GAME1_LevelEditor::containsPoint(const sf::FloatRect& bounds, sf::Vector2f point) const
{
	return point.x >= bounds.position.x &&
		point.x <= bounds.position.x + bounds.size.x &&
		point.y >= bounds.position.y &&
		point.y <= bounds.position.y + bounds.size.y;
}

void GAME1_LevelEditor::draw(sf::RenderWindow& window, sf::Vector2i mousePixelPosition)
{
	layout(window);

	const float windowWidth = static_cast<float>(window.getSize().x);
	const float windowHeight = static_cast<float>(window.getSize().y);

	sf::RectangleShape background;
	background.setPosition({ 0.f, 0.f });
	background.setSize({ windowWidth, windowHeight });
	background.setFillColor(sf::Color(22, 24, 32));
	window.draw(background);

	const float titleLeft = m_worldNextButtonBounds.position.x + m_worldNextButtonBounds.size.x + 10.f;
	const float titleRight = m_saveButtonBounds.position.x - 10.f;

	drawTextCentered(
		window,
		"SURFERSQUEST LEVEL EDITOR",
		24,
		sf::FloatRect(
			{ titleLeft, 12.f },
			{ std::max(160.f, titleRight - titleLeft), 32.f }),
		sf::Color::White,
		2.f);

	{
		sf::RectangleShape box;
		box.setPosition(m_worldPreviousButtonBounds.position);
		box.setSize(m_worldPreviousButtonBounds.size);
		box.setFillColor(sf::Color(45, 45, 70));
		box.setOutlineColor(sf::Color::White);
		box.setOutlineThickness(2.f);
		window.draw(box);

		drawTextCentered(window, "<", 28, m_worldPreviousButtonBounds, sf::Color::White, 2.f);
	}

	{
		sf::RectangleShape box;
		box.setPosition(m_worldSelectorBounds.position);
		box.setSize(m_worldSelectorBounds.size);
		box.setFillColor(sf::Color(45, 45, 65));
		box.setOutlineColor(sf::Color::White);
		box.setOutlineThickness(2.f);
		window.draw(box);

		drawTextCentered(
			window,
			"<World " + std::to_string(m_worldNumber) + ">",
			17,
			m_worldSelectorBounds,
			sf::Color(255, 230, 120),
			2.f);
	}

	{
		sf::RectangleShape box;
		box.setPosition(m_worldNextButtonBounds.position);
		box.setSize(m_worldNextButtonBounds.size);
		box.setFillColor(sf::Color(45, 45, 70));
		box.setOutlineColor(sf::Color::White);
		box.setOutlineThickness(2.f);
		window.draw(box);

		drawTextCentered(window, ">", 28, m_worldNextButtonBounds, sf::Color::White, 2.f);
	}

	{
		sf::RectangleShape box;
		box.setPosition(m_saveButtonBounds.position);
		box.setSize(m_saveButtonBounds.size);
		box.setFillColor(sf::Color(40, 120, 60));
		box.setOutlineColor(sf::Color::White);
		box.setOutlineThickness(2.f);
		window.draw(box);

		drawTextCentered(window, "SAVE", 19, m_saveButtonBounds, sf::Color::White, 2.f);
	}

	{
		sf::RectangleShape box;
		box.setPosition(m_loadButtonBounds.position);
		box.setSize(m_loadButtonBounds.size);
		box.setFillColor(sf::Color(45, 70, 120));
		box.setOutlineColor(sf::Color::White);
		box.setOutlineThickness(2.f);
		window.draw(box);

		drawTextCentered(window, "<Load>", 15, m_loadButtonBounds, sf::Color::White, 2.f);
	}

	{
		sf::RectangleShape box;
		box.setPosition(m_loadPreviousButtonBounds.position);
		box.setSize(m_loadPreviousButtonBounds.size);
		box.setFillColor(sf::Color(45, 45, 70));
		box.setOutlineColor(sf::Color::White);
		box.setOutlineThickness(2.f);
		window.draw(box);

		drawTextCentered(window, "<", 22, m_loadPreviousButtonBounds, sf::Color::White, 2.f);
	}

	{
		sf::RectangleShape box;
		box.setPosition(m_loadLevelSelectorBounds.position);
		box.setSize(m_loadLevelSelectorBounds.size);
		box.setFillColor(sf::Color(38, 38, 52));
		box.setOutlineColor(sf::Color::White);
		box.setOutlineThickness(2.f);
		window.draw(box);

		drawTextCentered(window, getSelectedLoadLevelName(), 12, m_loadLevelSelectorBounds, sf::Color(255, 230, 120), 1.5f);
	}

	{
		sf::RectangleShape box;
		box.setPosition(m_loadNextButtonBounds.position);
		box.setSize(m_loadNextButtonBounds.size);
		box.setFillColor(sf::Color(45, 45, 70));
		box.setOutlineColor(sf::Color::White);
		box.setOutlineThickness(2.f);
		window.draw(box);

		drawTextCentered(window, ">", 22, m_loadNextButtonBounds, sf::Color::White, 2.f);
	}

	const Tool* selectedTool = getSelectedTool();

	if (selectedTool != nullptr)
	{
		drawTextCentered(
			window,
			"Selected: " + selectedTool->label + " - " + selectedTool->description,
			17,
			sf::FloatRect({ 0.f, 58.f }, { windowWidth, 26.f }),
			sf::Color(255, 230, 120),
			1.5f);
	}

	const int rows = static_cast<int>(m_rows.size());
	const int totalCols = rows > 0 ? static_cast<int>(m_rows[0].size()) : 0;
	const int visibleCols = std::min(VisibleCols, std::max(1, totalCols));
	const int visibleRows = std::min(VisibleRows, std::max(1, rows));

	if (m_hasBackgroundTexture && visibleCols > 0 && visibleRows > 0)
	{
		const float gridWidth = static_cast<float>(visibleCols) * m_tileSize;
		const float gridHeight = static_cast<float>(visibleRows) * m_tileSize;

		sf::Sprite backgroundSprite(m_backgroundTexture);
		const sf::FloatRect localBounds = backgroundSprite.getLocalBounds();

		if (localBounds.size.x > 0.f && localBounds.size.y > 0.f)
		{
			backgroundSprite.setScale({
				gridWidth / localBounds.size.x,
				gridHeight / localBounds.size.y
				});
			backgroundSprite.setPosition(m_gridOrigin);
			window.draw(backgroundSprite);
		}
	}

	for (int visibleRow = 0; visibleRow < visibleRows; ++visibleRow)
	{
		const int worldRow = m_viewStartRow + visibleRow;

		if (worldRow < 0 || worldRow >= rows)
			continue;

		for (int visibleCol = 0; visibleCol < visibleCols; ++visibleCol)
		{
			const int worldCol = m_viewStartCol + visibleCol;

			if (worldCol < 0 || worldCol >= totalCols)
				continue;

			const sf::FloatRect tileRect(
				{
					m_gridOrigin.x + static_cast<float>(visibleCol) * m_tileSize,
					m_gridOrigin.y + static_cast<float>(visibleRow) * m_tileSize
				},
				{
					m_tileSize,
					m_tileSize
				});

				if (!m_hasBackgroundTexture)
					drawTilePreview(window, 'O', tileRect);

				const char tile = m_rows[worldRow][worldCol];

				if (tile != 'O')
					drawTilePreview(window, tile, tileRect);

				for (const EditorObject& object : m_objects)
				{
					if (object.gridPosition.x == worldCol &&
						object.gridPosition.y == worldRow &&
						object.type == GAME1_TrapType::Chain)
					{
						drawObjectPreview(window, object.type, object.orientation, tileRect);
					}
				}

				for (const EditorObject& object : m_objects)
				{
					if (object.gridPosition.x == worldCol &&
						object.gridPosition.y == worldRow &&
						object.type != GAME1_TrapType::Chain)
					{
						drawObjectPreview(window, object.type, object.orientation, tileRect);
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

	if (rows > 0 && visibleCols > 0)
	{
		const float gridWidth = static_cast<float>(visibleCols) * m_tileSize;
		const float gridHeight = static_cast<float>(visibleRows) * m_tileSize;

		sf::RectangleShape border;
		border.setPosition(m_gridOrigin);
		border.setSize({ gridWidth, gridHeight });
		border.setFillColor(sf::Color::Transparent);
		border.setOutlineColor(sf::Color::White);
		border.setOutlineThickness(3.f);
		window.draw(border);

		sf::RectangleShape leftHandle;
		leftHandle.setPosition(m_gridOrigin);
		leftHandle.setSize({ m_scrollHandleWidth, gridHeight });
		leftHandle.setFillColor(sf::Color(20, 20, 20, 130));
		leftHandle.setOutlineColor(sf::Color::White);
		leftHandle.setOutlineThickness(1.f);
		window.draw(leftHandle);

		sf::RectangleShape rightHandle;
		rightHandle.setPosition({ m_gridOrigin.x + gridWidth - m_scrollHandleWidth, m_gridOrigin.y });
		rightHandle.setSize({ m_scrollHandleWidth, gridHeight });
		rightHandle.setFillColor(sf::Color(20, 20, 20, 130));
		rightHandle.setOutlineColor(sf::Color::White);
		rightHandle.setOutlineThickness(1.f);
		window.draw(rightHandle);

		sf::RectangleShape topHandle;
		topHandle.setPosition(m_gridOrigin);
		topHandle.setSize({ gridWidth, m_scrollHandleWidth });
		topHandle.setFillColor(sf::Color(20, 20, 20, 130));
		topHandle.setOutlineColor(sf::Color::White);
		topHandle.setOutlineThickness(1.f);
		window.draw(topHandle);

		sf::RectangleShape bottomHandle;
		bottomHandle.setPosition({ m_gridOrigin.x, m_gridOrigin.y + gridHeight - m_scrollHandleWidth });
		bottomHandle.setSize({ gridWidth, m_scrollHandleWidth });
		bottomHandle.setFillColor(sf::Color(20, 20, 20, 130));
		bottomHandle.setOutlineColor(sf::Color::White);
		bottomHandle.setOutlineThickness(1.f);
		window.draw(bottomHandle);

		drawTextCentered(
			window,
			"<",
			18,
			sf::FloatRect(m_gridOrigin, { m_scrollHandleWidth, gridHeight }),
			sf::Color::White,
			1.f);

		drawTextCentered(
			window,
			">",
			18,
			sf::FloatRect({ m_gridOrigin.x + gridWidth - m_scrollHandleWidth, m_gridOrigin.y }, { m_scrollHandleWidth, gridHeight }),
			sf::Color::White,
			1.f);

		drawTextCentered(
			window,
			"/\\",
			18,
			sf::FloatRect(m_gridOrigin, { gridWidth, m_scrollHandleWidth }),
			sf::Color::White,
			1.f);

		drawTextCentered(
			window,
			"\\/",
			18,
			sf::FloatRect({ m_gridOrigin.x, m_gridOrigin.y + gridHeight - m_scrollHandleWidth }, { gridWidth, m_scrollHandleWidth }),
			sf::Color::White,
			1.f);

		const int visibleStart = m_viewStartCol + 1;
		const int visibleEnd = std::min(m_viewStartCol + visibleCols, totalCols);

		drawTextCentered(
			window,
			"Cols " + std::to_string(visibleStart) + "-" + std::to_string(visibleEnd) + " / " + std::to_string(totalCols),
			14,
			sf::FloatRect({ m_gridOrigin.x, m_gridOrigin.y - 24.f }, { gridWidth, 22.f }),
			sf::Color(220, 220, 220),
			1.f);

		const int visibleRowStart = m_viewStartRow + 1;
		const int visibleRowEnd = std::min(m_viewStartRow + visibleRows, rows);

		drawTextCentered(
			window,
			"Rows " + std::to_string(visibleRowStart) + "-" + std::to_string(visibleRowEnd) + " / " + std::to_string(rows),
			13,
			sf::FloatRect(
				{ 8.f, m_gridOrigin.y + (gridHeight - 22.f) * 0.5f },
				{ std::max(48.f, m_gridOrigin.x - 16.f), 22.f }),
			sf::Color(220, 220, 220),
			1.f);
	}

	if (const std::optional<sf::Vector2i> hoveredTile = getTileAtPixel(mousePixelPosition))
	{
		const int visibleCol = hoveredTile->x - m_viewStartCol;
		const int visibleRow = hoveredTile->y - m_viewStartRow;
		const sf::FloatRect hoveredTileRect(
			{
				m_gridOrigin.x + static_cast<float>(visibleCol) * m_tileSize,
				m_gridOrigin.y + static_cast<float>(visibleRow) * m_tileSize
			},
			{
				m_tileSize,
				m_tileSize
			});

		const Tool* selectedPreviewTool = getSelectedTool();
		if (selectedPreviewTool != nullptr &&
			selectedPreviewTool->kind == ToolKind::Object)
		{
			drawObjectPreview(
				window,
				selectedPreviewTool->objectType,
				GAME1_IsRotatableTrapType(selectedPreviewTool->objectType)
					? m_objectPreviewOrientation
					: GAME1_TrapOrientation::Up,
				hoveredTileRect);
		}

		sf::RectangleShape hoverRect;
		hoverRect.setPosition(hoveredTileRect.position);
		hoverRect.setSize({ m_tileSize, m_tileSize });
		hoverRect.setFillColor(sf::Color::Transparent);
		hoverRect.setOutlineColor(sf::Color::Yellow);
		hoverRect.setOutlineThickness(3.f);
		window.draw(hoverRect);
	}

	{
		sf::RectangleShape previousPageButton;
		previousPageButton.setPosition(m_previousHotbarPageButtonBounds.position);
		previousPageButton.setSize(m_previousHotbarPageButtonBounds.size);
		previousPageButton.setFillColor(sf::Color(45, 45, 70));
		previousPageButton.setOutlineColor(sf::Color::White);
		previousPageButton.setOutlineThickness(2.f);
		window.draw(previousPageButton);

		drawTextCentered(window, "<", 26, m_previousHotbarPageButtonBounds, sf::Color::White, 2.f);
	}

	for (int visibleIndex = 0; visibleIndex < ToolbarSlotCount; ++visibleIndex)
	{
		const sf::FloatRect slotBounds(
			{
				m_toolbarOrigin.x + static_cast<float>(visibleIndex) * (m_toolbarSlotSize + m_toolbarSlotGap),
				m_toolbarOrigin.y
			},
			{
				m_toolbarSlotSize,
				m_toolbarSlotSize
			});

			sf::RectangleShape slotBox;
			slotBox.setPosition(slotBounds.position);
			slotBox.setSize(slotBounds.size);
			slotBox.setFillColor(sf::Color(35, 35, 45));

			if (visibleIndex < static_cast<int>(m_visibleToolbarToolIndices.size()) &&
				m_visibleToolbarToolIndices[visibleIndex] == m_selectedToolIndex)
			{
				slotBox.setOutlineColor(sf::Color::Yellow);
				slotBox.setOutlineThickness(4.f);
			}
			else
			{
				slotBox.setOutlineColor(sf::Color::White);
				slotBox.setOutlineThickness(2.f);
			}

			window.draw(slotBox);

			if (visibleIndex >= static_cast<int>(m_visibleToolbarToolIndices.size()))
				continue;

			const int toolIndex = m_visibleToolbarToolIndices[visibleIndex];

			if (toolIndex < 0 || toolIndex >= static_cast<int>(m_tools.size()))
				continue;

			drawToolPreview(window, m_tools[toolIndex], slotBounds, visibleIndex + 1);
	}

	{
		sf::RectangleShape nextPageButton;
		nextPageButton.setPosition(m_nextHotbarPageButtonBounds.position);
		nextPageButton.setSize(m_nextHotbarPageButtonBounds.size);
		nextPageButton.setFillColor(sf::Color(45, 45, 70));
		nextPageButton.setOutlineColor(sf::Color::White);
		nextPageButton.setOutlineThickness(2.f);
		window.draw(nextPageButton);

		drawTextCentered(window, ">", 26, m_nextHotbarPageButtonBounds, sf::Color::White, 2.f);
	}

	drawTextCentered(
		window,
		"Left Click: place/select | Right Click: erase | Middle Click: pick | SHIFT + Scroll: rotate Fan/Fire | Scroll: tools | F5: save | F9: load",
		14,
		sf::FloatRect({ 0.f, windowHeight - 26.f }, { windowWidth, 22.f }),
		sf::Color(230, 230, 230),
		1.f);

	if (!m_popupMessage.empty() && m_popupTimer > 0.f)
	{
		const sf::FloatRect popupRect(
			{ windowWidth * 0.5f - 190.f, 90.f },
			{ 380.f, 38.f });

		sf::RectangleShape popupBox;
		popupBox.setPosition(popupRect.position);
		popupBox.setSize(popupRect.size);
		popupBox.setFillColor(m_popupIsError ? sf::Color(130, 45, 45, 230) : sf::Color(45, 120, 60, 230));
		popupBox.setOutlineColor(sf::Color::White);
		popupBox.setOutlineThickness(2.f);
		window.draw(popupBox);

		drawTextCentered(window, m_popupMessage, 16, popupRect, sf::Color::White, 1.5f);
	}
}

void GAME1_LevelEditor::drawText(sf::RenderTarget& target,
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

void GAME1_LevelEditor::drawTextCentered(sf::RenderTarget& target,
	const std::string& string,
	unsigned int size,
	const sf::FloatRect& rect,
	sf::Color fill,
	float outlineThickness) const
{
	sf::Text text(m_font);
	text.setString(string);
	text.setCharacterSize(size);
	text.setFillColor(fill);
	text.setOutlineColor(sf::Color::Black);
	text.setOutlineThickness(outlineThickness);

	const sf::FloatRect bounds = text.getLocalBounds();

	text.setPosition({
		rect.position.x + (rect.size.x - bounds.size.x) * 0.5f - bounds.position.x,
		rect.position.y + (rect.size.y - bounds.size.y) * 0.5f - bounds.position.y - 1.f
		});

	target.draw(text);
}

void GAME1_LevelEditor::drawToolPreview(sf::RenderTarget& target,
	const Tool& tool,
	const sf::FloatRect& bounds,
	int visibleSlotNumber) const
{
	const float padding = 6.f;

	const sf::FloatRect iconBounds(
		{
			bounds.position.x + padding,
			bounds.position.y + padding
		},
		{
			bounds.size.x - padding * 2.f,
			bounds.size.y - padding * 2.f
		});

		if (tool.kind == ToolKind::Object)
		{
			drawObjectPreview(
				target,
				tool.objectType,
				GAME1_IsRotatableTrapType(tool.objectType)
					? m_objectPreviewOrientation
					: GAME1_TrapOrientation::Up,
				iconBounds);
		}
		else if (tool.hasTexture)
		{
			drawTextureFitted(target, tool.texture, iconBounds);
		}
		else
		{
			sf::RectangleShape fallback;
			fallback.setPosition(iconBounds.position);
			fallback.setSize(iconBounds.size);
			fallback.setFillColor(tool.fallbackColor);
			target.draw(fallback);
		}

		drawText(target,
			std::to_string(visibleSlotNumber),
			13,
			{ bounds.position.x + 3.f, bounds.position.y + 1.f },
			sf::Color::White,
			1.5f);

		drawTextCentered(target,
			tool.label,
			13,
			sf::FloatRect(
				{ bounds.position.x, bounds.position.y + bounds.size.y - 18.f },
				{ bounds.size.x, 16.f }),
			sf::Color::White,
			1.5f);
}

void GAME1_LevelEditor::drawTilePreview(sf::RenderTarget& target,
	char tile,
	const sf::FloatRect& bounds) const
{
	if (tile == 'O')
	{
		sf::RectangleShape empty;
		empty.setPosition(bounds.position);
		empty.setSize(bounds.size);
		empty.setFillColor(sf::Color(70, 150, 210));
		target.draw(empty);
		return;
	}

	const Tool* tool = getToolForTile(tile);

	if (tool == nullptr)
		return;

	if (tool->hasTexture)
	{
		drawTextureFitted(target, tool->texture, bounds);
	}
	else
	{
		sf::RectangleShape fallback;
		fallback.setPosition(bounds.position);
		fallback.setSize(bounds.size);
		fallback.setFillColor(tool->fallbackColor);
		target.draw(fallback);
	}
}

void GAME1_LevelEditor::drawObjectPreview(sf::RenderTarget& target,
	GAME1_TrapType type,
	GAME1_TrapOrientation orientation,
	const sf::FloatRect& bounds) const
{
	const sf::Texture* texture = m_trapAssets.getEditorIconTexture(type);

	if (texture != nullptr)
	{
		drawTextureFittedRotated(target, *texture, bounds, orientation);
		return;
	}

	sf::RectangleShape fallback;
	fallback.setPosition(bounds.position);
	fallback.setSize(bounds.size);
	fallback.setFillColor(GAME1_GetTrapFallbackColor(type));
	fallback.setOutlineColor(sf::Color::White);
	fallback.setOutlineThickness(1.f);
	target.draw(fallback);
}

void GAME1_LevelEditor::drawTextureFitted(sf::RenderTarget& target,
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

void GAME1_LevelEditor::drawTextureFittedRotated(sf::RenderTarget& target,
	const sf::Texture& texture,
	const sf::FloatRect& bounds,
	GAME1_TrapOrientation orientation) const
{
	sf::Sprite sprite(texture);

	const sf::FloatRect localBounds = sprite.getLocalBounds();

	if (localBounds.size.x <= 0.f || localBounds.size.y <= 0.f)
		return;

	const bool sideways =
		orientation == GAME1_TrapOrientation::Right ||
		orientation == GAME1_TrapOrientation::Left;

	const float orientedWidth = sideways ? localBounds.size.y : localBounds.size.x;
	const float orientedHeight = sideways ? localBounds.size.x : localBounds.size.y;

	const float scale = std::min(
		bounds.size.x / orientedWidth,
		bounds.size.y / orientedHeight);

	sprite.setOrigin({
		localBounds.position.x + localBounds.size.x * 0.5f,
		localBounds.position.y + localBounds.size.y * 0.5f
		});

	sprite.setPosition({
		bounds.position.x + bounds.size.x * 0.5f,
		bounds.position.y + bounds.size.y * 0.5f
		});

	sprite.setRotation(sf::degrees(static_cast<float>(GAME1_GetTrapOrientationValue(orientation)) * 90.f));
	sprite.setScale({ scale, scale });

	target.draw(sprite);
}

const GAME1_LevelEditor::Tool* GAME1_LevelEditor::getSelectedTool() const
{
	if (m_selectedToolIndex < 0 ||
		m_selectedToolIndex >= static_cast<int>(m_tools.size()))
	{
		return nullptr;
	}

	return &m_tools[m_selectedToolIndex];
}

const GAME1_LevelEditor::Tool* GAME1_LevelEditor::getToolForTile(char tile) const
{
	const int toolIndex = findToolIndexForTile(tile);

	if (toolIndex < 0)
		return nullptr;

	return &m_tools[toolIndex];
}

const GAME1_LevelEditor::Tool* GAME1_LevelEditor::getToolForObject(GAME1_TrapType type) const
{
	for (const Tool& tool : m_tools)
	{
		if (tool.kind == ToolKind::Object && tool.objectType == type)
			return &tool;
	}

	return nullptr;
}

std::filesystem::path GAME1_LevelEditor::getResourcesDirectory() const
{
	return m_resourcesDirectory;
}

std::filesystem::path GAME1_LevelEditor::getMapsDirectory() const
{
	return m_mapsDirectory;
}

std::filesystem::path GAME1_LevelEditor::getTilesDirectory() const
{
	return getResourcesDirectory() / "Tiles";
}

std::filesystem::path GAME1_LevelEditor::getCurrentWorldTilesDirectory() const
{
	return getTilesDirectory() / ("World" + std::to_string(m_worldNumber));
}

bool GAME1_LevelEditor::isValidLevelFile(const std::filesystem::path& path) const
{
	if (!path.has_filename() || path.extension() != ".txt")
		return false;

	const std::string stem = path.stem().string();

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

int GAME1_LevelEditor::extractLevelNumber(const std::filesystem::path& path) const
{
	try
	{
		return std::stoi(path.stem().string().substr(5));
	}
	catch (...)
	{
		return 0;
	}
}

const std::string& GAME1_LevelEditor::getLastError() const
{
	return m_lastError;
}

const std::string& GAME1_LevelEditor::getLastSavedPath() const
{
	return m_lastSavedPath;
}
