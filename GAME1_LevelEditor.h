#pragma once

#include <SFML/Graphics.hpp>
#include <string>
#include <vector>

// The level editor lets the user paint a 10x16 tile map and save it
// into Assets/Game#1/Maps as level01.txt, level02.txt, and so on.
class GAME1_LevelEditor
{
public:
	static constexpr int Rows = 10;
	static constexpr int Cols = 16;
	static constexpr int TileSize = 64;

	enum class Brush
	{
		None,
		Floor,
		Breakable
	};

public:
	bool load(const std::string& floorTexturePath, const std::string& breakTexturePath);
	void resetEmpty();

	void toggleFloorBrush();
	void toggleBreakBrush();

	void paintAtPixel(sf::Vector2i mousePixelPosition);

	bool saveToNextLevelFile();

	void draw(sf::RenderWindow& window) const;

	const std::string& getLastError() const;
	const std::string& getLastSavedPath() const;

private:
	sf::Texture m_floorTexture;
	sf::Texture m_breakTexture;

	std::vector<std::string> m_rows;

	Brush m_activeBrush = Brush::None;

	std::string m_lastError;
	std::string m_lastSavedPath;
};