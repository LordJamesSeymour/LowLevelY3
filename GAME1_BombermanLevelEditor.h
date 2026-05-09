#pragma once

#include <SFML/Graphics.hpp>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

class GAME1_BombermanLevelEditor
{
public:
	bool load(const std::string& fontPath, const std::string& bombermanRootDirectory);

	void reset();
	bool resetFromTemplate();

	void layout(const sf::RenderWindow& window);
	void update(float deltaTime, sf::Vector2u windowSize);

	void draw(sf::RenderWindow& window);
	void draw(sf::RenderWindow& window, sf::Vector2i mousePixelPosition);

	void handleMousePressed(sf::Mouse::Button button, sf::Vector2i mousePixelPosition);
	void handleMouseWheelScrolled(float delta);
	void handleKeyReleased(sf::Keyboard::Key key);

	void paintAtPixel(sf::Vector2i pixelPosition);
	void eraseAtPixel(sf::Vector2i pixelPosition);
	void pickAtPixel(sf::Vector2i pixelPosition);

	void selectToolbarSlot(int slotNumber);

	bool saveToNextLevelFile();

	const std::string& getLastError() const;
	const std::string& getLastSavedPath() const;

private:
	struct Tool
	{
		char tile = ' ';
		std::string label;
		std::string description;

		sf::Texture texture;
		bool hasTexture = false;

		sf::Color fallbackColor = sf::Color::White;
		bool isWorldTile = false;
		bool isFixedTool = false;
	};

private:
	void buildTools();
	void rebuildVisibleToolbar();

	void addFixedTool(char tile,
		const std::string& label,
		const std::string& description,
		const std::string& texturePath,
		sf::Color fallback);

	void addFixedDirectoryTool(char tile,
		const std::string& label,
		const std::string& description,
		const std::string& directoryPath,
		sf::Color fallback);

	void addWorldTool(char tile,
		const std::string& label,
		const std::string& description,
		const std::string& texturePath,
		sf::Color fallback);

	bool loadRowsFromFile(const std::string& mapPath);
	bool validateTileCharacter(char tile) const;

	bool loadTexture(sf::Texture& texture, const std::string& texturePath);
	bool loadFirstTextureFromDirectory(sf::Texture& texture, const std::string& directoryPath);

	std::optional<sf::Vector2i> getTileAtPixel(sf::Vector2i pixelPosition) const;
	std::optional<int> getToolbarIndexAtPixel(sf::Vector2i pixelPosition) const;

	bool containsPoint(const sf::FloatRect& bounds, sf::Vector2f point) const;

	void selectToolByTile(char tile);
	void selectToolByHotkey(sf::Keyboard::Key key);
	void selectNextTool();
	void selectPreviousTool();

	void selectNextWorld();
	void selectPreviousWorld();

	void selectNextHotbarPage();
	void selectPreviousHotbarPage();
	void ensureSelectedToolVisible();

	void refreshSavedLevelList();
	void selectNextLoadLevel();
	void selectPreviousLoadLevel();
	bool loadSelectedLevelIntoEditor();
	std::string getSelectedLoadLevelName() const;

	int findToolIndexForTile(char tile) const;
	int findVisibleToolbarPositionForToolIndex(int toolIndex) const;

	void placeTileAt(int col, int row, char tile);

	void drawText(sf::RenderTarget& target,
		const std::string& string,
		unsigned int size,
		sf::Vector2f position,
		sf::Color fill,
		float outlineThickness = 1.f) const;

	void drawToolPreview(sf::RenderTarget& target,
		const Tool& tool,
		const sf::FloatRect& bounds) const;

	void drawTilePreview(sf::RenderTarget& target,
		char tile,
		const sf::FloatRect& bounds) const;

	void drawTextureFitted(sf::RenderTarget& target,
		const sf::Texture& texture,
		const sf::FloatRect& bounds) const;

	std::filesystem::path getTemplatePath() const;
	std::filesystem::path getMapsDirectory() const;
	std::filesystem::path getTilesDirectory() const;
	std::filesystem::path getCurrentWorldTilesDirectory() const;

	int getHighestAvailableWorldNumber() const;

	bool isValidLevelFile(const std::filesystem::path& path) const;
	int extractLevelNumber(const std::filesystem::path& path) const;

private:
	std::string m_bombermanRootDirectory;
	std::string m_resourcesDirectory;
	std::string m_mapsDirectory;

	sf::Font m_font;

	std::vector<std::string> m_rows;
	std::vector<Tool> m_tools;
	std::vector<int> m_visibleToolbarToolIndices;

	std::vector<std::string> m_savedLevelPaths;
	int m_selectedLoadLevelIndex = 0;

	int m_selectedToolIndex = 0;
	int m_worldNumber = 1;
	int m_hotbarPage = 0;

	int m_fixedToolCount = 6;
	int m_worldToolsPerPage = 8;

	sf::Vector2f m_gridOrigin{ 70.f, 70.f };
	sf::Vector2f m_toolbarOrigin{ 70.f, 555.f };

	float m_tileSize = 48.f;
	float m_toolbarSlotSize = 40.f;
	float m_toolbarSlotGap = 6.f;

	sf::FloatRect m_saveButtonBounds;

	sf::FloatRect m_loadButtonBounds;
	sf::FloatRect m_loadPreviousButtonBounds;
	sf::FloatRect m_loadLevelSelectorBounds;
	sf::FloatRect m_loadNextButtonBounds;

	sf::FloatRect m_worldPreviousButtonBounds;
	sf::FloatRect m_worldSelectorBounds;
	sf::FloatRect m_worldNextButtonBounds;

	sf::FloatRect m_previousHotbarPageButtonBounds;
	sf::FloatRect m_nextHotbarPageButtonBounds;

	sf::Vector2u m_lastWindowSize{ 1024, 640 };

	std::string m_lastError;
	std::string m_lastSavedPath;
};