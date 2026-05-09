#include "GAME1_BombermanMenu.h"

#include <filesystem>

bool GAME1_BombermanMenu::load(const std::string& fontPath, const std::string& bombermanRootDirectory)
{
	namespace fs = std::filesystem;

	m_lastError.clear();
	m_hasMusic = false;

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
		button->box.setFillColor(sf::Color(35, 35, 45));
		button->box.setOutlineColor(sf::Color::White);
		button->box.setOutlineThickness(3.f);

		if (button->text)
		{
			button->text->setCharacterSize(28);
			button->text->setFillColor(sf::Color::White);
			button->text->setOutlineColor(sf::Color::Black);
			button->text->setOutlineThickness(2.f);
		}
	}

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

GAME1_BombermanMenuAction GAME1_BombermanMenu::handleClick(sf::Vector2f mousePosition) const
{
	const Button* buttons[] =
	{
		&m_playLevelsButton,
		&m_levelEditorButton,
		&m_backButton
	};

	for (const Button* button : buttons)
	{
		if (containsPoint(button->box.getGlobalBounds(), mousePosition))
			return button->action;
	}

	return GAME1_BombermanMenuAction::None;
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

	sf::RectangleShape frame;
	frame.setPosition({ 24.f, 24.f });
	frame.setSize({
		static_cast<float>(windowSize.x) - 48.f,
		static_cast<float>(windowSize.y) - 48.f
		});
	frame.setFillColor(sf::Color::Transparent);
	frame.setOutlineColor(sf::Color(255, 230, 120));
	frame.setOutlineThickness(3.f);
	window.draw(frame);

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