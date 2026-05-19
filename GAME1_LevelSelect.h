#pragma once

#include <SFML/Graphics.hpp>
#include <array>
#include <optional>
#include <string>
#include <vector>

enum class GAME1_LevelSelectAction
{
	None,
	Back,
	PreviousPage,
	NextPage,
	SelectedLevel
};

// SurfersQuest level selection screen.
// Styled to match the Bomberman level selector for a consistent arcade UI.
class GAME1_LevelSelect
{
public:
	static constexpr int SlotCount = 5;

public:
	bool load(const std::string& fontPath);
	void setLevels(const std::vector<std::string>& levelPaths);

	void layout(const sf::RenderWindow& window);
	GAME1_LevelSelectAction handleClick(sf::Vector2f mousePosition);

	void selectPreviousSlot();
	void selectNextSlot();
	void selectPreviousPage();
	void selectNextPage();
	GAME1_LevelSelectAction activateSelectedSlot();

	void draw(sf::RenderWindow& window) const;

	bool hasLevelAt(int slotIndex) const;
	const std::string& getLevelPathAt(int slotIndex) const;
	const std::string& getSelectedLevelPath() const;

	const std::string& getLastError() const;

private:
	static bool containsPoint(const sf::FloatRect& bounds, sf::Vector2f point);
	void rebuildVisibleSlots();
	void refreshSelectionVisuals();
	void centerTextInRect(sf::Text& text, const sf::FloatRect& rect);
	int getLevelIndexForSlot(int slotIndex) const;

private:
	sf::Font m_font;

	std::vector<std::string> m_levelPaths;
	std::array<std::string, SlotCount> m_visibleLevelPaths;
	std::array<bool, SlotCount> m_slotActive{ false, false, false, false, false };

	int m_currentPage = 0;
	int m_selectedSlot = 0;
	std::string m_selectedLevelPath;

	std::optional<sf::Text> m_titleText;
	std::optional<sf::Text> m_pageText;
	std::optional<sf::Text> m_backText;
	std::optional<sf::Text> m_previousText;
	std::optional<sf::Text> m_nextText;

	std::array<sf::RectangleShape, SlotCount> m_slotBoxes;
	std::array<std::optional<sf::Text>, SlotCount> m_slotTexts;

	sf::RectangleShape m_backButton;
	sf::RectangleShape m_previousButton;
	sf::RectangleShape m_nextButton;

	std::string m_lastError;
};
