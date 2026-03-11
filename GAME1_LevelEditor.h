#pragma once

#include <SFML/Graphics.hpp>
#include <array>
#include <string>
#include <vector>

// The level editor now supports:
// - a wider side-scrolling map
// - a visible red placement grid
// - a mouse-follow preview block
// - a 9-slot toolbar, inspired by an inventory hotbar
//
// Slots are bound to keys 1-9.
// Right now:
//   1 = Floor block
//   2 = Breakable block
//   3-9 = empty placeholders for future block types
class GAME1_LevelEditor
{
public:
	static constexpr int Rows = 10;
	static constexpr int VisibleCols = 16;
	static constexpr int TotalCols = 48;
	static constexpr int TileSize = 64;
	static constexpr int HandleWidth = 24;
	static constexpr int ToolbarSlotCount = 9;

	enum class Brush
	{
		None,
		Floor,
		Breakable
	};

public:
	bool load(const std::string& floorTexturePath,
		const std::string& breakTexturePath,
		const std::string& fontPath);

	void resetEmpty();

	void selectToolbarSlot(int oneBasedSlot);
	void paintAtPixel(sf::Vector2i mousePixelPosition);

	bool saveToNextLevelFile();

	void draw(sf::RenderWindow& window, sf::Vector2i mousePixelPosition) const;

	const std::string& getLastError() const;
	const std::string& getLastSavedPath() const;

private:
	void scrollLeft();
	void scrollRight();

	bool isInsideVisibleGridPixel(sf::Vector2i mousePixelPosition) const;
	bool isInsideLeftHandle(sf::Vector2i mousePixelPosition) const;
	bool isInsideRightHandle(sf::Vector2i mousePixelPosition) const;
	bool tryGetHoveredCell(sf::Vector2i mousePixelPosition, int& outWorldCol, int& outRow) const;

	const sf::Texture* getTextureForBrush(Brush brush) const;

private:
	sf::Texture m_floorTexture;
	sf::Texture m_breakTexture;
	sf::Font m_font;

	std::vector<std::string> m_rows;

	std::array<Brush, ToolbarSlotCount> m_toolbarBrushes
	{
		Brush::Floor,
		Brush::Breakable,
		Brush::None,
		Brush::None,
		Brush::None,
		Brush::None,
		Brush::None,
		Brush::None,
		Brush::None
	};

	int m_selectedToolbarSlot = 0;
	Brush m_activeBrush = Brush::Floor;

	int m_viewStartCol = 0;

	std::string m_lastError;
	std::string m_lastSavedPath;
};