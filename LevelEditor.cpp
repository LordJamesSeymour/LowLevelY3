#include "LevelEditor.h"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace
{
	bool IsValidLevelFile(const std::filesystem::path& path)
	{
		if (!path.has_filename() || path.extension() != ".txt")
			return false;

		const std::string stem = path.stem().string(); // example: level01

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

bool LevelEditor::load(const std::string& floorTexturePath, const std::string& breakTexturePath)
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

	resetEmpty();
	return true;
}

void LevelEditor::resetEmpty()
{
	m_rows.assign(Rows, std::string(Cols, 'O'));
	m_activeBrush = Brush::None;
	m_lastError.clear();
	m_lastSavedPath.clear();
}

void LevelEditor::toggleFloorBrush()
{
	if (m_activeBrush == Brush::Floor)
		m_activeBrush = Brush::None;
	else
		m_activeBrush = Brush::Floor;
}

void LevelEditor::toggleBreakBrush()
{
	if (m_activeBrush == Brush::Breakable)
		m_activeBrush = Brush::None;
	else
		m_activeBrush = Brush::Breakable;
}

void LevelEditor::paintAtPixel(sf::Vector2i mousePixelPosition)
{
	if (m_activeBrush == Brush::None)
		return;

	if (mousePixelPosition.x < 0 || mousePixelPosition.y < 0)
		return;

	const int col = mousePixelPosition.x / TileSize;
	const int row = mousePixelPosition.y / TileSize;

	if (col < 0 || col >= Cols || row < 0 || row >= Rows)
		return;

	if (m_rows[row][col] != 'O')
		return;

	if (m_activeBrush == Brush::Floor)
		m_rows[row][col] = 'X';
	else if (m_activeBrush == Brush::Breakable)
		m_rows[row][col] = 'B';
}

bool LevelEditor::saveToNextLevelFile()
{
	namespace fs = std::filesystem;

	m_lastError.clear();
	m_lastSavedPath.clear();

	try
	{
		fs::create_directories("maps");

		int levelCount = 0;

		for (const auto& entry : fs::directory_iterator("maps"))
		{
			if (entry.is_regular_file() && IsValidLevelFile(entry.path()))
			{
				++levelCount;
			}
		}

		const int nextNumber = levelCount + 1;

		std::ostringstream fileNameStream;
		fileNameStream << "level" << std::setw(2) << std::setfill('0') << nextNumber << ".txt";

		const fs::path savePath = fs::path("maps") / fileNameStream.str();

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

void LevelEditor::draw(sf::RenderWindow& window) const
{
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

const std::string& LevelEditor::getLastError() const
{
	return m_lastError;
}

const std::string& LevelEditor::getLastSavedPath() const
{
	return m_lastSavedPath;
}