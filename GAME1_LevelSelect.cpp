#include "GAME1_LevelSelect.h"

#include <filesystem>

bool GAME1_LevelSelect::load(const std::string& fontPath)
{
	m_lastError.clear();

	if (!m_font.openFromFile(fontPath))
	{
		m_lastError = "Failed to load SurfersQuest level select font: " + fontPath;
		return false;
	}

	m_titleText.emplace(m_font);
	m_titleText->setString("SELECT SURFERSQUEST LEVEL");
	m_titleText->setCharacterSize(42);
	m_titleText->setFillColor(sf::Color::White);
	m_titleText->setOutlineColor(sf::Color::Black);
	m_titleText->setOutlineThickness(3.f);

	m_pageText.emplace(m_font);
	m_pageText->setCharacterSize(22);
	m_pageText->setFillColor(sf::Color(230, 230, 230));
	m_pageText->setOutlineColor(sf::Color::Black);
	m_pageText->setOutlineThickness(2.f);

	m_backText.emplace(m_font);
	m_backText->setString("BACK");
	m_backText->setCharacterSize(24);
	m_backText->setFillColor(sf::Color::White);
	m_backText->setOutlineColor(sf::Color::Black);
	m_backText->setOutlineThickness(2.f);

	m_previousText.emplace(m_font);
	m_previousText->setString("<");
	m_previousText->setCharacterSize(38);
	m_previousText->setFillColor(sf::Color::White);
	m_previousText->setOutlineColor(sf::Color::Black);
	m_previousText->setOutlineThickness(2.f);

	m_nextText.emplace(m_font);
	m_nextText->setString(">");
	m_nextText->setCharacterSize(38);
	m_nextText->setFillColor(sf::Color::White);
	m_nextText->setOutlineColor(sf::Color::Black);
	m_nextText->setOutlineThickness(2.f);

	for (int i = 0; i < SlotCount; ++i)
	{
		m_slotBoxes[i].setSize({ 520.f, 58.f });
		m_slotBoxes[i].setFillColor(sf::Color(36, 36, 48));
		m_slotBoxes[i].setOutlineColor(sf::Color::White);
		m_slotBoxes[i].setOutlineThickness(2.f);

		m_slotTexts[i].emplace(m_font);
		m_slotTexts[i]->setCharacterSize(28);
		m_slotTexts[i]->setOutlineColor(sf::Color::Black);
		m_slotTexts[i]->setOutlineThickness(2.f);
	}

	m_backButton.setSize({ 140.f, 46.f });
	m_backButton.setFillColor(sf::Color(90, 45, 45));
	m_backButton.setOutlineColor(sf::Color::White);
	m_backButton.setOutlineThickness(2.f);

	m_previousButton.setSize({ 64.f, 58.f });
	m_previousButton.setFillColor(sf::Color(45, 45, 70));
	m_previousButton.setOutlineColor(sf::Color::White);
	m_previousButton.setOutlineThickness(2.f);

	m_nextButton.setSize({ 64.f, 58.f });
	m_nextButton.setFillColor(sf::Color(45, 45, 70));
	m_nextButton.setOutlineColor(sf::Color::White);
	m_nextButton.setOutlineThickness(2.f);

	setLevels({});
	return true;
}

void GAME1_LevelSelect::setLevels(const std::vector<std::string>& levelPaths)
{
	m_levelPaths = levelPaths;
	m_currentPage = 0;
	m_selectedSlot = 0;
	m_selectedLevelPath.clear();
	rebuildVisibleSlots();
}

void GAME1_LevelSelect::rebuildVisibleSlots()
{
	for (int i = 0; i < SlotCount; ++i)
	{
		m_visibleLevelPaths[i].clear();
		m_slotActive[i] = false;

		const int levelIndex = getLevelIndexForSlot(i);
		const bool hasLevel = levelIndex >= 0 && levelIndex < static_cast<int>(m_levelPaths.size());

		if (hasLevel)
		{
			m_visibleLevelPaths[i] = m_levelPaths[levelIndex];
			m_slotActive[i] = true;

			if (m_slotTexts[i])
			{
				const std::filesystem::path path(m_levelPaths[levelIndex]);
				m_slotTexts[i]->setString(path.stem().string());
			}
		}
		else
		{
			if (m_slotTexts[i])
			{
				m_slotTexts[i]->setString("N/A");
			}
		}
	}

	const int totalPages = m_levelPaths.empty()
		? 1
		: static_cast<int>((m_levelPaths.size() - 1) / SlotCount) + 1;

	if (m_currentPage < 0)
		m_currentPage = 0;

	if (m_currentPage >= totalPages)
		m_currentPage = totalPages - 1;

	if (m_pageText)
	{
		m_pageText->setString(
			"Page " + std::to_string(m_currentPage + 1) +
			" / " + std::to_string(totalPages)
		);
	}

	refreshSelectionVisuals();
}

