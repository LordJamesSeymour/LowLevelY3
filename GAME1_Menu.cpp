#include "GAME1_Menu.h"

#include "GAME1_SurfersQuestAudio.h"

#include <filesystem>

bool GAME1_Menu::load(const std::string& closeButtonTexturePath,
	const std::string& logoTexturePath,
	const std::string& playButtonTexturePath,
	const std::string& levelEditorTexturePath)
{
	(void)closeButtonTexturePath;
	(void)logoTexturePath;
	(void)playButtonTexturePath;
	(void)levelEditorTexturePath;

	m_lastError.clear();
	m_selectedButtonIndex = 0;

	GAME1_SurfersQuestAudio::initialise((std::filesystem::path("assets") / "Game#1" / "SurfersQuest" / "Resources").string());

	const std::string fontPath = "assets/menu.ttf";

	if (!m_font.openFromFile(fontPath))
	{
		m_lastError = "Failed to load SurfersQuest menu font: " + fontPath;
		return false;
	}

	m_titleText.emplace(m_font);
	m_titleText->setString("SURFERS QUEST");
	m_titleText->setCharacterSize(64);
	m_titleText->setFillColor(sf::Color::White);
	m_titleText->setOutlineColor(sf::Color::Black);
	m_titleText->setOutlineThickness(4.f);

	m_subtitleText.emplace(m_font);
	m_subtitleText->setString("Platformer Adventure");
	m_subtitleText->setCharacterSize(30);
	m_subtitleText->setFillColor(sf::Color(255, 230, 120));
	m_subtitleText->setOutlineColor(sf::Color::Black);
	m_subtitleText->setOutlineThickness(2.f);

	m_playButton.action = GAME1_MenuAction::Play;
	m_playButton.text.emplace(m_font);
	m_playButton.text->setString("PLAY LEVELS");

	m_levelEditorButton.action = GAME1_MenuAction::LevelEditor;
	m_levelEditorButton.text.emplace(m_font);
	m_levelEditorButton.text->setString("LEVEL EDITOR");

	m_backButton.action = GAME1_MenuAction::Quit;
	m_backButton.text.emplace(m_font);
	m_backButton.text->setString("BACK TO ARCADE");

	Button* buttons[] =
	{
		&m_playButton,
		&m_levelEditorButton,
		&m_backButton
	};

	for (Button* button : buttons)
	{
		button->box.setSize({ 360.f, 64.f });
		button->box.setOutlineThickness(3.f);

		if (button->text)
		{
			button->text->setCharacterSize(28);
			button->text->setOutlineColor(sf::Color::Black);
			button->text->setOutlineThickness(2.f);
		}
	}

	refreshSelectionVisuals();
	return true;
}

void GAME1_Menu::layout(const sf::RenderWindow& window)
{
	startMusic();

	const float windowWidth = static_cast<float>(window.getSize().x);
	const float windowHeight = static_cast<float>(window.getSize().y);

	if (m_titleText)
	{
		const sf::FloatRect bounds = m_titleText->getLocalBounds();
		m_titleText->setPosition({
			(windowWidth - bounds.size.x) * 0.5f - bounds.position.x,
			78.f - bounds.position.y
			});
	}

	if (m_subtitleText)
	{
		const sf::FloatRect bounds = m_subtitleText->getLocalBounds();
		m_subtitleText->setPosition({
			(windowWidth - bounds.size.x) * 0.5f - bounds.position.x,
			154.f - bounds.position.y
			});
	}

	const float buttonX = (windowWidth - m_playButton.box.getSize().x) * 0.5f;
	const float startY = windowHeight * 0.5f - 70.f;
	const float spacing = 86.f;

	m_playButton.box.setPosition({ buttonX, startY });
	m_levelEditorButton.box.setPosition({ buttonX, startY + spacing });
	m_backButton.box.setPosition({ buttonX, startY + spacing * 2.f });

	centerTextInButton(m_playButton);
	centerTextInButton(m_levelEditorButton);
	centerTextInButton(m_backButton);
}

