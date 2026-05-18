#include "GAME1_LevelEditor.h"

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

		if (name.rfind(prefix, 0) != 0 || name.size() <= prefix.size())
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

	bool IsReservedEditorTileCode(char code)
	{
		return code == 'O' || code == 'B' || code == 'P';
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

		// Old fallback: .../Game#1/Resources/FloorTile.png means root is .../Game#1.
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
	const std::string& breakTexturePath,
	const std::string& fontPath)
{
	const std::filesystem::path inferredRoot = InferRootDirectoryFromOldFloorPath(floorTexturePath);
	return initialise(fontPath, inferredRoot, breakTexturePath);
}

bool GAME1_LevelEditor::initialise(const std::string& fontPath,
	const std::filesystem::path& rootDirectory,
	const std::string& optionalBreakTexturePath)
{
	namespace fs = std::filesystem;

	m_lastError.clear();
	m_lastSavedPath.clear();
	m_popupMessage.clear();
	m_popupTimer = 0.f;
	m_popupIsError = false;

	m_rootDirectory = rootDirectory.string();
	m_resourcesDirectory = (rootDirectory / "Resources").string();
	m_mapsDirectory = (rootDirectory / "Maps").string();
	m_breakTexturePathOverride = optionalBreakTexturePath;

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

	if (!loadBreakTexture())
		return false;

	refreshSavedLevelList();
	resetEmpty();
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

bool GAME1_LevelEditor::loadBreakTexture()
{
	namespace fs = std::filesystem;

	m_hasBreakTexture = false;
	m_loadedBreakTexturePath.clear();

	std::vector<fs::path> candidates;

	if (!m_breakTexturePathOverride.empty())
		candidates.push_back(m_breakTexturePathOverride);

	const fs::path resources = getResourcesDirectory();
	candidates.push_back(resources / "breakblock.png");
	candidates.push_back(resources / "BreakBlock.png");
	candidates.push_back(resources / "Tiles" / "breakblock.png");
	candidates.push_back(resources / "Tiles" / "BreakBlock.png");

	for (const fs::path& path : candidates)
	{
		if (m_breakTexture.loadFromFile(path.string()))
		{
			m_hasBreakTexture = true;
			m_loadedBreakTexturePath = path.string();
			return true;
		}
	}

	m_lastError =
		"Failed to load SurfersQuest break block texture. Tried old path plus Resources/breakblock.png and Resources/Tiles/breakblock.png.";
	return false;
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

	m_rows.assign(Rows, std::string(TotalCols, 'O'));
	m_viewStartCol = 0;
	m_hotbarPage = 0;

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

	Tool breakTool;
	breakTool.tile = 'B';
	breakTool.label = "BREAK";
	breakTool.description = "Breakable block";
	breakTool.fallbackColor = sf::Color(150, 90, 40);
	breakTool.isBreakable = true;
	breakTool.hasTexture = m_hasBreakTexture;

	if (m_hasBreakTexture && !m_loadedBreakTexturePath.empty())
		breakTool.hasTexture = breakTool.texture.loadFromFile(m_loadedBreakTexturePath);

	addTool(std::move(breakTool));

	Tool playerSpawnTool;
	playerSpawnTool.tile = 'P';
	playerSpawnTool.label = "P";
	playerSpawnTool.description = "PlayerSpawn";
	playerSpawnTool.fallbackColor = sf::Color(70, 150, 255);

	const fs::path playerIdleDirectory = getResourcesDirectory() / "Player" / "PlayerIdle";
	const fs::path preferredPlayerIdleFrame = playerIdleDirectory / "PlayerIdle_0.png";

	if (fs::exists(preferredPlayerIdleFrame))
		playerSpawnTool.hasTexture = loadTexture(playerSpawnTool.texture, preferredPlayerIdleFrame.string());
	else
		playerSpawnTool.hasTexture = loadFirstTextureFromDirectory(playerSpawnTool.texture, playerIdleDirectory.string());

	addTool(std::move(playerSpawnTool));

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

			if (IsReservedEditorTileCode(finalCode) || usedCodes.find(finalCode) != usedCodes.end())
			{
				for (char fallback : FallbackTileCodes())
				{
					if (!IsReservedEditorTileCode(fallback) && usedCodes.find(fallback) == usedCodes.end())
					{
						finalCode = fallback;
						break;
					}
				}
			}

			if (IsReservedEditorTileCode(finalCode) || usedCodes.find(finalCode) != usedCodes.end())
				return;

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
		tileTool.description = "World " + std::to_string(m_worldNumber) + " - " + LabelFromFileStem(definition.second.stem().string());
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
		m_visibleToolbarToolIndices.push_back(i);
}

void GAME1_LevelEditor::layout(const sf::RenderWindow& window)
{
	rebuildVisibleToolbar();
	clampViewStartColumn();

	m_lastWindowSize = window.getSize();

	const float windowWidth = static_cast<float>(m_lastWindowSize.x);
	const float windowHeight = static_cast<float>(m_lastWindowSize.y);

	const int rows = static_cast<int>(m_rows.size());
	const int totalCols = rows > 0 ? static_cast<int>(m_rows[0].size()) : TotalCols;
	const int visibleCols = std::min(VisibleCols, std::max(1, totalCols));

	const float horizontalMargin = 90.f;
	const float topReserved = 126.f;
	const float bottomReserved = 88.f;

	// The map itself should be centred in the window.
	// The top-left / top-right UI floats above it, instead of pushing the playable grid sideways.
	const float maxGridWidth = std::max(120.f, windowWidth - horizontalMargin * 2.f);
	const float maxGridHeight = std::max(120.f, windowHeight - topReserved - bottomReserved);

	if (rows > 0 && visibleCols > 0)
	{
		const float fitX = maxGridWidth / static_cast<float>(visibleCols);
		const float fitY = maxGridHeight / static_cast<float>(rows);

		m_tileSize = std::floor(std::min({ static_cast<float>(GameplayTileSize), fitX, fitY }));

		if (m_tileSize < 16.f)
			m_tileSize = 16.f;
	}
	else
	{
		m_tileSize = 42.f;
	}

	const float gridWidth = static_cast<float>(visibleCols) * m_tileSize;
	const float gridHeight = static_cast<float>(rows) * m_tileSize;

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
	const float loadTotalWidth = loadArrowSize + loadGap + loadSelectorWidth + loadGap + loadArrowSize;
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

	// Keep the hotbar centred directly underneath the visible tile map.
	float toolbarStartX = m_gridOrigin.x + (gridWidth - totalToolbarWidth) * 0.5f;
	toolbarStartX = std::clamp(toolbarStartX, 8.f, std::max(8.f, windowWidth - totalToolbarWidth - 8.f));

	m_previousHotbarPageButtonBounds = sf::FloatRect({ toolbarStartX, toolbarY }, { hotbarArrowWidth, m_toolbarSlotSize });
	m_toolbarOrigin = { toolbarStartX + hotbarArrowWidth + m_toolbarSlotGap, toolbarY };
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

	if (containsPoint(m_loadButtonBounds, mousePosition) || containsPoint(m_loadLevelSelectorBounds, mousePosition))
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
		if (toolbarIndex.value() >= 0 && toolbarIndex.value() < static_cast<int>(m_visibleToolbarToolIndices.size()))
			m_selectedToolIndex = m_visibleToolbarToolIndices[toolbarIndex.value()];

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

	paintAtPixel(mousePixelPosition);
}

void GAME1_LevelEditor::handleMouseWheelScrolled(float delta)
{
	if (delta < 0.f)
		selectNextTool();
	else if (delta > 0.f)
		selectPreviousTool();
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

	// Letter keys select the matching tile from the hotbar/tool list.
	// P selects PlayerSpawn, L selects the left floor tile, R selects the right floor tile, etc.
	selectToolByHotkey(key);
}

void GAME1_LevelEditor::selectToolbarSlot(int oneBasedVisibleSlot)
{
	if (oneBasedVisibleSlot < 1 || oneBasedVisibleSlot > ToolbarSlotCount)
		return;

	const int visibleIndex = oneBasedVisibleSlot - 1;

	if (visibleIndex < 0 || visibleIndex >= static_cast<int>(m_visibleToolbarToolIndices.size()))
		return;

	m_selectedToolIndex = m_visibleToolbarToolIndices[visibleIndex];
}

void GAME1_LevelEditor::paintAtPixel(sf::Vector2i mousePixelPosition)
{
	const sf::Vector2f mousePosition(
		static_cast<float>(mousePixelPosition.x),
		static_cast<float>(mousePixelPosition.y));

	// Compatibility path for old main.cpp versions that still call paintAtPixel()
	// directly on left-click instead of forwarding the full mouse event.
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

	if (containsPoint(m_loadButtonBounds, mousePosition) || containsPoint(m_loadLevelSelectorBounds, mousePosition))
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
		if (toolbarIndex.value() >= 0 && toolbarIndex.value() < static_cast<int>(m_visibleToolbarToolIndices.size()))
			m_selectedToolIndex = m_visibleToolbarToolIndices[toolbarIndex.value()];

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

	const std::optional<sf::Vector2i> tilePosition = getTileAtPixel(mousePixelPosition);

	if (!tilePosition.has_value())
		return;

	const Tool* tool = getSelectedTool();
	if (tool == nullptr)
		return;

	placeTileAt(tilePosition->x, tilePosition->y, tool->tile);
}

void GAME1_LevelEditor::eraseAtPixel(sf::Vector2i mousePixelPosition)
{
	const std::optional<sf::Vector2i> tilePosition = getTileAtPixel(mousePixelPosition);

	if (!tilePosition.has_value())
		return;

	placeTileAt(tilePosition->x, tilePosition->y, 'O');
}

void GAME1_LevelEditor::pickAtPixel(sf::Vector2i mousePixelPosition)
{
	const std::optional<sf::Vector2i> tilePosition = getTileAtPixel(mousePixelPosition);

	if (!tilePosition.has_value())
		return;

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
	std::string line;
	std::size_t widestLine = 0;
	int loadedWorldNumber = 1;

	while (std::getline(file, line))
	{
		if (!line.empty() && line.back() == '\r')
			line.pop_back();

		if (line.empty())
			continue;

		if (TryParseWorldMetadata(line, loadedWorldNumber))
			continue;

		if (!line.empty() && line[0] == '#')
			continue;

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

	std::vector<std::string> loadedRows;
	loadedRows.reserve(rawLines.size());

	for (std::size_t row = 0; row < rawLines.size(); ++row)
	{
		std::string paddedLine = rawLines[row];
		paddedLine.resize(widestLine, 'O');

		for (std::size_t col = 0; col < paddedLine.size(); ++col)
		{
			if (!validateTileCharacter(paddedLine[col]))
			{
				m_lastError =
					"SurfersQuest level file error: unsupported character '" +
					std::string(1, paddedLine[col]) +
					"' at row " + std::to_string(row + 1) +
					", column " + std::to_string(col + 1) +
					".";
				return false;
			}
		}

		loadedRows.push_back(std::move(paddedLine));
	}

	m_rows = std::move(loadedRows);
	m_viewStartCol = 0;
	m_hotbarPage = 0;
	m_selectedToolIndex = m_tools.empty() ? -1 : 0;
	rebuildVisibleToolbar();
	return true;
}

bool GAME1_LevelEditor::validateTileCharacter(char tile) const
{
	return findToolIndexForTile(tile) >= 0;
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
		return false;
	}

	if (m_selectedLoadLevelIndex < 0 || m_selectedLoadLevelIndex >= static_cast<int>(m_savedLevelPaths.size()))
		m_selectedLoadLevelIndex = 0;

	return loadRowsFromFile(m_savedLevelPaths[m_selectedLoadLevelIndex]);
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

	m_selectedLoadLevelIndex = (m_selectedLoadLevelIndex + 1) % static_cast<int>(m_savedLevelPaths.size());
}

std::string GAME1_LevelEditor::getSelectedLoadLevelName() const
{
	if (m_savedLevelPaths.empty())
		return "<none>";

	if (m_selectedLoadLevelIndex < 0 || m_selectedLoadLevelIndex >= static_cast<int>(m_savedLevelPaths.size()))
		return "<none>";

	return std::filesystem::path(m_savedLevelPaths[m_selectedLoadLevelIndex]).stem().string();
}

void GAME1_LevelEditor::selectPreviousWorld()
{
	const int highestWorld = getHighestAvailableWorldNumber();

	if (highestWorld <= 1)
	{
		m_worldNumber = 1;
		buildTools();
		return;
	}

	--m_worldNumber;
	if (m_worldNumber < 1)
		m_worldNumber = highestWorld;

	m_hotbarPage = 0;
	buildTools();
	m_selectedToolIndex = m_tools.empty() ? -1 : 0;
}

void GAME1_LevelEditor::selectNextWorld()
{
	const int highestWorld = getHighestAvailableWorldNumber();

	if (highestWorld <= 1)
	{
		m_worldNumber = 1;
		buildTools();
		return;
	}

	++m_worldNumber;
	if (m_worldNumber > highestWorld)
		m_worldNumber = 1;

	m_hotbarPage = 0;
	buildTools();
	m_selectedToolIndex = m_tools.empty() ? -1 : 0;
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
		if (worldNumber.has_value() && worldNumber.value() >= 1)
			highestWorld = std::max(highestWorld, worldNumber.value());
	}

	return highestWorld;
}

