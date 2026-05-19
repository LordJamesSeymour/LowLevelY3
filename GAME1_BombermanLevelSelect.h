#pragma once

#include <SFML/Graphics.hpp>
#include <array>
#include <optional>
#include <string>
#include <vector>

enum class GAME1_BombermanLevelSelectAction
{
	None,
	Back,
	PreviousPage,
	NextPage,
	SelectedLevel
};

class GAME1_BombermanLevelSelect
{
public:
	static constexpr int SlotCount = 5;

public:
	bool load(const std::string& fontPath, const std::string& mapsDirectory);

	void refreshLevelList();

	void layout(const sf::RenderWindow& window);
	GAME1_BombermanLevelSelectAction handleClick(sf::Vector2f mousePosition);

	void selectPreviousSlot();
	void selectNextSlot();
	void selectPreviousPage();
	void selectNextPage();
	GAME1_BombermanLevelSelectAction activateSelectedSlot();

	void draw(sf::RenderWindow& window) const;

	const std::string& getSelectedLevelPath() const;
	const std::string& getLastError() const;

private:
	static bool containsPoint(const sf::FloatRect& bounds, sf::Vector2f point);
	static bool isValidLevelFile(const std::filesystem::path& path);
	static int extractLevelNumber(const std::filesystem::path& path);

	void rebuildVisibleSlots();
	void updateSelectionVisuals();
	void centerTextInRect(sf::Text& text, const sf::FloatRect& rect);

private:
	sf::Font m_font;

	std::string m_mapsDirectory;
	std::vector<std::string> m_levelPaths;

	int m_currentPage = 0;
	int m_selectedVisibleSlot = 0;
	std::string m_selectedLevelPath;

	std::optional<sf::Text> m_titleText;
	std::optional<sf::Text> m_pageText;
	std::optional<sf::Text> m_backText;
	std::optional<sf::Text> m_previousText;
	std::optional<sf::Text> m_nextText;

	std::array<sf::RectangleShape, SlotCount> m_slotBoxes;
	std::array<std::optional<sf::Text>, SlotCount> m_slotTexts;
	std::array<bool, SlotCount> m_slotActive{ false, false, false, false, false };

	sf::RectangleShape m_backButton;
	sf::RectangleShape m_previousButton;
	sf::RectangleShape m_nextButton;

	std::string m_lastError;
};