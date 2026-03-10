#include "GAME1_LevelEditor.h"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace
{
	// All Game 1 maps now live in the new standardized folder.
	const std::filesystem::path kGame1MapsDirectory = "Assets/Game#1/Maps";

	bool IsValidLevelFile(const std::filesystem::path& path)
	{
		if (!path.has_filename() || path.extension() != ".txt")
			return false;

		const std::string stem = path.stem().string(); // Example: level01

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

bool GAME1_LevelEditor::load(const std::string& floorTexturePath, const std::string& breakTexturePath)
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

	// After textures are ready, build a clean empty map.
	resetEmpty();
	return true;
}

void GAME1_LevelEditor::resetEmpty()
{
	m_rows.assign(Rows, std::string(Cols, 'O'));
	m_activeBrush = Brush::None;
	m_lastError.clear();
	m_lastSavedPath.clear();
}

void GAME1_LevelEditor::toggleFloorBrush()
{
	if (m_activeBrush == Brush::Floor)
		m_activeBrush = Brush::None;
	else
		m_activeBrush = Brush::Floor;
}

void GAME1_LevelEditor::toggleBreakBrush()
{
	if (m_activeBrush == Brush::Breakable)
		m_activeBrush = Brush::None;
	else
		m_activeBrush = Brush::Breakable;
}

void GAME1_LevelEditor::paintAtPixel(sf::Vector2i mousePixelPosition)
{
	if (m_activeBrush == Brush::None)
		return;

	if (mousePixelPosition.x < 0 || mousePixelPosition.y < 0)
		return;

	// Convert from mouse coordinates into grid coordinates.
	const int col = mousePixelPosition.x / TileSize;
	const int row = mousePixelPosition.y / TileSize;

	if (col < 0 || col >= Cols || row < 0 || row >= Rows)
		return;

	// This simple version only paints into empty tiles.
	if (m_rows[row][col] != 'O')
		return;

	if (m_activeBrush == Brush::Floor)
		m_rows[row][col] = 'X';
	else if (m_activeBrush == Brush::Breakable)
		m_rows[row][col] = 'B';
}

bool GAME1_LevelEditor::saveToNextLevelFile()
{
	namespace fs = std::filesystem;

	m_lastError.clear();
	m_lastSavedPath.clear();

	try
	{
		// Make sure the new Game 1 map directory exists.
		fs::create_directories(kGame1MapsDirectory);

		int levelCount = 0;

		// Count existing valid level files so the next one gets the next number.
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

		// Write the text map out line by line.
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

void GAME1_LevelEditor::draw(sf::RenderWindow& window) const
{
	// Draw placed tiles in the editor.
	for (int row = 0; row < Rows; ++row)
	{
		for (int col = 0; col < Cols; ++col)
		{
			const char tile = m_rows[row][col];
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
				static_cast<float>(col * TileSize),
				static_cast<float>(row * TileSize)
				});

			window.draw(sprite);
		}
	}

	// Red border so the editable region is visually obvious.
	sf::RectangleShape border;
	border.setPosition({ 2.f, 2.f });
	border.setSize({
		static_cast<float>(Cols * TileSize) - 4.f,
		static_cast<float>(Rows * TileSize) - 4.f
		});
	border.setFillColor(sf::Color::Transparent);
	border.setOutlineColor(sf::Color::Red);
	border.setOutlineThickness(4.f);

	window.draw(border);
}

const std::string& GAME1_LevelEditor::getLastError() const
{
	return m_lastError;
}

const std::string& GAME1_LevelEditor::getLastSavedPath() const
{
	return m_lastSavedPath;
}