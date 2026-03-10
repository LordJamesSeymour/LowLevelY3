#pragma once

#include <SFML/Graphics.hpp>
#include <array>
#include <optional>
#include <string>
#include <vector>

// Simple level selection screen for Game 1.
// It displays up to five available map files.
class GAME1_LevelSelect
{
public:
	static constexpr int SlotCount = 5;

public:
	bool load(const std::string& fontPath);
	void setLevels(const std::vector<std::string>& levelPaths);

	void layout(const sf::RenderWindow& window);
	int handleClick(sf::Vector2f mousePosition) const;
	void draw(sf::RenderWindow& window) const;

	bool hasLevelAt(int slotIndex) const;
	const std::string& getLevelPathAt(int slotIndex) const;

	const std::string& getLastError() const;

private:
	static bool containsPoint(const sf::FloatRect& bounds, sf::Vector2f point);

private:
	sf::Font m_font;

	std::optional<sf::Text> m_titleText;
	std::array<std::optional<sf::Text>, SlotCount> m_slotTexts;

	std::array<std::string, SlotCount> m_levelPaths;
	std::array<bool, SlotCount> m_slotActive{ false, false, false, false, false };

	std::string m_lastError;
};