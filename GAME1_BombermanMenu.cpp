#include "GAME1_BombermanMenu.h"

#include <filesystem>

bool GAME1_BombermanMenu::load(const std::string& fontPath, const std::string& bombermanRootDirectory)
{
	namespace fs = std::filesystem;

	m_lastError.clear();
	m_hasMusic = false;
	m_selectedButtonIndex = 0;

	if (!m_font.openFromFile(fontPath))
	{
		m_lastError = "Failed to load Bomberman menu font: " + fontPath;
		return false;
	}

	m_titleText.emplace(m_font);
	m_titleText->setString("BOMBERMAN");
	m_titleText->setCharacterSize(72);
	m_titleText->setFillColor(sf::Color::White);
	m_titleText->setOutlineColor(sf::Color::Black);
	m_titleText->setOutlineThickness(4.f);

	m_subtitleText.emplace(m_font);
	m_subtitleText->setString("Enhanced Edition");
	m_subtitleText->setCharacterSize(30);
	m_subtitleText->setFillColor(sf::Color(255, 220, 120));
	m_subtitleText->setOutlineColor(sf::Color::Black);
	m_subtitleText->setOutlineThickness(2.f);

	m_playLevelsButton.action = GAME1_BombermanMenuAction::PlayLevels;
	m_playLevelsButton.text.emplace(m_font);
	m_playLevelsButton.text->setString("PLAY LEVELS");

	m_levelEditorButton.action = GAME1_BombermanMenuAction::LevelEditor;
	m_levelEditorButton.text.emplace(m_font);
	m_levelEditorButton.text->setString("LEVEL EDITOR");

	m_backButton.action = GAME1_BombermanMenuAction::BackToHub;
	m_backButton.text.emplace(m_font);
	m_backButton.text->setString("BACK TO ARCADE");

	Button* buttons[] =
	{
		&m_playLevelsButton,
		&m_levelEditorButton,
		&m_backButton
	};

	for (Button* button : buttons)
	{
		button->box.setSize({ 340.f, 64.f });
		button->box.setOutlineThickness(3.f);

		if (button->text)
		{
			button->text->setCharacterSize(28);
			button->text->setOutlineColor(sf::Color::Black);
			button->text->setOutlineThickness(2.f);
		}
	}

	refreshSelectionVisuals();

	const fs::path audioDirectory = fs::path(bombermanRootDirectory) / "Resources" / "Audio";

	tryOpenMusicFile(
		{
			(audioDirectory / "Bomberman_Soundtrack.ogg").string(),
			(audioDirectory / "Bomberman_Soundtrack.wav").string(),
			(audioDirectory / "BombermanSoundtrack.ogg").string(),
			(audioDirectory / "BombermanSoundtrack.wav").string(),
			(audioDirectory / "Soundtrack.ogg").string(),
			(audioDirectory / "Soundtrack.wav").string(),
			(audioDirectory / "soundtrack.ogg").string(),
			(audioDirectory / "soundtrack.wav").string()
		});

	return true;
}

bool GAME1_BombermanMenu::tryOpenMusicFile(const std::vector<std::string>& candidatePaths)
{
	for (const std::string& path : candidatePaths)
	{
		if (m_music.openFromFile(path))
		{
			m_music.setLooping(true);
			m_music.setVolume(65.f);
			m_hasMusic = true;
			return true;
		}
	}

	m_hasMusic = false;
	return false;
}

void GAME1_BombermanMenu::startMusic()
{
	if (!m_hasMusic)
		return;

	if (m_music.getStatus() != sf::SoundSource::Status::Playing)
	{
		m_music.play();
	}
}

void GAME1_BombermanMenu::stopMusic()
{
	if (!m_hasMusic)
		return;

	if (m_music.getStatus() == sf::SoundSource::Status::Playing)
	{
		m_music.stop();
	}
}

void GAME1_BombermanMenu::layout(const sf::RenderWindow& window)
{
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

	const float buttonX = (windowWidth - m_playLevelsButton.box.getSize().x) * 0.5f;
	const float startY = windowHeight * 0.5f - 70.f;
	const float spacing = 86.f;

	m_playLevelsButton.box.setPosition({ buttonX, startY });
	m_levelEditorButton.box.setPosition({ buttonX, startY + spacing });
	m_backButton.box.setPosition({ buttonX, startY + spacing * 2.f });

	centerTextInButton(m_playLevelsButton);
	centerTextInButton(m_levelEditorButton);
	centerTextInButton(m_backButton);
}

void GAME1_BombermanMenu::centerTextInButton(Button& button)
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

GAME1_BombermanMenuAction GAME1_BombermanMenu::handleClick(sf::Vector2f mousePosition)
{
	for (int i = 0; i < 3; ++i)
	{
		Button* button = getButtonByIndex(i);
		if (button != nullptr && containsPoint(button->box.getGlobalBounds(), mousePosition))
		{
			m_selectedButtonIndex = i;
			refreshSelectionVisuals();
			return button->action;
		}
	}

	return GAME1_BombermanMenuAction::None;
}

void GAME1_BombermanMenu::selectPreviousButton()
{
	m_selectedButtonIndex = (m_selectedButtonIndex + 2) % 3;
	refreshSelectionVisuals();
}

void GAME1_BombermanMenu::selectNextButton()
{
	m_selectedButtonIndex = (m_selectedButtonIndex + 1) % 3;
	refreshSelectionVisuals();
}

GAME1_BombermanMenuAction GAME1_BombermanMenu::activateSelectedButton() const
{
	const Button* button = getButtonByIndex(m_selectedButtonIndex);
	return button != nullptr ? button->action : GAME1_BombermanMenuAction::None;
}

void GAME1_BombermanMenu::refreshSelectionVisuals()
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

GAME1_BombermanMenu::Button* GAME1_BombermanMenu::getButtonByIndex(int index)
{
	switch (index)
	{
	case 0: return &m_playLevelsButton;
	case 1: return &m_levelEditorButton;
	case 2: return &m_backButton;
	default: return nullptr;
	}
}

const GAME1_BombermanMenu::Button* GAME1_BombermanMenu::getButtonByIndex(int index) const
{
	switch (index)
	{
	case 0: return &m_playLevelsButton;
	case 1: return &m_levelEditorButton;
	case 2: return &m_backButton;
	default: return nullptr;
	}
}

void GAME1_BombermanMenu::draw(sf::RenderWindow& window) const
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
		&m_playLevelsButton,
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

const std::string& GAME1_BombermanMenu::getLastError() const
{
	return m_lastError;
}

bool GAME1_BombermanMenu::containsPoint(const sf::FloatRect& bounds, sf::Vector2f point)
{
	return point.x >= bounds.position.x &&
		point.x <= bounds.position.x + bounds.size.x &&
		point.y >= bounds.position.y &&
		point.y <= bounds.position.y + bounds.size.y;
}
