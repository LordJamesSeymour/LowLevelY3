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

	buildTools();

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

	if (!loadRowsFromFile(getTemplatePath().string()))
		return false;

	m_selectedToolIndex = 0;

	return true;
}

void GAME1_BombermanLevelEditor::buildTools()
{
	namespace fs = std::filesystem;

	m_tools.clear();

	const fs::path resourcesPath = fs::path(m_resourcesDirectory);
	const fs::path tilesPath = resourcesPath / "Tiles";
	const fs::path enemiesPath = resourcesPath / "Enemies";
	const fs::path playerPath = resourcesPath / "Player" / "Blue";

	auto addTool = [this](char tile,
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

			if (!texturePath.empty())
			{
				tool.hasTexture = loadTexture(tool.texture, texturePath);
			}

			m_tools.push_back(std::move(tool));
		};

	auto addDirectoryTool = [this](char tile,
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
			tool.hasTexture = loadFirstTextureFromDirectory(tool.texture, directoryPath);

			m_tools.push_back(std::move(tool));
		};

	// Slot 1
	addTool(
		' ',
		"EMPTY",
		"Empty floor / erase",
		(tilesPath / "floor.png").string(),
		sf::Color(55, 120, 55)
	);

	// Slot 2
	addDirectoryTool(
		'B',
		"B",
		"Breakable block",
		(tilesPath / "Breakable").string(),
		sf::Color(150, 90, 40)
	);

	// Slot 3
	addDirectoryTool(
		'P',
		"P",
		"Player spawn",
		(playerPath / "Front").string(),
		sf::Color(70, 150, 255)
	);

	// Slot 4
	addDirectoryTool(
		'O',
		"O",
		"Copter enemy",
		(enemiesPath / "Copter" / "Front").string(),
		sf::Color(255, 80, 80)
	);

	// Slot 5
	// A = Lamp enemy
	addDirectoryTool(
		'A',
		"A",
		"Lamp enemy",
		(enemiesPath / "Lamp").string(),
		sf::Color(255, 220, 70)
	);

	// Slot 6
	addDirectoryTool(
		'E',
		"E",
		"Exit marker",
		(tilesPath / "Exit").string(),
		sf::Color(255, 230, 80)
	);

	// Slot 7
	addTool(
		'M',
		"M",
		"Solid block",
		(tilesPath / "solidblock.png").string(),
		sf::Color(120, 120, 120)
	);

	// Slot 8
	addTool(
		'U',
		"U",
		"Solid wall up",
		(tilesPath / "solidwall_up.png").string(),
		sf::Color(130, 130, 130)
	);

	// Slot 9
	addTool(
		'D',
		"D",
		"Solid wall down",
		(tilesPath / "solidwall_down.png").string(),
		sf::Color(130, 130, 130)
	);

	addTool(
		'L',
		"L",
		"Solid wall left",
		(tilesPath / "solidwall_left.png").string(),
		sf::Color(130, 130, 130)
	);

	addTool(
		'R',
		"R",
		"Solid wall right",
		(tilesPath / "solidwall_right.png").string(),
		sf::Color(130, 130, 130)
	);

	addTool(
		'T',
		"T",
		"Solid wall top",
		(tilesPath / "solidwall_top.png").string(),
		sf::Color(130, 130, 130)
	);

	addTool(
		'Q',
		"Q",
		"Solid wall top-left",
		(tilesPath / "solidwall_topleft.png").string(),
		sf::Color(130, 130, 130)
	);

	addTool(
		'Y',
		"Y",
		"Solid wall top-right",
		(tilesPath / "solidwall_topright.png").string(),
		sf::Color(130, 130, 130)
	);

	addTool(
		'Z',
		"Z",
		"Solid wall bottom-left",
		(tilesPath / "solidwall_botleft.png").string(),
		sf::Color(130, 130, 130)
	);

	addTool(
		'C',
		"C",
		"Solid wall bottom-right",
		(tilesPath / "solidwall_botright.png").string(),
		sf::Color(130, 130, 130)
	);
}