void GAME1_LevelSelect::refreshSelectionVisuals()
{
	if (m_selectedSlot < 0 ||
		m_selectedSlot >= SlotCount ||
		!m_slotActive[m_selectedSlot])
	{
		m_selectedSlot = 0;

		for (int i = 0; i < SlotCount; ++i)
		{
			if (m_slotActive[i])
			{
				m_selectedSlot = i;
				break;
			}
		}
	}

	for (int i = 0; i < SlotCount; ++i)
	{
		const bool active = m_slotActive[i];
		const bool selected = active && i == m_selectedSlot;

		m_slotBoxes[i].setFillColor(selected ? sf::Color(55, 55, 78) : active ? sf::Color(36, 36, 48) : sf::Color(26, 26, 34));
		m_slotBoxes[i].setOutlineColor(selected ? sf::Color(255, 220, 120) : sf::Color::White);

		if (m_slotTexts[i])
		{
			m_slotTexts[i]->setFillColor(selected ? sf::Color(255, 220, 120) : active ? sf::Color::White : sf::Color(150, 150, 150));
		}
	}
}

void GAME1_LevelSelect::layout(const sf::RenderWindow& window)
{
	const float windowWidth = static_cast<float>(window.getSize().x);

	if (m_titleText)
	{
		const sf::FloatRect bounds = m_titleText->getLocalBounds();
		m_titleText->setPosition({
			(windowWidth - bounds.size.x) * 0.5f - bounds.position.x,
			72.f - bounds.position.y
			});
	}

	if (m_pageText)
	{
		const sf::FloatRect bounds = m_pageText->getLocalBounds();
		m_pageText->setPosition({
			(windowWidth - bounds.size.x) * 0.5f - bounds.position.x,
			124.f - bounds.position.y
			});
	}

	const float slotX = (windowWidth - m_slotBoxes[0].getSize().x) * 0.5f;
	const float firstSlotY = 170.f;
	const float slotSpacing = 68.f;

	for (int i = 0; i < SlotCount; ++i)
	{
		m_slotBoxes[i].setPosition({
			slotX,
			firstSlotY + static_cast<float>(i) * slotSpacing
			});

		if (m_slotTexts[i])
		{
			centerTextInRect(*m_slotTexts[i], m_slotBoxes[i].getGlobalBounds());
		}
	}

	m_backButton.setPosition({ 26.f, 24.f });
	if (m_backText)
	{
		centerTextInRect(*m_backText, m_backButton.getGlobalBounds());
	}

	m_previousButton.setPosition({
		slotX - 88.f,
		firstSlotY + slotSpacing * 2.f
		});

	m_nextButton.setPosition({
		slotX + m_slotBoxes[0].getSize().x + 24.f,
		firstSlotY + slotSpacing * 2.f
		});

	if (m_previousText)
		centerTextInRect(*m_previousText, m_previousButton.getGlobalBounds());

	if (m_nextText)
		centerTextInRect(*m_nextText, m_nextButton.getGlobalBounds());
}

GAME1_LevelSelectAction GAME1_LevelSelect::handleClick(sf::Vector2f mousePosition)
{
	if (containsPoint(m_backButton.getGlobalBounds(), mousePosition))
		return GAME1_LevelSelectAction::Back;

	if (containsPoint(m_previousButton.getGlobalBounds(), mousePosition))
	{
		selectPreviousPage();
		return GAME1_LevelSelectAction::PreviousPage;
	}

	if (containsPoint(m_nextButton.getGlobalBounds(), mousePosition))
	{
		selectNextPage();
		return GAME1_LevelSelectAction::NextPage;
	}

	for (int i = 0; i < SlotCount; ++i)
	{
		if (!m_slotActive[i])
			continue;

		if (containsPoint(m_slotBoxes[i].getGlobalBounds(), mousePosition))
		{
			m_selectedSlot = i;
			refreshSelectionVisuals();
			m_selectedLevelPath = m_visibleLevelPaths[i];
			return GAME1_LevelSelectAction::SelectedLevel;
		}
	}

	return GAME1_LevelSelectAction::None;
}

