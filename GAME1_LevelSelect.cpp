#include "GAME1_LevelSelect.h"

#include <filesystem>

bool GAME1_LevelSelect::load(const std::string& fontPath)
{
	m_lastError.clear();

	if (!m_font.openFromFile(fontPath))
	{
		m_lastError = "Failed to load font: " + fontPath;
		return false;
	}

	// Create the title once.
	m_titleText.emplace(m_font);
	m_titleText->setString("Select Level");
	m_titleText->setCharacterSize(42);
	m_titleText->setFillColor(sf::Color::White);
	m_titleText->setOutlineColor(sf::Color::Black);
	m_titleText->setOutlineThickness(2.f);

	// Create the 5 selectable slot texts.
	for (int i = 0; i < SlotCount; ++i)
	{
		m_slotTexts[i].emplace(m_font);
		m_slotTexts[i]->setCharacterSize(34);
		m_slotTexts[i]->setOutlineThickness(2.f);
	}

	setLevels({});
	return true;
}

void GAME1_LevelSelect::setLevels(const std::vector<std::string>& levelPaths)
{
	// Reset all slots to an inactive state first.
	for (int i = 0; i < SlotCount; ++i)
	{
		m_levelPaths[i].clear();
		m_slotActive[i] = false;

		if (m_slotTexts[i])
		{
			m_slotTexts[i]->setString("N/A");
			m_slotTexts[i]->setFillColor(sf::Color(180, 180, 180));
			m_slotTexts[i]->setOutlineColor(sf::Color::Black);
		}
	}

	const int count = static_cast<int>(levelPaths.size()) < SlotCount
		? static_cast<int>(levelPaths.size())
		: SlotCount;

	// Copy real map paths into the visible slots.
	for (int i = 0; i < count; ++i)
	{
		m_levelPaths[i] = levelPaths[i];
		m_slotActive[i] = true;

		const std::string fileStem = std::filesystem::path(levelPaths[i]).stem().string();

		if (m_slotTexts[i])
		{
			m_slotTexts[i]->setString(fileStem);
			m_slotTexts[i]->setFillColor(sf::Color::White);
			m_slotTexts[i]->setOutlineColor(sf::Color::Black);
		}
	}
}

void GAME1_LevelSelect::layout(const sf::RenderWindow& window)
{
	if (!m_titleText)
		return;

	const float windowWidth = static_cast<float>(window.getSize().x);

	// Center the title near the top.
	const sf::FloatRect titleBounds = m_titleText->getLocalBounds();

	m_titleText->setPosition({
		(windowWidth - titleBounds.size.x) * 0.5f - titleBounds.position.x,
		100.f - titleBounds.position.y
		});

	// Stack level entries underneath the title.
	const float startY = 200.f;
	const float spacing = 65.f;

	for (int i = 0; i < SlotCount; ++i)
	{
		if (!m_slotTexts[i])
			continue;

		const sf::FloatRect bounds = m_slotTexts[i]->getLocalBounds();

		m_slotTexts[i]->setPosition({
			(windowWidth - bounds.size.x) * 0.5f - bounds.position.x,
			startY + i * spacing - bounds.position.y
			});
	}
}

int GAME1_LevelSelect::handleClick(sf::Vector2f mousePosition) const
{
	for (int i = 0; i < SlotCount; ++i)
	{
		if (!m_slotTexts[i] || !m_slotActive[i])
			continue;

		if (containsPoint(m_slotTexts[i]->getGlobalBounds(), mousePosition))
			return i;
	}

	return -1;
}

void GAME1_LevelSelect::draw(sf::RenderWindow& window) const
{
	if (m_titleText)
		window.draw(*m_titleText);

	for (int i = 0; i < SlotCount; ++i)
	{
		if (m_slotTexts[i])
			window.draw(*m_slotTexts[i]);
	}
}

bool GAME1_LevelSelect::hasLevelAt(int slotIndex) const
{
	if (slotIndex < 0 || slotIndex >= SlotCount)
		return false;

	return m_slotActive[slotIndex];
}

const std::string& GAME1_LevelSelect::getLevelPathAt(int slotIndex) const
{
	return m_levelPaths[slotIndex];
}

const std::string& GAME1_LevelSelect::getLastError() const
{
	return m_lastError;
}

bool GAME1_LevelSelect::containsPoint(const sf::FloatRect& bounds, sf::Vector2f point)
{
	return point.x >= bounds.position.x &&
		point.x <= bounds.position.x + bounds.size.x &&
		point.y >= bounds.position.y &&
		point.y <= bounds.position.y + bounds.size.y;
}