#include "GAME1_Menu.h"

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

	return true;
}

void GAME1_Menu::layout(const sf::RenderWindow& window)
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

GAME1_MenuAction GAME1_Menu::handleClick(sf::Vector2f mousePosition) const
{
	const Button* buttons[] =
	{
		&m_playButton,
		&m_levelEditorButton,
		&m_backButton
	};

	for (const Button* button : buttons)
	{
		if (containsPoint(button->box.getGlobalBounds(), mousePosition))
			return button->action;
	}

	return GAME1_MenuAction::None;
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