void GAME1_LevelSelect::selectPreviousSlot()
{
	for (int step = 0; step < SlotCount; ++step)
	{
		m_selectedSlot = (m_selectedSlot + SlotCount - 1) % SlotCount;

		if (m_slotActive[m_selectedSlot])
			break;
	}

	refreshSelectionVisuals();
}

void GAME1_LevelSelect::selectNextSlot()
{
	for (int step = 0; step < SlotCount; ++step)
	{
		m_selectedSlot = (m_selectedSlot + 1) % SlotCount;

		if (m_slotActive[m_selectedSlot])
			break;
	}

	refreshSelectionVisuals();
}

void GAME1_LevelSelect::selectPreviousPage()
{
	const int totalPages = m_levelPaths.empty()
		? 1
		: static_cast<int>((m_levelPaths.size() - 1) / SlotCount) + 1;

	if (totalPages <= 1)
		return;

	m_currentPage = (m_currentPage + totalPages - 1) % totalPages;
	m_selectedSlot = 0;
	rebuildVisibleSlots();
}

void GAME1_LevelSelect::selectNextPage()
{
	const int totalPages = m_levelPaths.empty()
		? 1
		: static_cast<int>((m_levelPaths.size() - 1) / SlotCount) + 1;

	if (totalPages <= 1)
		return;

	m_currentPage = (m_currentPage + 1) % totalPages;
	m_selectedSlot = 0;
	rebuildVisibleSlots();
}

GAME1_LevelSelectAction GAME1_LevelSelect::activateSelectedSlot()
{
	if (m_selectedSlot < 0 || m_selectedSlot >= SlotCount)
		return GAME1_LevelSelectAction::None;

	if (!m_slotActive[m_selectedSlot])
		return GAME1_LevelSelectAction::None;

	m_selectedLevelPath = m_visibleLevelPaths[m_selectedSlot];
	return GAME1_LevelSelectAction::SelectedLevel;
}

void GAME1_LevelSelect::draw(sf::RenderWindow& window) const
{
	if (m_titleText)
		window.draw(*m_titleText);

	if (m_pageText)
		window.draw(*m_pageText);

	window.draw(m_backButton);
	if (m_backText)
		window.draw(*m_backText);

	for (int i = 0; i < SlotCount; ++i)
	{
		window.draw(m_slotBoxes[i]);

		if (m_slotTexts[i])
			window.draw(*m_slotTexts[i]);
	}

	window.draw(m_previousButton);
	window.draw(m_nextButton);

	if (m_previousText)
		window.draw(*m_previousText);

	if (m_nextText)
		window.draw(*m_nextText);
}

bool GAME1_LevelSelect::hasLevelAt(int slotIndex) const
{
	if (slotIndex < 0 || slotIndex >= SlotCount)
		return false;

	return m_slotActive[slotIndex];
}

const std::string& GAME1_LevelSelect::getLevelPathAt(int slotIndex) const
{
	return m_visibleLevelPaths[slotIndex];
}

const std::string& GAME1_LevelSelect::getSelectedLevelPath() const
{
	return m_selectedLevelPath;
}

const std::string& GAME1_LevelSelect::getLastError() const
{
	return m_lastError;
}

int GAME1_LevelSelect::getLevelIndexForSlot(int slotIndex) const
{
	return m_currentPage * SlotCount + slotIndex;
}

void GAME1_LevelSelect::centerTextInRect(sf::Text& text, const sf::FloatRect& rect)
{
	const sf::FloatRect bounds = text.getLocalBounds();

	text.setPosition({
		rect.position.x + (rect.size.x - bounds.size.x) * 0.5f - bounds.position.x,
		rect.position.y + (rect.size.y - bounds.size.y) * 0.5f - bounds.position.y - 2.f
		});
}

bool GAME1_LevelSelect::containsPoint(const sf::FloatRect& bounds, sf::Vector2f point)
{
	return point.x >= bounds.position.x &&
		point.x <= bounds.position.x + bounds.size.x &&
		point.y >= bounds.position.y &&
		point.y <= bounds.position.y + bounds.size.y;
}