void GAME1_LevelEditor::selectPreviousHotbarPage()
{
	const int pageCount = m_tools.empty()
		? 1
		: static_cast<int>((m_tools.size() - 1) / ToolbarSlotCount) + 1;

	if (pageCount <= 1)
		return;

	--m_hotbarPage;
	if (m_hotbarPage < 0)
		m_hotbarPage = pageCount - 1;

	rebuildVisibleToolbar();
}

void GAME1_LevelEditor::selectNextHotbarPage()
{
	const int pageCount = m_tools.empty()
		? 1
		: static_cast<int>((m_tools.size() - 1) / ToolbarSlotCount) + 1;

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
		if (m_tools[i].tile == tile)
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

	if (tile == 'P')
	{
		for (std::string& levelRow : m_rows)
		{
			std::replace(levelRow.begin(), levelRow.end(), 'P', 'O');
		}
	}

	m_rows[row][col] = tile;
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

bool GAME1_LevelEditor::isInsideLeftHandle(sf::Vector2i mousePixelPosition) const
{
	const sf::Vector2f point(static_cast<float>(mousePixelPosition.x), static_cast<float>(mousePixelPosition.y));

	const int rows = static_cast<int>(m_rows.size());
	const float gridHeight = static_cast<float>(rows) * m_tileSize;

	const sf::FloatRect bounds(
		m_gridOrigin,
		{ m_scrollHandleWidth, gridHeight }
	);

	return containsPoint(bounds, point);
}

bool GAME1_LevelEditor::isInsideRightHandle(sf::Vector2i mousePixelPosition) const
{
	const sf::Vector2f point(static_cast<float>(mousePixelPosition.x), static_cast<float>(mousePixelPosition.y));

	const int rows = static_cast<int>(m_rows.size());
	const int totalCols = rows > 0 ? static_cast<int>(m_rows[0].size()) : 0;
	const int visibleCols = std::min(VisibleCols, std::max(1, totalCols));

	const float gridHeight = static_cast<float>(rows) * m_tileSize;
	const float gridWidth = static_cast<float>(visibleCols) * m_tileSize;

	const sf::FloatRect bounds(
		{ m_gridOrigin.x + gridWidth - m_scrollHandleWidth, m_gridOrigin.y },
		{ m_scrollHandleWidth, gridHeight }
	);

	return containsPoint(bounds, point);
}

std::optional<sf::Vector2i> GAME1_LevelEditor::getTileAtPixel(sf::Vector2i mousePixelPosition) const
{
	if (m_rows.empty())
		return std::nullopt;

	if (isInsideLeftHandle(mousePixelPosition) || isInsideRightHandle(mousePixelPosition))
		return std::nullopt;

	const float localX = static_cast<float>(mousePixelPosition.x) - m_gridOrigin.x;
	const float localY = static_cast<float>(mousePixelPosition.y) - m_gridOrigin.y;

	if (localX < 0.f || localY < 0.f)
		return std::nullopt;

	const int visibleCol = static_cast<int>(std::floor(localX / m_tileSize));
	const int row = static_cast<int>(std::floor(localY / m_tileSize));

	const int totalCols = static_cast<int>(m_rows[0].size());
	const int visibleCols = std::min(VisibleCols, std::max(1, totalCols));

	if (visibleCol < 0 || visibleCol >= visibleCols || row < 0 || row >= static_cast<int>(m_rows.size()))
		return std::nullopt;

	const int worldCol = m_viewStartCol + visibleCol;

	if (worldCol < 0 || worldCol >= totalCols)
		return std::nullopt;

	return sf::Vector2i{ worldCol, row };
}

std::optional<int> GAME1_LevelEditor::getToolbarIndexAtPixel(sf::Vector2i mousePixelPosition) const
{
	const sf::Vector2f point(static_cast<float>(mousePixelPosition.x), static_cast<float>(mousePixelPosition.y));

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
			}
		);

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
	drawTextCentered(window,
		"SURFERSQUEST LEVEL EDITOR",
		24,
		sf::FloatRect({ titleLeft, 12.f }, { std::max(160.f, titleRight - titleLeft), 32.f }),
		sf::Color::White,
		2.f);

	// World selector.
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
		drawTextCentered(window, "<World " + std::to_string(m_worldNumber) + ">", 19, m_worldSelectorBounds, sf::Color(255, 230, 120), 2.f);
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

	// Save/load buttons.
	{
		sf::RectangleShape saveButton;
		saveButton.setPosition(m_saveButtonBounds.position);
		saveButton.setSize(m_saveButtonBounds.size);
		saveButton.setFillColor(sf::Color(40, 120, 60));
		saveButton.setOutlineColor(sf::Color::White);
		saveButton.setOutlineThickness(2.f);
		window.draw(saveButton);
		drawTextCentered(window, "SAVE", 22, m_saveButtonBounds, sf::Color::White, 2.f);
	}

	{
		sf::RectangleShape loadButton;
		loadButton.setPosition(m_loadButtonBounds.position);
		loadButton.setSize(m_loadButtonBounds.size);
		loadButton.setFillColor(sf::Color(45, 70, 120));
		loadButton.setOutlineColor(sf::Color::White);
		loadButton.setOutlineThickness(2.f);
		window.draw(loadButton);
		drawTextCentered(window, "<Load>", 19, m_loadButtonBounds, sf::Color::White, 2.f);
	}

	{
		sf::RectangleShape box;
		box.setPosition(m_loadPreviousButtonBounds.position);
		box.setSize(m_loadPreviousButtonBounds.size);
		box.setFillColor(sf::Color(45, 45, 70));
		box.setOutlineColor(sf::Color::White);
		box.setOutlineThickness(2.f);
		window.draw(box);
		drawTextCentered(window, "<", 24, m_loadPreviousButtonBounds, sf::Color::White, 2.f);
	}

	{
		sf::RectangleShape box;
		box.setPosition(m_loadLevelSelectorBounds.position);
		box.setSize(m_loadLevelSelectorBounds.size);
		box.setFillColor(sf::Color(38, 38, 52));
		box.setOutlineColor(sf::Color::White);
		box.setOutlineThickness(2.f);
		window.draw(box);
		drawTextCentered(window, getSelectedLoadLevelName(), 16, m_loadLevelSelectorBounds, sf::Color(255, 230, 120), 1.5f);
	}

	{
		sf::RectangleShape box;
		box.setPosition(m_loadNextButtonBounds.position);
		box.setSize(m_loadNextButtonBounds.size);
		box.setFillColor(sf::Color(45, 45, 70));
		box.setOutlineColor(sf::Color::White);
		box.setOutlineThickness(2.f);
		window.draw(box);
		drawTextCentered(window, ">", 24, m_loadNextButtonBounds, sf::Color::White, 2.f);
	}

	const Tool* selectedTool = getSelectedTool();
	if (selectedTool != nullptr)
	{
		drawTextCentered(window,
			"Selected: " + std::string(1, selectedTool->tile) + " - " + selectedTool->description,
			18,
			sf::FloatRect({ 230.f, 54.f }, { std::max(220.f, m_saveButtonBounds.position.x - 240.f), 24.f }),
			sf::Color(255, 230, 120),
			1.5f);
	}

	const int rows = static_cast<int>(m_rows.size());
	const int totalCols = rows > 0 ? static_cast<int>(m_rows[0].size()) : 0;
	const int visibleCols = std::min(VisibleCols, std::max(1, totalCols));

	const float gridWidth = static_cast<float>(visibleCols) * m_tileSize;
	const float gridHeight = static_cast<float>(rows) * m_tileSize;

	// Sky/grid background.
	{
		sf::RectangleShape sky;
		sky.setPosition(m_gridOrigin);
		sky.setSize({ gridWidth, gridHeight });
		sky.setFillColor(sf::Color(80, 170, 255));
		window.draw(sky);
	}

	// Visible map slice.
	for (int row = 0; row < rows; ++row)
	{
		for (int screenCol = 0; screenCol < visibleCols; ++screenCol)
		{
			const int worldCol = m_viewStartCol + screenCol;
			if (worldCol < 0 || worldCol >= totalCols)
				continue;

			const sf::FloatRect tileRect(
				{
					m_gridOrigin.x + static_cast<float>(screenCol) * m_tileSize,
					m_gridOrigin.y + static_cast<float>(row) * m_tileSize
				},
				{
					m_tileSize,
					m_tileSize
				}
			);

			drawTilePreview(window, m_rows[row][worldCol], tileRect);
		}
	}

	// Red placement grid.
	for (int row = 0; row < rows; ++row)
	{
		for (int screenCol = 0; screenCol < visibleCols; ++screenCol)
		{
			sf::RectangleShape cellOutline;
			cellOutline.setPosition({
				m_gridOrigin.x + static_cast<float>(screenCol) * m_tileSize,
				m_gridOrigin.y + static_cast<float>(row) * m_tileSize
				});
			cellOutline.setSize({ m_tileSize, m_tileSize });
			cellOutline.setFillColor(sf::Color::Transparent);
			cellOutline.setOutlineColor(sf::Color(255, 0, 0, 80));
			cellOutline.setOutlineThickness(-1.f);
			window.draw(cellOutline);
		}
	}

	// Preview tile.
	if (selectedTool != nullptr)
	{
		const std::optional<sf::Vector2i> hoveredTile = getTileAtPixel(mousePixelPosition);

		if (hoveredTile.has_value())
		{
			const int screenCol = hoveredTile->x - m_viewStartCol;

			const sf::FloatRect tileRect(
				{
					m_gridOrigin.x + static_cast<float>(screenCol) * m_tileSize,
					m_gridOrigin.y + static_cast<float>(hoveredTile->y) * m_tileSize
				},
				{ m_tileSize, m_tileSize });

			if (selectedTool->tile != 'O')
			{
				if (selectedTool->hasTexture)
				{
					sf::Sprite sprite(selectedTool->texture);
					const sf::FloatRect localBounds = sprite.getLocalBounds();

					if (localBounds.size.x > 0.f && localBounds.size.y > 0.f)
					{
						sprite.setScale({
							tileRect.size.x / localBounds.size.x,
							tileRect.size.y / localBounds.size.y
							});
						sprite.setPosition(tileRect.position);
						sprite.setColor(sf::Color(255, 255, 255, 170));
						window.draw(sprite);
					}
				}
				else
				{
					sf::RectangleShape preview;
					preview.setPosition(tileRect.position);
					preview.setSize(tileRect.size);
					preview.setFillColor(sf::Color(selectedTool->fallbackColor.r, selectedTool->fallbackColor.g, selectedTool->fallbackColor.b, 170));
					window.draw(preview);
				}
			}
			else
			{
				sf::RectangleShape erasePreview;
				erasePreview.setPosition(tileRect.position);
				erasePreview.setSize(tileRect.size);
				erasePreview.setFillColor(sf::Color(255, 255, 255, 70));
				erasePreview.setOutlineColor(sf::Color::White);
				erasePreview.setOutlineThickness(2.f);
				window.draw(erasePreview);
			}
		}
	}

	// Border.
	{
		sf::RectangleShape border;
		border.setPosition({ m_gridOrigin.x + 2.f, m_gridOrigin.y + 2.f });
		border.setSize({ gridWidth - 4.f, gridHeight - 4.f });
		border.setFillColor(sf::Color::Transparent);
		border.setOutlineColor(sf::Color::Red);
		border.setOutlineThickness(4.f);
		window.draw(border);
	}

	// Horizontal scroll handles.
	{
		sf::RectangleShape leftHandle;
		leftHandle.setPosition(m_gridOrigin);
		leftHandle.setSize({ m_scrollHandleWidth, gridHeight });
		leftHandle.setFillColor(sf::Color(20, 20, 20, 140));
		leftHandle.setOutlineColor(sf::Color::White);
		leftHandle.setOutlineThickness(2.f);
		window.draw(leftHandle);

		sf::ConvexShape leftArrow(3);
		leftArrow.setPoint(0, { m_gridOrigin.x + 7.f, m_gridOrigin.y + gridHeight * 0.5f });
		leftArrow.setPoint(1, { m_gridOrigin.x + m_scrollHandleWidth - 6.f, m_gridOrigin.y + gridHeight * 0.5f - 18.f });
		leftArrow.setPoint(2, { m_gridOrigin.x + m_scrollHandleWidth - 6.f, m_gridOrigin.y + gridHeight * 0.5f + 18.f });
		leftArrow.setFillColor(sf::Color::White);
		window.draw(leftArrow);
	}

	{
		sf::RectangleShape rightHandle;
		rightHandle.setPosition({ m_gridOrigin.x + gridWidth - m_scrollHandleWidth, m_gridOrigin.y });
		rightHandle.setSize({ m_scrollHandleWidth, gridHeight });
		rightHandle.setFillColor(sf::Color(20, 20, 20, 140));
		rightHandle.setOutlineColor(sf::Color::White);
		rightHandle.setOutlineThickness(2.f);
		window.draw(rightHandle);

		const float rightBaseX = m_gridOrigin.x + gridWidth - m_scrollHandleWidth;
		sf::ConvexShape rightArrow(3);
		rightArrow.setPoint(0, { rightBaseX + m_scrollHandleWidth - 7.f, m_gridOrigin.y + gridHeight * 0.5f });
		rightArrow.setPoint(1, { rightBaseX + 6.f, m_gridOrigin.y + gridHeight * 0.5f - 18.f });
		rightArrow.setPoint(2, { rightBaseX + 6.f, m_gridOrigin.y + gridHeight * 0.5f + 18.f });
		rightArrow.setFillColor(sf::Color::White);
		window.draw(rightArrow);
	}

	// Column/slice indicator.
	{
		const int visibleStart = m_viewStartCol + 1;
		const int visibleEnd = std::min(totalCols, m_viewStartCol + visibleCols);

		drawTextCentered(window,
			"Cols " + std::to_string(visibleStart) + "-" + std::to_string(visibleEnd) + " / " + std::to_string(totalCols),
			18,
			sf::FloatRect({ m_gridOrigin.x, m_gridOrigin.y + 8.f }, { gridWidth, 24.f }),
			sf::Color::White,
			2.f);
	}

	// Hotbar arrows and slots.
	{
		sf::RectangleShape previousButton;
		previousButton.setPosition(m_previousHotbarPageButtonBounds.position);
		previousButton.setSize(m_previousHotbarPageButtonBounds.size);
		previousButton.setFillColor(sf::Color(45, 45, 70));
		previousButton.setOutlineColor(sf::Color::White);
		previousButton.setOutlineThickness(2.f);
		window.draw(previousButton);
		drawTextCentered(window, "<", 28, m_previousHotbarPageButtonBounds, sf::Color::White, 2.f);
	}

	for (int i = 0; i < ToolbarSlotCount; ++i)
	{
		const sf::FloatRect slotBounds(
			{
				m_toolbarOrigin.x + static_cast<float>(i) * (m_toolbarSlotSize + m_toolbarSlotGap),
				m_toolbarOrigin.y
			},
			{ m_toolbarSlotSize, m_toolbarSlotSize });

		if (i < static_cast<int>(m_visibleToolbarToolIndices.size()))
		{
			const int toolIndex = m_visibleToolbarToolIndices[i];
			if (toolIndex >= 0 && toolIndex < static_cast<int>(m_tools.size()))
				drawToolPreview(window, m_tools[toolIndex], slotBounds, i + 1);
		}
		else
		{
			sf::RectangleShape emptySlot;
			emptySlot.setPosition(slotBounds.position);
			emptySlot.setSize(slotBounds.size);
			emptySlot.setFillColor(sf::Color(30, 30, 38, 180));
			emptySlot.setOutlineColor(sf::Color(100, 100, 110));
			emptySlot.setOutlineThickness(1.f);
			window.draw(emptySlot);
		}
	}

	{
		sf::RectangleShape nextButton;
		nextButton.setPosition(m_nextHotbarPageButtonBounds.position);
		nextButton.setSize(m_nextHotbarPageButtonBounds.size);
		nextButton.setFillColor(sf::Color(45, 45, 70));
		nextButton.setOutlineColor(sf::Color::White);
		nextButton.setOutlineThickness(2.f);
		window.draw(nextButton);
		drawTextCentered(window, ">", 28, m_nextHotbarPageButtonBounds, sf::Color::White, 2.f);
	}

	const int totalPages = m_tools.empty()
		? 1
		: static_cast<int>((m_tools.size() - 1) / ToolbarSlotCount) + 1;

	drawTextCentered(window,
		"Hotbar " + std::to_string(m_hotbarPage + 1) + " / " + std::to_string(totalPages) +
		"    Left place/select | Right erase | Middle pick | Wheel tool | F5 save | F9 load | Delete reset",
		12,
		sf::FloatRect({ 0.f, m_toolbarOrigin.y - 25.f }, { windowWidth, 20.f }),
		sf::Color(230, 230, 230),
		1.f);

	// Temporary in-editor save popup.
	if (m_popupTimer > 0.f && !m_popupMessage.empty())
	{
		const float popupWidth = 430.f;
		const float popupHeight = 72.f;
		const sf::FloatRect popupBounds(
			{ (windowWidth - popupWidth) * 0.5f, windowHeight * 0.5f - popupHeight * 0.5f },
			{ popupWidth, popupHeight });

		sf::RectangleShape shadow;
		shadow.setPosition({ popupBounds.position.x + 6.f, popupBounds.position.y + 6.f });
		shadow.setSize(popupBounds.size);
		shadow.setFillColor(sf::Color(0, 0, 0, 150));
		window.draw(shadow);

		sf::RectangleShape popup;
		popup.setPosition(popupBounds.position);
		popup.setSize(popupBounds.size);
		popup.setFillColor(m_popupIsError ? sf::Color(120, 35, 35, 235) : sf::Color(35, 115, 55, 235));
		popup.setOutlineColor(sf::Color::White);
		popup.setOutlineThickness(3.f);
		window.draw(popup);

		drawTextCentered(window,
			m_popupMessage,
			22,
			popupBounds,
			sf::Color::White,
			2.f);
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

	unsigned int fittedSize = size;
	while (fittedSize > 9)
	{
		const sf::FloatRect testBounds = text.getLocalBounds();
		if (testBounds.size.x <= rect.size.x - 6.f && testBounds.size.y <= rect.size.y + 8.f)
			break;

		--fittedSize;
		text.setCharacterSize(fittedSize);
	}

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
	sf::RectangleShape slotRect;
	slotRect.setPosition(bounds.position);
	slotRect.setSize(bounds.size);
	slotRect.setFillColor(sf::Color(40, 40, 48, 230));

	const bool isSelected = getSelectedTool() == &tool;

	if (isSelected)
	{
		slotRect.setOutlineColor(sf::Color::Yellow);
		slotRect.setOutlineThickness(4.f);
	}
	else
	{
		slotRect.setOutlineColor(sf::Color(180, 180, 180));
		slotRect.setOutlineThickness(2.f);
	}

	target.draw(slotRect);

	const sf::FloatRect iconBounds(
		{ bounds.position.x + 6.f, bounds.position.y + 6.f },
		{ bounds.size.x - 12.f, bounds.size.y - 12.f }
	);

	if (tool.hasTexture)
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

	// Number key indicator.
	drawText(target,
		std::to_string(visibleSlotNumber),
		13,
		{ bounds.position.x + 4.f, bounds.position.y + 1.f },
		sf::Color::White,
		1.f);

	// Tile-letter hotkey / saved map character.
	drawText(target,
		std::string(1, tool.tile),
		17,
		{ bounds.position.x + bounds.size.x - 16.f, bounds.position.y + bounds.size.y - 22.f },
		sf::Color(255, 230, 120),
		1.5f);
}

void GAME1_LevelEditor::drawTilePreview(sf::RenderTarget& target,
	char tile,
	const sf::FloatRect& bounds) const
{
	if (tile == 'O')
		return;

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

const GAME1_LevelEditor::Tool* GAME1_LevelEditor::getSelectedTool() const
{
	if (m_selectedToolIndex < 0 || m_selectedToolIndex >= static_cast<int>(m_tools.size()))
		return nullptr;

	return &m_tools[m_selectedToolIndex];
}

const GAME1_LevelEditor::Tool* GAME1_LevelEditor::getToolForTile(char tile) const
{
	const int toolIndex = findToolIndexForTile(tile);

	if (toolIndex < 0)
		return nullptr;

	return &m_tools[toolIndex];
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