bool GAME1_BombermanLevelEditor::loadRowsFromFile(const std::string& mapPath)
{
	m_rows.clear();

	std::ifstream file(mapPath);

	if (!file.is_open())
	{
		m_lastError =
			"Failed to open Bomberman level template:\n" +
			mapPath +
			"\n\nCreate this file inside the Bomberman Maps folder.";
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
		m_lastError = "Bomberman level template is empty: " + mapPath;
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
					"Bomberman template error: unsupported character '" +
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
	case 'E':
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

	const float totalToolbarWidth =
		static_cast<float>(m_tools.size()) * m_toolbarSlotSize +
		static_cast<float>(m_tools.size() > 0 ? m_tools.size() - 1 : 0) * m_toolbarSlotGap;

	float toolbarY = m_gridOrigin.y + gridHeight + 12.f;

	if (toolbarY + m_toolbarSlotSize > windowHeight - 12.f)
		toolbarY = windowHeight - m_toolbarSlotSize - 12.f;

	m_toolbarOrigin = {
		(windowWidth - totalToolbarWidth) * 0.5f,
		toolbarY
	};
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

	sf::RectangleShape saveButton;
	saveButton.setPosition(m_saveButtonBounds.position);
	saveButton.setSize(m_saveButtonBounds.size);
	saveButton.setFillColor(sf::Color(40, 120, 60));
	saveButton.setOutlineColor(sf::Color::White);
	saveButton.setOutlineThickness(2.f);
	window.draw(saveButton);

	drawTextCentered("SAVE", 22, m_saveButtonBounds, sf::Color::White);

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

	for (std::size_t i = 0; i < m_tools.size(); ++i)
	{
		const sf::FloatRect slotRect(
			{
				m_toolbarOrigin.x + static_cast<float>(i) * (m_toolbarSlotSize + m_toolbarSlotGap),
				m_toolbarOrigin.y
			},
			{
				m_toolbarSlotSize,
				m_toolbarSlotSize
			}
		);

		const Tool& tool = m_tools[i];

		sf::RectangleShape slot;
		slot.setPosition(slotRect.position);
		slot.setSize(slotRect.size);
		slot.setFillColor(sf::Color(42, 42, 50));

		if (static_cast<int>(i) == m_selectedToolIndex)
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

	const float controlsY = std::min(windowHeight - 28.f, m_toolbarOrigin.y + m_toolbarSlotSize + 8.f);

	drawTextCentered(
		"Controls: Left Click = place/select toolbar    Right Click = erase    Middle Mouse = color pick    P = save    R = reset template    1-9 = toolbar slots",
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

		for (std::size_t i = 0; i < m_tools.size(); ++i)
		{
			const sf::FloatRect slotRect(
				{
					m_toolbarOrigin.x + static_cast<float>(i) * (m_toolbarSlotSize + m_toolbarSlotGap),
					m_toolbarOrigin.y
				},
				{
					m_toolbarSlotSize,
					m_toolbarSlotSize
				}
			);

			if (containsPoint(slotRect, mousePosition))
			{
				m_selectedToolIndex = static_cast<int>(i);
				return;
			}
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

void GAME1_BombermanLevelEditor::handleKeyReleased(sf::Keyboard::Key key)
{
	switch (key)
	{
	case sf::Keyboard::Key::P:
		saveToNextLevelFile();
		break;

	case sf::Keyboard::Key::R:
		resetFromTemplate();
		break;

	case sf::Keyboard::Key::Num1:
	case sf::Keyboard::Key::Numpad1:
		selectToolbarSlot(1);
		break;

	case sf::Keyboard::Key::Num2:
	case sf::Keyboard::Key::Numpad2:
		selectToolbarSlot(2);
		break;

	case sf::Keyboard::Key::Num3:
	case sf::Keyboard::Key::Numpad3:
		selectToolbarSlot(3);
		break;

	case sf::Keyboard::Key::Num4:
	case sf::Keyboard::Key::Numpad4:
		selectToolbarSlot(4);
		break;

	case sf::Keyboard::Key::Num5:
	case sf::Keyboard::Key::Numpad5:
		selectToolbarSlot(5);
		break;

	case sf::Keyboard::Key::Num6:
	case sf::Keyboard::Key::Numpad6:
		selectToolbarSlot(6);
		break;

	case sf::Keyboard::Key::Num7:
	case sf::Keyboard::Key::Numpad7:
		selectToolbarSlot(7);
		break;

	case sf::Keyboard::Key::Num8:
	case sf::Keyboard::Key::Numpad8:
		selectToolbarSlot(8);
		break;

	case sf::Keyboard::Key::Num9:
	case sf::Keyboard::Key::Numpad9:
		selectToolbarSlot(9);
		break;

	default:
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
	const int index = slotNumber - 1;

	if (index < 0 || index >= static_cast<int>(m_tools.size()))
		return;

	m_selectedToolIndex = index;
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

		if (!m_tools[toolIndex].hasTexture || tile == 'P' || tile == 'O' || tile == 'A' || tile == 'E')
		{
			sf::Text label(m_font);
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
	return getMapsDirectory() / "leveltemplate.txt";
}

std::filesystem::path GAME1_BombermanLevelEditor::getMapsDirectory() const
{
	return std::filesystem::path(m_mapsDirectory);
}

bool GAME1_BombermanLevelEditor::isValidLevelFile(const std::filesystem::path& path) const
{
	if (!path.has_filename() || path.extension() != ".txt")
		return false;

	const std::string stem = path.stem().string();

	if (stem == "leveltemplate")
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