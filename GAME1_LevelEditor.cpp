#include "GAME1_LevelEditor.h"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace
{
	const std::filesystem::path kGame1MapsDirectory = "Assets/Game#1/Maps";

	bool IsValidLevelFile(const std::filesystem::path& path)
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
}

bool GAME1_LevelEditor::load(const std::string& floorTexturePath,
	const std::string& breakTexturePath,
	const std::string& fontPath)
{
	m_lastError.clear();
	m_lastSavedPath.clear();

	if (!m_floorTexture.loadFromFile(floorTexturePath))
	{
		m_lastError = "Failed to load floor texture: " + floorTexturePath;
		return false;
	}

	if (!m_breakTexture.loadFromFile(breakTexturePath))
	{
		m_lastError = "Failed to load break block texture: " + breakTexturePath;
		return false;
	}

	if (!m_font.openFromFile(fontPath))
	{
		m_lastError = "Failed to load font: " + fontPath;
		return false;
	}

	resetEmpty();
	return true;
}

void GAME1_LevelEditor::resetEmpty()
{
	m_rows.assign(Rows, std::string(TotalCols, 'O'));

	// Toolbar slot 1 starts selected by default, similar to many inventory UIs.
	m_selectedToolbarSlot = 0;
	m_activeBrush = m_toolbarBrushes[m_selectedToolbarSlot];

	m_viewStartCol = 0;
	m_lastError.clear();
	m_lastSavedPath.clear();
}

void GAME1_LevelEditor::selectToolbarSlot(int oneBasedSlot)
{
	if (oneBasedSlot < 1 || oneBasedSlot > ToolbarSlotCount)
		return;

	m_selectedToolbarSlot = oneBasedSlot - 1;
	m_activeBrush = m_toolbarBrushes[m_selectedToolbarSlot];
}

void GAME1_LevelEditor::scrollLeft()
{
	if (m_viewStartCol > 0)
		--m_viewStartCol;
}

void GAME1_LevelEditor::scrollRight()
{
	const int maxStartCol = TotalCols - VisibleCols;
	if (m_viewStartCol < maxStartCol)
		++m_viewStartCol;
}

bool GAME1_LevelEditor::isInsideLeftHandle(sf::Vector2i mousePixelPosition) const
{
	return mousePixelPosition.x >= 0 &&
		mousePixelPosition.x < HandleWidth &&
		mousePixelPosition.y >= 0 &&
		mousePixelPosition.y < Rows * TileSize;
}

bool GAME1_LevelEditor::isInsideRightHandle(sf::Vector2i mousePixelPosition) const
{
	return mousePixelPosition.x >= VisibleCols * TileSize - HandleWidth &&
		mousePixelPosition.x < VisibleCols * TileSize &&
		mousePixelPosition.y >= 0 &&
		mousePixelPosition.y < Rows * TileSize;
}

bool GAME1_LevelEditor::isInsideVisibleGridPixel(sf::Vector2i mousePixelPosition) const
{
	return mousePixelPosition.x >= 0 &&
		mousePixelPosition.x < VisibleCols * TileSize &&
		mousePixelPosition.y >= 0 &&
		mousePixelPosition.y < Rows * TileSize;
}

bool GAME1_LevelEditor::tryGetHoveredCell(sf::Vector2i mousePixelPosition, int& outWorldCol, int& outRow) const
{
	if (!isInsideVisibleGridPixel(mousePixelPosition))
		return false;

	if (isInsideLeftHandle(mousePixelPosition) || isInsideRightHandle(mousePixelPosition))
		return false;

	const int visibleCol = mousePixelPosition.x / TileSize;
	const int worldCol = visibleCol + m_viewStartCol;
	const int row = mousePixelPosition.y / TileSize;

	if (worldCol < 0 || worldCol >= TotalCols || row < 0 || row >= Rows)
		return false;

	outWorldCol = worldCol;
	outRow = row;
	return true;
}

