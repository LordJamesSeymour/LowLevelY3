#pragma once

#include <SFML/Graphics.hpp>

#include "GAME1_LevelObject.h"
#include "GAME1_Trap.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

// SurfersQuest level editor.
// This follows the same editor style as the Bomberman editor:
// - world selector and #WORLD=N map metadata
// - save and load buttons
// - load-level selector arrows
// - paged hotbar with arrow buttons
// - scroll wheel hotbar navigation
// - direct tile-letter hotkeys
// - middle-mouse tile picker
// - right-click erase
// - horizontal screen scrolling for wide platformer maps
// - vertical screen scrolling for tall maps (build upward from the bottom)
// - fruit pickup placement using lowercase map codes:
//   c Cherry, s Strawberry, a Apple, b Banana, o Orange, k Kiwi,
//   m Melon, p Pineapple
//
// Save is handled by the SAVE button or F5. P is not a save key.
class GAME1_LevelEditor
{
public:
	static constexpr int VisibleRows = 10;
	static constexpr int TotalRows = 40;
	static constexpr int VisibleCols = 16;
	static constexpr int TotalCols = 48;
	static constexpr int GameplayTileSize = 64;
	static constexpr int ToolbarSlotCount = 9;

public:
	// Preferred new loader.
	// Example root: assets/Game#1/SurfersQuest
	bool load(const std::string& fontPath,
		const std::string& surfersQuestRootDirectory);

	// Kept for old main.cpp compatibility. The middle argument is ignored.
	bool load(const std::string& floorTexturePath,
		const std::string& ignoredLegacyTexturePath,
		const std::string& fontPath);

	void resetEmpty();

	void layout(const sf::RenderWindow& window);
	void update(float deltaTime, sf::Vector2u windowSize);

	void handleMousePressed(sf::Mouse::Button button, sf::Vector2i mousePixelPosition);
	void handleMouseReleased(sf::Mouse::Button button, sf::Vector2i mousePixelPosition);
	void handleMouseWheelScrolled(float delta);
	void handleKeyReleased(sf::Keyboard::Key key);

	// Returns true if Escape was consumed by the editor (e.g. it cancelled an
	// active selection preview). When it returns false the caller should leave
	// the editor as before.
	bool handleEscape();

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
	enum class ToolKind
	{
		Tile,
		TrapObject,
		LevelObject
	};

	struct Tool
	{
		ToolKind kind = ToolKind::Tile;
		char tile = 'O';
		GAME1_TrapType trapType = GAME1_TrapType::FallingPlatform;
		GAME1_LevelObjectType levelObjectType = GAME1_LevelObjectType::StartTile;
		std::string label;
		std::string description;

		sf::Texture texture;
		bool hasTexture = false;

		sf::Color fallbackColor = sf::Color::White;
		bool isEraser = false;
		// Tile tools that paint into the decoration-only background layer
		// (m_bgRows) instead of the foreground collision layer (m_rows).
		bool isBackgroundTile = false;
	};

	struct EditorObject
	{
		ToolKind kind = ToolKind::TrapObject;
		GAME1_TrapType trapType = GAME1_TrapType::FallingPlatform;
		GAME1_LevelObjectType levelObjectType = GAME1_LevelObjectType::StartTile;
		sf::Vector2i gridPosition{ 0, 0 };
		GAME1_TrapOrientation orientation = GAME1_TrapOrientation::Up;
	};

	// Rectangular block of tiles + objects copied by the drag-selection tool.
	// Object grid positions are stored relative to the selection's top-left.
	struct ClipboardSelection
	{
		int width = 0;
		int height = 0;
		std::vector<std::string> tiles;
		std::vector<EditorObject> objects;
	};

private:
	bool initialise(const std::string& fontPath,
		const std::filesystem::path& rootDirectory,
		const std::string& ignoredLegacyTexturePath);

	void buildTools();
	void rebuildVisibleToolbar();
	void addTool(Tool tool);

	bool loadTexture(sf::Texture& texture, const std::string& texturePath);
	bool loadFirstTextureFromDirectory(sf::Texture& texture, const std::string& directoryPath);

	bool loadRowsFromFile(const std::string& mapPath);
	bool validateTileCharacter(char tile) const;
	bool validateBackgroundTileCharacter(char tile) const;
	bool parseObjectLine(const std::string& line, EditorObject& outObject) const;

	void refreshSavedLevelList();
	bool loadSelectedLevelIntoEditor();
	void selectPreviousLoadLevel();
	void selectNextLoadLevel();
	std::string getSelectedLoadLevelName() const;

	bool writeCurrentLevelToFile(const std::filesystem::path& savePath);
	bool overwriteLoadedLevelFile();
	bool deleteLoadedLevelFile();
	bool hasCurrentLoadedLevel() const;

	void selectPreviousWorld();
	void selectNextWorld();
	int getHighestAvailableWorldNumber() const;
	void loadWorldBackgroundTexture();

	void selectPreviousHotbarPage();
	void selectNextHotbarPage();
	void selectNextTool();
	void selectPreviousTool();
	void ensureSelectedToolVisible();

