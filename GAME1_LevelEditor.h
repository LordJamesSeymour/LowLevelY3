#pragma once

#include <SFML/Graphics.hpp>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

// SurfersQuest level editor.
// This now follows the same editor style as the Bomberman editor:
// - world selector and #WORLD=N map metadata
// - save and load buttons
// - load-level selector arrows
// - paged hotbar with arrow buttons
// - scroll wheel hotbar navigation
// - direct tile-letter hotkeys
// - middle-mouse tile picker
// - right-click erase
// - keeps the old horizontal screen scrolling for wide platformer maps
class GAME1_LevelEditor
{
public:
	static constexpr int Rows = 10;
	static constexpr int VisibleCols = 16;
	static constexpr int TotalCols = 48;
	static constexpr int GameplayTileSize = 64;
	static constexpr int ToolbarSlotCount = 9;

public:
	// Preferred new loader.
	// Example root: assets/Game#1/SurfersQuest
	bool load(const std::string& fontPath,
		const std::string& surfersQuestRootDirectory);

	// Kept for old main.cpp compatibility.
	bool load(const std::string& floorTexturePath,
		const std::string& breakTexturePath,
		const std::string& fontPath);

	void resetEmpty();

	void layout(const sf::RenderWindow& window);
	void update(float deltaTime, sf::Vector2u windowSize);

	void handleMousePressed(sf::Mouse::Button button, sf::Vector2i mousePixelPosition);
	void handleMouseWheelScrolled(float delta);
	void handleKeyReleased(sf::Keyboard::Key key);

	// Kept public so the existing main.cpp can still call these directly.
	void selectToolbarSlot(int oneBasedVisibleSlot);
	void paintAtPixel(sf::Vector2i mousePixelPosition);
	void eraseAtPixel(sf::Vector2i mousePixelPosition);
	void pickAtPixel(sf::Vector2i mousePixelPosition);

	bool saveToNextLevelFile();

	void draw(sf::RenderWindow& window, sf::Vector2i mousePixelPosition);

	const std::string& getLastError() const;
	const std::string& getLastSavedPath() const;

private:
	struct Tool
	{
		char tile = 'O';
		std::string label;
		std::string description;

		sf::Texture texture;
		bool hasTexture = false;

		sf::Color fallbackColor = sf::Color::White;
		bool isEraser = false;
		bool isBreakable = false;
	};

private:
	bool initialise(const std::string& fontPath,
		const std::filesystem::path& rootDirectory,
		const std::string& optionalBreakTexturePath);

	void buildTools();
	void rebuildVisibleToolbar();
	void addTool(Tool tool);

	bool loadTexture(sf::Texture& texture, const std::string& texturePath);
	bool loadFirstTextureFromDirectory(sf::Texture& texture, const std::string& directoryPath);
	bool loadBreakTexture();

	bool loadRowsFromFile(const std::string& mapPath);
	bool validateTileCharacter(char tile) const;

	void refreshSavedLevelList();
	bool loadSelectedLevelIntoEditor();
	void selectPreviousLoadLevel();
	void selectNextLoadLevel();
	std::string getSelectedLoadLevelName() const;

	void selectPreviousWorld();
	void selectNextWorld();
	int getHighestAvailableWorldNumber() const;

	void selectPreviousHotbarPage();
	void selectNextHotbarPage();
	void selectNextTool();
	void selectPreviousTool();
	void ensureSelectedToolVisible();

	void selectToolByTile(char tile);
	void selectToolByHotkey(sf::Keyboard::Key key);
	int findToolIndexForTile(char tile) const;

	void placeTileAt(int col, int row, char tile);

	void scrollLeft();
	void scrollRight();
	void clampViewStartColumn();

	bool isInsideLeftHandle(sf::Vector2i mousePixelPosition) const;
	bool isInsideRightHandle(sf::Vector2i mousePixelPosition) const;
	std::optional<sf::Vector2i> getTileAtPixel(sf::Vector2i mousePixelPosition) const;
	std::optional<int> getToolbarIndexAtPixel(sf::Vector2i mousePixelPosition) const;

	bool containsPoint(const sf::FloatRect& bounds, sf::Vector2f point) const;

	void drawText(sf::RenderTarget& target,
		const std::string& string,
		unsigned int size,
		sf::Vector2f position,
		sf::Color fill,
		float outlineThickness = 1.f) const;

	void drawTextCentered(sf::RenderTarget& target,
		const std::string& string,
		unsigned int size,
		const sf::FloatRect& rect,
		sf::Color fill,
		float outlineThickness = 1.f) const;

	void drawToolPreview(sf::RenderTarget& target,
		const Tool& tool,
		const sf::FloatRect& bounds,
		int visibleSlotNumber) const;

	void drawTilePreview(sf::RenderTarget& target,
		char tile,
		const sf::FloatRect& bounds) const;

	void drawTextureFitted(sf::RenderTarget& target,
		const sf::Texture& texture,
		const sf::FloatRect& bounds) const;

	const Tool* getSelectedTool() const;
	const Tool* getToolForTile(char tile) const;

	std::filesystem::path getResourcesDirectory() const;
	std::filesystem::path getMapsDirectory() const;
	std::filesystem::path getTilesDirectory() const;
	std::filesystem::path getCurrentWorldTilesDirectory() const;

	bool isValidLevelFile(const std::filesystem::path& path) const;
	int extractLevelNumber(const std::filesystem::path& path) const;

private:
	std::string m_rootDirectory;
	std::string m_resourcesDirectory;
	std::string m_mapsDirectory;
	std::string m_breakTexturePathOverride;
	std::string m_loadedBreakTexturePath;

	sf::Font m_font;
	sf::Texture m_breakTexture;
	bool m_hasBreakTexture = false;

	std::vector<std::string> m_rows;
	std::vector<Tool> m_tools;
	std::vector<int> m_visibleToolbarToolIndices;

	std::vector<std::string> m_savedLevelPaths;
	int m_selectedLoadLevelIndex = 0;

	int m_selectedToolIndex = 0;
	int m_worldNumber = 1;
	int m_hotbarPage = 0;
	int m_viewStartCol = 0;

	sf::Vector2u m_lastWindowSize{ 1024, 640 };

	sf::Vector2f m_gridOrigin{ 0.f, 92.f };
	float m_tileSize = 48.f;
	float m_scrollHandleWidth = 18.f;

	sf::Vector2f m_toolbarOrigin{ 0.f, 0.f };
	float m_toolbarSlotSize = 48.f;
	float m_toolbarSlotGap = 6.f;

	sf::FloatRect m_worldPreviousButtonBounds;
	sf::FloatRect m_worldSelectorBounds;
	sf::FloatRect m_worldNextButtonBounds;

	sf::FloatRect m_saveButtonBounds;
	sf::FloatRect m_loadButtonBounds;
	sf::FloatRect m_loadPreviousButtonBounds;
	sf::FloatRect m_loadLevelSelectorBounds;
	sf::FloatRect m_loadNextButtonBounds;

	sf::FloatRect m_previousHotbarPageButtonBounds;
	sf::FloatRect m_nextHotbarPageButtonBounds;

	std::string m_popupMessage;
	float m_popupTimer = 0.f;
	float m_popupDuration = 2.5f;
	bool m_popupIsError = false;

	std::string m_lastError;
	std::string m_lastSavedPath;
};