const sf::Texture* GAME1_LevelEditor::getTextureForBrush(Brush brush) const
{
	switch (brush)
	{
	case Brush::Floor:
		return &m_floorTexture;
	case Brush::Breakable:
		return &m_breakTexture;
	case Brush::None:
	default:
		return nullptr;
	}
}

void GAME1_LevelEditor::paintAtPixel(sf::Vector2i mousePixelPosition)
{
	if (mousePixelPosition.x < 0 || mousePixelPosition.y < 0)
		return;

	// Side handles move the visible 16-column window through the wider map.
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

	if (m_activeBrush == Brush::None)
		return;

	int worldCol = 0;
	int row = 0;

	if (!tryGetHoveredCell(mousePixelPosition, worldCol, row))
		return;

	if (m_rows[row][worldCol] != 'O')
		return;

	if (m_activeBrush == Brush::Floor)
		m_rows[row][worldCol] = 'X';
	else if (m_activeBrush == Brush::Breakable)
		m_rows[row][worldCol] = 'B';
}

bool GAME1_LevelEditor::saveToNextLevelFile()
{
	namespace fs = std::filesystem;

	m_lastError.clear();
	m_lastSavedPath.clear();

	try
	{
		fs::create_directories(kGame1MapsDirectory);

		int levelCount = 0;

		for (const auto& entry : fs::directory_iterator(kGame1MapsDirectory))
		{
			if (entry.is_regular_file() && IsValidLevelFile(entry.path()))
			{
				++levelCount;
			}
		}

		const int nextNumber = levelCount + 1;

		std::ostringstream fileNameStream;
		fileNameStream << "level" << std::setw(2) << std::setfill('0') << nextNumber << ".txt";

		const fs::path savePath = kGame1MapsDirectory / fileNameStream.str();

		std::ofstream file(savePath);
		if (!file.is_open())
		{
			m_lastError = "Failed to create file: " + savePath.string();
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
			m_lastError = "Failed while writing file: " + savePath.string();
			return false;
		}

		m_lastSavedPath = savePath.string();
		return true;
	}
	catch (const std::exception& e)
	{
		m_lastError = std::string("Save failed: ") + e.what();
		return false;
	}
}