	void selectToolByTile(char tile);
	void selectToolByHotkey(sf::Keyboard::Key key);
	int findToolIndexForTile(char tile) const;

	void placeTileAt(int col, int row, char tile);
	void placeBackgroundTileAt(int col, int row, char tile);
	bool isBackgroundToolSelected() const;
	void syncBackgroundLayerSize();
	void placeTrapObjectAt(int col, int row, GAME1_TrapType type);
	void placeLevelObjectAt(int col, int row, GAME1_LevelObjectType type);
	void placeSelectedToolAtTile(int col, int row);
	void eraseObjectAt(int col, int row);

	// SPACE + click column editing.
	void insertColumnAt(int col);
	void deleteColumnAt(int col);

	// Drag selection / copy / paste tool.
	std::optional<sf::Vector2i> clampPixelToTile(sf::Vector2i mousePixelPosition) const;
	void copySelection(sf::Vector2i startTile, sf::Vector2i endTile);
	void pasteClipboardAt(int destCol, int destRow);
	void cancelSelectionPreview();
	bool tileToVisibleRect(int worldCol, int worldRow, sf::FloatRect& outRect) const;
	void rotateSelectedObjectTool(int quarterTurnsClockwise);
	bool hasChainAt(int col, int row) const;
	int findObjectIndexAt(int col, int row, std::optional<GAME1_TrapType> type = std::nullopt) const;
	const EditorObject* getObjectAt(int col, int row) const;
	const EditorObject* getTopObjectAt(int col, int row) const;

	void scrollLeft();
	void scrollRight();
	void clampViewStartColumn();

	void scrollUp();
	void scrollDown();
	void clampViewStartRow();

	bool isInsideLeftHandle(sf::Vector2i mousePixelPosition) const;
	bool isInsideRightHandle(sf::Vector2i mousePixelPosition) const;
	bool isInsideTopHandle(sf::Vector2i mousePixelPosition) const;
	bool isInsideBottomHandle(sf::Vector2i mousePixelPosition) const;
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
		const sf::FloatRect& bounds,
		std::uint8_t alpha = 255) const;

	void drawObjectPreview(sf::RenderTarget& target,
		GAME1_TrapType type,
		GAME1_TrapOrientation orientation,
		const sf::FloatRect& bounds,
		std::uint8_t alpha = 255) const;

	// Grows a single grid-cell rect into the trap's on-grid footprint (2x2 for
	// SpikeHead, otherwise unchanged) so multi-tile objects preview at true size.
	sf::FloatRect objectFootprintBounds(GAME1_TrapType type,
		const sf::FloatRect& tileRect) const;

	void drawLevelObjectPreview(sf::RenderTarget& target,
		GAME1_LevelObjectType type,
		const sf::FloatRect& bounds,
		std::uint8_t alpha = 255) const;

	void drawTextureFitted(sf::RenderTarget& target,
		const sf::Texture& texture,
		const sf::FloatRect& bounds,
		std::uint8_t alpha = 255) const;

	void drawTextureFittedRotated(sf::RenderTarget& target,
		const sf::Texture& texture,
		const sf::FloatRect& bounds,
		GAME1_TrapOrientation orientation,
		std::uint8_t alpha = 255) const;

	const Tool* getSelectedTool() const;
	const Tool* getToolForTile(char tile) const;
	const Tool* getToolForTrapObject(GAME1_TrapType type) const;
	const Tool* getToolForLevelObject(GAME1_LevelObjectType type) const;

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

	sf::Font m_font;

	std::vector<std::string> m_rows;
	// Decoration-only background layer, kept the same size as m_rows. Saved in a
	// separate #BACKGROUND section. 'O' = empty.
	std::vector<std::string> m_bgRows;
	std::vector<EditorObject> m_objects;
	std::vector<Tool> m_tools;
	std::vector<int> m_visibleToolbarToolIndices;
	GAME1_TrapAssets m_trapAssets;

	std::vector<std::string> m_savedLevelPaths;
	int m_selectedLoadLevelIndex = 0;

	int m_selectedToolIndex = 0;
	int m_worldNumber = 1;
	int m_hotbarPage = 0;
	int m_viewStartCol = 0;
	int m_viewStartRow = 0;
	GAME1_TrapOrientation m_objectPreviewOrientation = GAME1_TrapOrientation::Up;

	// Drag-selection / clipboard state. A left press over the grid arms a
	// potential drag (placement is deferred to release); if the mouse releases
	// on a different tile the rectangle is copied and the editor enters a paste
	// preview that follows the mouse until clicked or cancelled.
	bool m_isPotentialDrag = false;
	sf::Vector2i m_dragStartTile{ 0, 0 };
	bool m_hasClipboard = false;
	bool m_inPreviewMode = false;
	ClipboardSelection m_clipboard;

	sf::Texture m_backgroundTexture;
	bool m_hasBackgroundTexture = false;

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

	sf::FloatRect m_overwriteButtonBounds;
	sf::FloatRect m_deleteButtonBounds;

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
	std::string m_currentLoadedLevelPath;
};