void GAME1_Menu::centerTextInButton(Button& button)
{
	if (!button.text)
		return;

	const sf::FloatRect buttonBounds = button.box.getGlobalBounds();
	const sf::FloatRect textBounds = button.text->getLocalBounds();

	button.text->setPosition({
		buttonBounds.position.x + (buttonBounds.size.x - textBounds.size.x) * 0.5f - textBounds.position.x,
		buttonBounds.position.y + (buttonBounds.size.y - textBounds.size.y) * 0.5f - textBounds.position.y - 2.f
		});
}

GAME1_MenuAction GAME1_Menu::handleClick(sf::Vector2f mousePosition)
{
	for (int i = 0; i < 3; ++i)
	{
		Button* button = getButtonByIndex(i);
		if (button != nullptr && containsPoint(button->box.getGlobalBounds(), mousePosition))
		{
			m_selectedButtonIndex = i;
			refreshSelectionVisuals();

			if (button->action != GAME1_MenuAction::Play)
				stopMusic();

			return button->action;
		}
	}

	return GAME1_MenuAction::None;
}

void GAME1_Menu::selectPreviousButton()
{
	m_selectedButtonIndex = (m_selectedButtonIndex + 2) % 3;
	refreshSelectionVisuals();
}

void GAME1_Menu::selectNextButton()
{
	m_selectedButtonIndex = (m_selectedButtonIndex + 1) % 3;
	refreshSelectionVisuals();
}

GAME1_MenuAction GAME1_Menu::activateSelectedButton() const
{
	const Button* button = getButtonByIndex(m_selectedButtonIndex);
	return button != nullptr ? button->action : GAME1_MenuAction::None;
}

void GAME1_Menu::startMusic()
{
	GAME1_SurfersQuestAudio::playMenu();
}

void GAME1_Menu::stopMusic()
{
	GAME1_SurfersQuestAudio::stopAll();
}

void GAME1_Menu::refreshSelectionVisuals()
{
	for (int i = 0; i < 3; ++i)
	{
		Button* button = getButtonByIndex(i);
		if (button == nullptr)
			continue;

		const bool selected = i == m_selectedButtonIndex;

		button->box.setFillColor(selected ? sf::Color(55, 55, 78) : sf::Color(35, 35, 45));
		button->box.setOutlineColor(selected ? sf::Color(255, 220, 120) : sf::Color::White);

		if (button->text)
		{
			button->text->setFillColor(selected ? sf::Color(255, 220, 120) : sf::Color::White);
		}
	}
}

GAME1_Menu::Button* GAME1_Menu::getButtonByIndex(int index)
{
	switch (index)
	{
	case 0: return &m_playButton;
	case 1: return &m_levelEditorButton;
	case 2: return &m_backButton;
	default: return nullptr;
	}
}

const GAME1_Menu::Button* GAME1_Menu::getButtonByIndex(int index) const
{
	switch (index)
	{
	case 0: return &m_playButton;
	case 1: return &m_levelEditorButton;
	case 2: return &m_backButton;
	default: return nullptr;
	}
}

void GAME1_Menu::draw(sf::RenderWindow& window) const
{
	const sf::Vector2u windowSize = window.getSize();

	sf::RectangleShape background;
	background.setPosition({ 0.f, 0.f });
	background.setSize({
		static_cast<float>(windowSize.x),
		static_cast<float>(windowSize.y)
		});
	background.setFillColor(sf::Color(18, 18, 28));
	window.draw(background);

	if (m_titleText)
		window.draw(*m_titleText);

	if (m_subtitleText)
		window.draw(*m_subtitleText);

	const Button* buttons[] =
	{
		&m_playButton,
		&m_levelEditorButton,
		&m_backButton
	};

	for (const Button* button : buttons)
	{
		window.draw(button->box);

		if (button->text)
			window.draw(*button->text);
	}
}

const std::string& GAME1_Menu::getLastError() const
{
	return m_lastError;
}

bool GAME1_Menu::containsPoint(const sf::FloatRect& bounds, sf::Vector2f point)
{
	return point.x >= bounds.position.x &&
		point.x <= bounds.position.x + bounds.size.x &&
		point.y >= bounds.position.y &&
		point.y <= bounds.position.y + bounds.size.y;
}