void GAME1_LevelEditor::draw(sf::RenderWindow& window, sf::Vector2i mousePixelPosition) const
{
	// -------------------------------------------------------------------------
	// 1) Draw the currently visible slice of the level
	// -------------------------------------------------------------------------
	for (int row = 0; row < Rows; ++row)
	{
		for (int screenCol = 0; screenCol < VisibleCols; ++screenCol)
		{
			const int worldCol = m_viewStartCol + screenCol;
			const char tile = m_rows[row][worldCol];

			const sf::Texture* texture = nullptr;

			if (tile == 'X')
				texture = &m_floorTexture;
			else if (tile == 'B')
				texture = &m_breakTexture;
			else
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
				static_cast<float>(screenCol * TileSize),
				static_cast<float>(row * TileSize)
				});

			window.draw(sprite);
		}
	}

	// -------------------------------------------------------------------------
	// 2) Draw a red cell grid so buildable cells are visually obvious
	// -------------------------------------------------------------------------
	for (int row = 0; row < Rows; ++row)
	{
		for (int screenCol = 0; screenCol < VisibleCols; ++screenCol)
		{
			sf::RectangleShape cellOutline;
			cellOutline.setPosition({
				static_cast<float>(screenCol * TileSize),
				static_cast<float>(row * TileSize)
				});
			cellOutline.setSize({
				static_cast<float>(TileSize),
				static_cast<float>(TileSize)
				});
			cellOutline.setFillColor(sf::Color::Transparent);
			cellOutline.setOutlineColor(sf::Color(255, 0, 0, 80));
			cellOutline.setOutlineThickness(-1.f);

			window.draw(cellOutline);
		}
	}

	// -------------------------------------------------------------------------
	// 3) Draw a block preview that snaps to the hovered grid cell
	// -------------------------------------------------------------------------
	if (m_activeBrush != Brush::None)
	{
		int worldCol = 0;
		int row = 0;

		if (tryGetHoveredCell(mousePixelPosition, worldCol, row))
		{
			const sf::Texture* previewTexture = getTextureForBrush(m_activeBrush);

			if (previewTexture != nullptr)
			{
				const int screenCol = worldCol - m_viewStartCol;

				sf::Sprite previewSprite(*previewTexture);

				const sf::FloatRect localBounds = previewSprite.getLocalBounds();
				if (localBounds.size.x > 0.f && localBounds.size.y > 0.f)
				{
					previewSprite.setScale({
						static_cast<float>(TileSize) / localBounds.size.x,
						static_cast<float>(TileSize) / localBounds.size.y
						});

					previewSprite.setPosition({
						static_cast<float>(screenCol * TileSize),
						static_cast<float>(row * TileSize)
						});

					// If the tile is already occupied, tint the preview redder.
					if (m_rows[row][worldCol] == 'O')
						previewSprite.setColor(sf::Color(255, 255, 255, 170));
					else
						previewSprite.setColor(sf::Color(255, 120, 120, 150));

					window.draw(previewSprite);
				}
			}
		}
	}

	// -------------------------------------------------------------------------
	// 4) Draw editor border
	// -------------------------------------------------------------------------
	sf::RectangleShape border;
	border.setPosition({ 2.f, 2.f });
	border.setSize({
		static_cast<float>(VisibleCols * TileSize) - 4.f,
		static_cast<float>(Rows * TileSize) - 4.f
		});
	border.setFillColor(sf::Color::Transparent);
	border.setOutlineColor(sf::Color::Red);
	border.setOutlineThickness(4.f);
	window.draw(border);

	// -------------------------------------------------------------------------
	// 5) Draw the left/right scroll handles
	// -------------------------------------------------------------------------
	sf::RectangleShape leftHandle;
	leftHandle.setPosition({ 0.f, 0.f });
	leftHandle.setSize({
		static_cast<float>(HandleWidth),
		static_cast<float>(Rows * TileSize)
		});
	leftHandle.setFillColor(sf::Color(20, 20, 20, 140));
	leftHandle.setOutlineColor(sf::Color::White);
	leftHandle.setOutlineThickness(2.f);
	window.draw(leftHandle);

	sf::RectangleShape rightHandle;
	rightHandle.setPosition({
		static_cast<float>(VisibleCols * TileSize - HandleWidth),
		0.f
		});
	rightHandle.setSize({
		static_cast<float>(HandleWidth),
		static_cast<float>(Rows * TileSize)
		});
	rightHandle.setFillColor(sf::Color(20, 20, 20, 140));
	rightHandle.setOutlineColor(sf::Color::White);
	rightHandle.setOutlineThickness(2.f);
	window.draw(rightHandle);

	sf::ConvexShape leftArrow(3);
	leftArrow.setPoint(0, { 7.f, static_cast<float>(Rows * TileSize) * 0.5f });
	leftArrow.setPoint(1, { 18.f, static_cast<float>(Rows * TileSize) * 0.5f - 18.f });
	leftArrow.setPoint(2, { 18.f, static_cast<float>(Rows * TileSize) * 0.5f + 18.f });
	leftArrow.setFillColor(sf::Color::White);
	window.draw(leftArrow);

	const float rightBaseX = static_cast<float>(VisibleCols * TileSize - HandleWidth);
	sf::ConvexShape rightArrow(3);
	rightArrow.setPoint(0, { rightBaseX + 17.f, static_cast<float>(Rows * TileSize) * 0.5f });
	rightArrow.setPoint(1, { rightBaseX + 6.f, static_cast<float>(Rows * TileSize) * 0.5f - 18.f });
	rightArrow.setPoint(2, { rightBaseX + 6.f, static_cast<float>(Rows * TileSize) * 0.5f + 18.f });
	rightArrow.setFillColor(sf::Color::White);
	window.draw(rightArrow);

	// -------------------------------------------------------------------------
	// 6) Draw a small slice indicator so the user knows which portion
	//    of the larger map they are editing
	// -------------------------------------------------------------------------
	{
		const int visibleStart = m_viewStartCol + 1;
		const int visibleEnd = m_viewStartCol + VisibleCols;

		sf::Text sliceText(m_font);
		sliceText.setCharacterSize(20);
		sliceText.setString(
			"Cols " + std::to_string(visibleStart) +
			"-" + std::to_string(visibleEnd) +
			" / " + std::to_string(TotalCols)
		);
		sliceText.setFillColor(sf::Color::White);
		sliceText.setOutlineColor(sf::Color::Black);
		sliceText.setOutlineThickness(2.f);

		const sf::FloatRect bounds = sliceText.getLocalBounds();
		sliceText.setPosition({
			(static_cast<float>(VisibleCols * TileSize) - bounds.size.x) * 0.5f - bounds.position.x,
			12.f - bounds.position.y
			});

		window.draw(sliceText);
	}

	// -------------------------------------------------------------------------
	// 7) Draw the hotbar-style inventory toolbar
	// -------------------------------------------------------------------------
	const float slotSize = 56.f;
	const float slotGap = 8.f;
	const float toolbarWidth = ToolbarSlotCount * slotSize + (ToolbarSlotCount - 1) * slotGap;
	const float toolbarX = (static_cast<float>(VisibleCols * TileSize) - toolbarWidth) * 0.5f;
	const float toolbarY = static_cast<float>(Rows * TileSize) - slotSize - 14.f;

	sf::RectangleShape toolbarBacking;
	toolbarBacking.setPosition({ toolbarX - 10.f, toolbarY - 10.f });
	toolbarBacking.setSize({ toolbarWidth + 20.f, slotSize + 20.f });
	toolbarBacking.setFillColor(sf::Color(20, 20, 20, 170));
	toolbarBacking.setOutlineColor(sf::Color::White);
	toolbarBacking.setOutlineThickness(2.f);
	window.draw(toolbarBacking);

	for (int i = 0; i < ToolbarSlotCount; ++i)
	{
		const float slotX = toolbarX + i * (slotSize + slotGap);

		sf::RectangleShape slotRect;
		slotRect.setPosition({ slotX, toolbarY });
		slotRect.setSize({ slotSize, slotSize });
		slotRect.setFillColor(sf::Color(40, 40, 40, 220));

		if (i == m_selectedToolbarSlot)
		{
			slotRect.setOutlineColor(sf::Color::Yellow);
			slotRect.setOutlineThickness(4.f);
		}
		else
		{
			slotRect.setOutlineColor(sf::Color(180, 180, 180));
			slotRect.setOutlineThickness(2.f);
		}

		window.draw(slotRect);

		const Brush slotBrush = m_toolbarBrushes[i];
		const sf::Texture* slotTexture = getTextureForBrush(slotBrush);

		if (slotTexture != nullptr)
		{
			sf::Sprite iconSprite(*slotTexture);

			const sf::FloatRect localBounds = iconSprite.getLocalBounds();
			if (localBounds.size.x > 0.f && localBounds.size.y > 0.f)
			{
				const float targetIconSize = 38.f;

				iconSprite.setScale({
					targetIconSize / localBounds.size.x,
					targetIconSize / localBounds.size.y
					});

				iconSprite.setPosition({
					slotX + (slotSize - targetIconSize) * 0.5f,
					toolbarY + (slotSize - targetIconSize) * 0.5f
					});

				window.draw(iconSprite);
			}
		}

		sf::Text slotNumberText(m_font);
		slotNumberText.setCharacterSize(16);
		slotNumberText.setString(std::to_string(i + 1));
		slotNumberText.setFillColor(sf::Color::White);
		slotNumberText.setOutlineColor(sf::Color::Black);
		slotNumberText.setOutlineThickness(1.f);
		slotNumberText.setPosition({
			slotX + 4.f,
			toolbarY + 1.f
			});

		window.draw(slotNumberText);
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