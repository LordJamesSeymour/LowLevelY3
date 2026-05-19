#include "GAME1_BombermanLevelSelect.h"


#include <algorithm>
#include <cctype>
#include <filesystem>

bool GAME1_BombermanLevelSelect::load(const std::string& fontPath, const std::string& mapsDirectory)
{
	m_lastError.clear();
	m_mapsDirectory = mapsDirectory;

	if (!m_font.openFromFile(fontPath))
	{
		m_lastError = "Failed to load Bomberman level select font: " + fontPath;
		return false;
	}

	m_titleText.emplace(m_font);
	m_titleText->setString("SELECT BOMBERMAN LEVEL");
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

	refreshLevelList();
	return true;
}

void GAME1_BombermanLevelSelect::refreshLevelList()
{
	namespace fs = std::filesystem;

	m_levelPaths.clear();
	m_selectedLevelPath.clear();

	if (!fs::exists(m_mapsDirectory))
	{
		fs::create_directories(m_mapsDirectory);
	}

	std::vector<fs::path> paths;

	for (const auto& entry : fs::directory_iterator(m_mapsDirectory))
	{
		if (entry.is_regular_file() && isValidLevelFile(entry.path()))
		{
			paths.push_back(entry.path());
		}
	}

	std::sort(paths.begin(), paths.end(),
		[](const fs::path& a, const fs::path& b)
		{
			return extractLevelNumber(a) < extractLevelNumber(b);
		});

	for (const fs::path& path : paths)
	{
		m_levelPaths.push_back(path.string());
	}

	const int maxPage = m_levelPaths.empty()
		? 0
		: static_cast<int>((m_levelPaths.size() - 1) / SlotCount);

	if (m_currentPage > maxPage)
		m_currentPage = maxPage;

	if (m_currentPage < 0)
		m_currentPage = 0;

	rebuildVisibleSlots();
}

void GAME1_BombermanLevelSelect::rebuildVisibleSlots()
{
	for (int i = 0; i < SlotCount; ++i)
	{
		const int levelIndex = m_currentPage * SlotCount + i;
		const bool hasLevel = levelIndex >= 0 && levelIndex < static_cast<int>(m_levelPaths.size());

		m_slotActive[i] = hasLevel;

		if (!m_slotTexts[i])
			continue;

		if (hasLevel)
		{
			const std::filesystem::path path(m_levelPaths[levelIndex]);
			m_slotTexts[i]->setString(path.stem().string());
			m_slotTexts[i]->setFillColor(sf::Color::White);
			m_slotBoxes[i].setFillColor(sf::Color(36, 36, 48));
		}
		else
		{
			m_slotTexts[i]->setString("<empty>");
			m_slotTexts[i]->setFillColor(sf::Color(150, 150, 150));
			m_slotBoxes[i].setFillColor(sf::Color(26, 26, 34));
		}
	}

	if (m_pageText)
	{
		const int totalPages = m_levelPaths.empty()
			? 1
			: static_cast<int>((m_levelPaths.size() - 1) / SlotCount) + 1;

		m_pageText->setString(
			"Page " + std::to_string(m_currentPage + 1) +
			" / " + std::to_string(totalPages)
		);
	}
}

void GAME1_BombermanLevelSelect::layout(const sf::RenderWindow& window)
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

GAME1_BombermanLevelSelectAction GAME1_BombermanLevelSelect::handleClick(sf::Vector2f mousePosition)
{
	if (containsPoint(m_backButton.getGlobalBounds(), mousePosition))
		return GAME1_BombermanLevelSelectAction::Back;

	const int totalPages = m_levelPaths.empty()
		? 1
		: static_cast<int>((m_levelPaths.size() - 1) / SlotCount) + 1;

	if (m_currentPage > 0 && containsPoint(m_previousButton.getGlobalBounds(), mousePosition))
	{
		--m_currentPage;
		rebuildVisibleSlots();
		return GAME1_BombermanLevelSelectAction::PreviousPage;
	}

	if (m_currentPage + 1 < totalPages && containsPoint(m_nextButton.getGlobalBounds(), mousePosition))
	{
		++m_currentPage;
		rebuildVisibleSlots();
		return GAME1_BombermanLevelSelectAction::NextPage;
	}

	for (int i = 0; i < SlotCount; ++i)
	{
		if (!m_slotActive[i])
			continue;

		if (containsPoint(m_slotBoxes[i].getGlobalBounds(), mousePosition))
		{
			const int levelIndex = m_currentPage * SlotCount + i;

			if (levelIndex >= 0 && levelIndex < static_cast<int>(m_levelPaths.size()))
			{
				m_selectedLevelPath = m_levelPaths[levelIndex];
				return GAME1_BombermanLevelSelectAction::SelectedLevel;
			}
		}
	}

	return GAME1_BombermanLevelSelectAction::None;
}

void GAME1_BombermanLevelSelect::draw(sf::RenderWindow& window) const
{
	const sf::Vector2u windowSize = window.getSize();

	sf::RectangleShape background;
	background.setPosition({ 0.f, 0.f });
	background.setSize({
		static_cast<float>(windowSize.x),
		static_cast<float>(windowSize.y)
		});
	background.setFillColor(sf::Color(20, 20, 32));
	window.draw(background);

	if (m_titleText)
		window.draw(*m_titleText);

	if (m_pageText)
		window.draw(*m_pageText);

	for (int i = 0; i < SlotCount; ++i)
	{
		window.draw(m_slotBoxes[i]);

		if (m_slotTexts[i])
			window.draw(*m_slotTexts[i]);
	}

	window.draw(m_backButton);

	if (m_backText)
		window.draw(*m_backText);

	const int totalPages = m_levelPaths.empty()
		? 1
		: static_cast<int>((m_levelPaths.size() - 1) / SlotCount) + 1;

	if (m_currentPage > 0)
	{
		window.draw(m_previousButton);

		if (m_previousText)
			window.draw(*m_previousText);
	}

	if (m_currentPage + 1 < totalPages)
	{
		window.draw(m_nextButton);

		if (m_nextText)
			window.draw(*m_nextText);
	}
}

const std::string& GAME1_BombermanLevelSelect::getSelectedLevelPath() const
{
	return m_selectedLevelPath;
}

const std::string& GAME1_BombermanLevelSelect::getLastError() const
{
	return m_lastError;
}

bool GAME1_BombermanLevelSelect::containsPoint(const sf::FloatRect& bounds, sf::Vector2f point)
{
	return point.x >= bounds.position.x &&
		point.x <= bounds.position.x + bounds.size.x &&
		point.y >= bounds.position.y &&
		point.y <= bounds.position.y + bounds.size.y;
}

bool GAME1_BombermanLevelSelect::isValidLevelFile(const std::filesystem::path& path)
{
	if (!path.has_filename() || path.extension() != ".txt")
		return false;

	const std::string stem = path.stem().string();

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

int GAME1_BombermanLevelSelect::extractLevelNumber(const std::filesystem::path& path)
{
	const std::string stem = path.stem().string();
	return std::stoi(stem.substr(5));
}

void GAME1_BombermanLevelSelect::centerTextInRect(sf::Text& text, const sf::FloatRect& rect)
{
	const sf::FloatRect bounds = text.getLocalBounds();

	text.setPosition({
		rect.position.x + (rect.size.x - bounds.size.x) * 0.5f - bounds.position.x,
		rect.position.y + (rect.size.y - bounds.size.y) * 0.5f - bounds.position.y - 2.f
		});
}