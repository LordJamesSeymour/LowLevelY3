#include "GAME1_Menu.h"
#include "ArcadeInput.h"

#include <algorithm>
#include <filesystem>

bool GAME1_Menu::load(const std::string& closeButtonTexturePath,
	const std::string& logoTexturePath,
	const std::string& playButtonTexturePath,
	const std::string& levelEditorButtonTexturePath)
{
	(void)closeButtonTexturePath;
	(void)logoTexturePath;
	(void)playButtonTexturePath;
	(void)levelEditorButtonTexturePath;

	m_lastError.clear();
	m_hasMusic = false;
	m_controlsPopupOpen = false;
	m_controlsPopupPage = 0;

	const std::vector<std::string> fontCandidates =
	{
		"assets/menu.ttf",
		"Assets/menu.ttf",
		"resources/menu.ttf",
		"Resources/menu.ttf"
	};

	bool loadedFont = false;
	for (const std::string& fontPath : fontCandidates)
	{
		if (m_font.openFromFile(fontPath))
		{
			loadedFont = true;
			break;
		}
	}

	if (!loadedFont)
	{
		m_lastError = "Failed to load SurfersQuest menu font. Expected assets/menu.ttf.";
		return false;
	}

	m_titleText.emplace(m_font);
	m_titleText->setString("SURFERS QUEST");
	m_titleText->setCharacterSize(62);
	m_titleText->setFillColor(sf::Color::White);
	m_titleText->setOutlineColor(sf::Color::Black);
	m_titleText->setOutlineThickness(4.f);

	m_subtitleText.emplace(m_font);
	m_subtitleText->setString("Platformer Adventure");
	m_subtitleText->setCharacterSize(28);
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

	Button* menuButtons[] =
	{
		&m_playButton,
		&m_levelEditorButton,
		&m_backButton
	};

	for (Button* button : menuButtons)
	{
		button->box.setSize({ 370.f, 64.f });
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

	m_controlsButton.action = GAME1_MenuAction::None;
	m_controlsButton.box.setSize({ 58.f, 58.f });
	m_controlsButton.box.setFillColor(sf::Color(35, 35, 45));
	m_controlsButton.box.setOutlineColor(sf::Color(255, 230, 120));
	m_controlsButton.box.setOutlineThickness(3.f);
	m_controlsButton.text.emplace(m_font);
	m_controlsButton.text->setString("(?)");
	m_controlsButton.text->setCharacterSize(24);
	m_controlsButton.text->setFillColor(sf::Color::White);
	m_controlsButton.text->setOutlineColor(sf::Color::Black);
	m_controlsButton.text->setOutlineThickness(2.f);

	resetSelection();

	tryOpenMusicFile(
		{
			"assets/Game#1/SurfersQuest/Resources/Audio/SurfersQuest_Soundtrack.ogg",
			"assets/Game#1/SurfersQuest/Resources/Audio/SurfersQuest_Soundtrack.wav",
			"assets/Game#1/SurfersQuest/Resources/Audio/SurfersQuestSoundtrack.ogg",
			"assets/Game#1/SurfersQuest/Resources/Audio/SurfersQuestSoundtrack.wav",
			"assets/Game#1/SurfersQuest/Resources/Audio/MenuMusic.ogg",
			"assets/Game#1/SurfersQuest/Resources/Audio/MenuMusic.wav",
			"assets/Game#1/SurfersQuest/Resources/Audio/Soundtrack.ogg",
			"assets/Game#1/SurfersQuest/Resources/Audio/Soundtrack.wav",
			"assets/Game#1/SurfersQuest/Resources/Audio/soundtrack.ogg",
			"assets/Game#1/SurfersQuest/Resources/Audio/soundtrack.wav"
		});

	return true;
}

bool GAME1_Menu::tryOpenMusicFile(const std::vector<std::string>& candidatePaths)
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

void GAME1_Menu::startMusic()
{
	if (!m_hasMusic)
		return;

	if (m_music.getStatus() != sf::SoundSource::Status::Playing)
	{
		m_music.play();
	}
}

void GAME1_Menu::stopMusic()
{
	if (!m_hasMusic)
		return;

	if (m_music.getStatus() == sf::SoundSource::Status::Playing)
	{
		m_music.stop();
	}
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

	m_controlsButton.box.setPosition({ windowWidth - 92.f, 36.f });
	centerTextInButton(m_controlsButton);

	const float arrowSize = 56.f;
	const float arrowPadding = 24.f;
	const float screenEdgePadding = 18.f;

	const float horizontalSpaceNeededForArrows =
		(arrowSize + arrowPadding + screenEdgePadding) * 2.f;

	const float popupWidth = std::min(
		760.f,
		std::max(520.f, windowWidth - horizontalSpaceNeededForArrows)
	);

	const float popupHeight = std::min(
		450.f,
		std::max(350.f, windowHeight - 150.f)
	);

	m_popupBounds = sf::FloatRect(
		{ (windowWidth - popupWidth) * 0.5f, (windowHeight - popupHeight) * 0.5f },
		{ popupWidth, popupHeight }
	);

	const float arrowY =
		m_popupBounds.position.y +
		(m_popupBounds.size.y - arrowSize) * 0.5f;

	m_popupPreviousPageButtonBounds = sf::FloatRect(
		{ m_popupBounds.position.x - arrowSize - arrowPadding, arrowY },
		{ arrowSize, arrowSize }
	);

	m_popupNextPageButtonBounds = sf::FloatRect(
		{ m_popupBounds.position.x + m_popupBounds.size.x + arrowPadding, arrowY },
		{ arrowSize, arrowSize }
	);

	updateSelectionVisuals();
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

GAME1_Menu::Button* GAME1_Menu::getButtonForSelection(MenuSelection selection)
{
	switch (selection)
	{
	case MenuSelection::Play:
		return &m_playButton;

	case MenuSelection::LevelEditor:
		return &m_levelEditorButton;

	case MenuSelection::BackToArcade:
		return &m_backButton;

	case MenuSelection::Controls:
	default:
		return &m_controlsButton;
	}
}

const GAME1_Menu::Button* GAME1_Menu::getButtonForSelection(MenuSelection selection) const
{
	switch (selection)
	{
	case MenuSelection::Play:
		return &m_playButton;

	case MenuSelection::LevelEditor:
		return &m_levelEditorButton;

	case MenuSelection::BackToArcade:
		return &m_backButton;

	case MenuSelection::Controls:
	default:
		return &m_controlsButton;
	}
}

void GAME1_Menu::setSelectedItem(MenuSelection selection)
{
	m_selectedItem = selection;
	updateSelectionVisuals();
}

void GAME1_Menu::resetSelection()
{
	setSelectedItem(MenuSelection::Play);
}

void GAME1_Menu::selectPreviousItem()
{
	const int current = static_cast<int>(m_selectedItem);
	const int previous = (current + 3) % 4;
	setSelectedItem(static_cast<MenuSelection>(previous));
}

void GAME1_Menu::selectNextItem()
{
	const int current = static_cast<int>(m_selectedItem);
	const int next = (current + 1) % 4;
	setSelectedItem(static_cast<MenuSelection>(next));
}

void GAME1_Menu::updateSelectionVisuals()
{
	Button* buttons[] =
	{
		&m_playButton,
		&m_levelEditorButton,
		&m_backButton,
		&m_controlsButton
	};

	for (Button* button : buttons)
	{
		button->box.setFillColor(sf::Color(35, 35, 45));
		button->box.setOutlineColor(sf::Color::White);
		button->box.setOutlineThickness(3.f);

		if (button->text)
		{
			button->text->setFillColor(sf::Color::White);
		}
	}

	Button* selectedButton = getButtonForSelection(m_selectedItem);
	if (selectedButton == nullptr)
		return;

	selectedButton->box.setFillColor(sf::Color(70, 70, 105));
	selectedButton->box.setOutlineColor(sf::Color(255, 220, 120));
	selectedButton->box.setOutlineThickness(3.f);

	if (selectedButton->text)
	{
		selectedButton->text->setFillColor(sf::Color(255, 220, 120));
	}
}

GAME1_MenuAction GAME1_Menu::activateSelectedItem()
{
	if (m_controlsPopupOpen)
		return GAME1_MenuAction::None;

	if (m_selectedItem == MenuSelection::Controls)
	{
		m_controlsPopupOpen = true;
		return GAME1_MenuAction::None;
	}

	const Button* selectedButton = getButtonForSelection(m_selectedItem);
	return selectedButton != nullptr ? selectedButton->action : GAME1_MenuAction::None;
}

GAME1_MenuAction GAME1_Menu::handleControllerInput()
{
	if (m_controlsPopupOpen)
	{
		if (ArcadeInput::isControllerCancelPressed() || ArcadeInput::isControllerBackPressed())
		{
			m_controlsPopupOpen = false;
			return GAME1_MenuAction::None;
		}

		if (ArcadeInput::isControllerMoveLeftPressed() || ArcadeInput::isControllerMoveUpPressed())
		{
			showPreviousControlsPage();
			return GAME1_MenuAction::None;
		}

		if (ArcadeInput::isControllerMoveRightPressed() || ArcadeInput::isControllerMoveDownPressed())
		{
			showNextControlsPage();
			return GAME1_MenuAction::None;
		}

		return GAME1_MenuAction::None;
	}

	if (ArcadeInput::isControllerMoveUpPressed())
	{
		selectPreviousItem();
		return GAME1_MenuAction::None;
	}

	if (ArcadeInput::isControllerMoveDownPressed())
	{
		selectNextItem();
		return GAME1_MenuAction::None;
	}

	if (ArcadeInput::isControllerConfirmPressed())
	{
		return activateSelectedItem();
	}

	if (ArcadeInput::isControllerCancelPressed() || ArcadeInput::isControllerBackPressed())
	{
		return GAME1_MenuAction::Quit;
	}

	return GAME1_MenuAction::None;
}

GAME1_MenuAction GAME1_Menu::handleClick(sf::Vector2f mousePosition)
{
	if (containsPoint(m_controlsButton.box.getGlobalBounds(), mousePosition))
	{
		setSelectedItem(MenuSelection::Controls);
		m_controlsPopupOpen = !m_controlsPopupOpen;
		return GAME1_MenuAction::None;
	}

	if (m_controlsPopupOpen)
	{
		if (containsPoint(m_popupPreviousPageButtonBounds, mousePosition))
		{
			showPreviousControlsPage();
			return GAME1_MenuAction::None;
		}

		if (containsPoint(m_popupNextPageButtonBounds, mousePosition))
		{
			showNextControlsPage();
			return GAME1_MenuAction::None;
		}

		return GAME1_MenuAction::None;
	}

	if (containsPoint(m_playButton.box.getGlobalBounds(), mousePosition))
	{
		setSelectedItem(MenuSelection::Play);
		return m_playButton.action;
	}

	if (containsPoint(m_levelEditorButton.box.getGlobalBounds(), mousePosition))
	{
		setSelectedItem(MenuSelection::LevelEditor);
		return m_levelEditorButton.action;
	}

	if (containsPoint(m_backButton.box.getGlobalBounds(), mousePosition))
	{
		setSelectedItem(MenuSelection::BackToArcade);
		return m_backButton.action;
	}

	return GAME1_MenuAction::None;
}

void GAME1_Menu::handleMouseMoved(sf::Vector2f mousePosition)
{
	if (m_controlsPopupOpen)
		return;

	if (containsPoint(m_playButton.box.getGlobalBounds(), mousePosition))
	{
		setSelectedItem(MenuSelection::Play);
	}
	else if (containsPoint(m_levelEditorButton.box.getGlobalBounds(), mousePosition))
	{
		setSelectedItem(MenuSelection::LevelEditor);
	}
	else if (containsPoint(m_backButton.box.getGlobalBounds(), mousePosition))
	{
		setSelectedItem(MenuSelection::BackToArcade);
	}
	else if (containsPoint(m_controlsButton.box.getGlobalBounds(), mousePosition))
	{
		setSelectedItem(MenuSelection::Controls);
	}
}

bool GAME1_Menu::handleKeyReleased(sf::Keyboard::Key key)
{
	if (m_controlsPopupOpen)
	{
		if (key == sf::Keyboard::Key::Escape)
		{
			m_controlsPopupOpen = false;
			return true;
		}

		if (key == sf::Keyboard::Key::Left || key == sf::Keyboard::Key::A ||
			key == sf::Keyboard::Key::Up || key == sf::Keyboard::Key::W)
		{
			showPreviousControlsPage();
			return true;
		}

		if (key == sf::Keyboard::Key::Right || key == sf::Keyboard::Key::D ||
			key == sf::Keyboard::Key::Down || key == sf::Keyboard::Key::S)
		{
			showNextControlsPage();
			return true;
		}

		return true;
	}

	if (key == sf::Keyboard::Key::Up || key == sf::Keyboard::Key::W)
	{
		selectPreviousItem();
		return true;
	}

	if (key == sf::Keyboard::Key::Down || key == sf::Keyboard::Key::S)
	{
		selectNextItem();
		return true;
	}

	return false;
}

void GAME1_Menu::showPreviousControlsPage()
{
	m_controlsPopupPage = m_controlsPopupPage == 0 ? 1 : 0;
}

void GAME1_Menu::showNextControlsPage()
{
	m_controlsPopupPage = m_controlsPopupPage == 0 ? 1 : 0;
}

std::string GAME1_Menu::getControlsPopupTitle() const
{
	return m_controlsPopupPage == 0
		? "Game Controls"
		: "Level Editor Controls";
}

std::string GAME1_Menu::getControlsPopupBody() const
{
	if (m_controlsPopupPage == 0)
	{
		return
			"A / D / Arrow Keys / D-pad = Move\n"
			"Space / Controller A = Jump\n"
			"Space / Controller A again = Double jump\n"
			"Hold into a wall = Wall grab\n"
			"Jump while wall grabbing = Wall jump\n"
			"Left Shift / Controller B = Drop through platform\n"
			"ESC / Select = Return to menu\n\n"
			"Avoid spikes, stomp enemies from above,\n"
			"and reach the end of the level.";
	}

	return
		"Left Click = Place tile / select toolbar button\n"
		"Right Click = Erase tile\n"
		"Middle Mouse = Pick tile / tool\n"
		"Mouse Wheel = Cycle visible tools\n"
		"1-9 = Select visible hotbar slot\n"
		"Letter Keys = Select tile by tile letter\n"
		"F5 = Save level\n"
		"World arrows = Switch world palette\n"
		"Load arrows = Choose saved map, Load opens it\n"
		"Hotbar < / > = Change tool page";
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
		&m_backButton,
		&m_controlsButton
	};

	for (const Button* button : buttons)
	{
		window.draw(button->box);

		if (button->text)
			window.draw(*button->text);
	}

	if (m_controlsPopupOpen)
	{
		drawControlsPopup(window, windowSize);
	}
}

void GAME1_Menu::drawTextCentered(sf::RenderTarget& target,
	const std::string& string,
	unsigned int size,
	const sf::FloatRect& rect,
	sf::Color fill,
	float outlineThickness) const
{
	sf::Text text(m_font);
	text.setString(string);
	text.setCharacterSize(size);
	text.setFillColor(fill);
	text.setOutlineColor(sf::Color::Black);
	text.setOutlineThickness(outlineThickness);

	const sf::FloatRect bounds = text.getLocalBounds();

	text.setPosition({
		rect.position.x + (rect.size.x - bounds.size.x) * 0.5f - bounds.position.x,
		rect.position.y + (rect.size.y - bounds.size.y) * 0.5f - bounds.position.y - 1.f
		});

	target.draw(text);
}

void GAME1_Menu::drawControlsPopup(sf::RenderTarget& target, sf::Vector2u windowSize) const
{
	sf::RectangleShape overlay;
	overlay.setPosition({ 0.f, 0.f });
	overlay.setSize({ static_cast<float>(windowSize.x), static_cast<float>(windowSize.y) });
	overlay.setFillColor(sf::Color(0, 0, 0, 155));
	target.draw(overlay);

	sf::RectangleShape popup;
	popup.setPosition(m_popupBounds.position);
	popup.setSize(m_popupBounds.size);
	popup.setFillColor(sf::Color(35, 35, 45));
	popup.setOutlineColor(sf::Color(255, 230, 120));
	popup.setOutlineThickness(3.f);
	target.draw(popup);

	sf::RectangleShape innerFrame;
	innerFrame.setPosition({ m_popupBounds.position.x + 12.f, m_popupBounds.position.y + 12.f });
	innerFrame.setSize({ m_popupBounds.size.x - 24.f, m_popupBounds.size.y - 24.f });
	innerFrame.setFillColor(sf::Color::Transparent);
	innerFrame.setOutlineColor(sf::Color::White);
	innerFrame.setOutlineThickness(1.5f);
	target.draw(innerFrame);

	drawTextCentered(
		target,
		getControlsPopupTitle(),
		34,
		sf::FloatRect(
			{ m_popupBounds.position.x, m_popupBounds.position.y + 28.f },
			{ m_popupBounds.size.x, 48.f }),
		sf::Color::White,
		2.f);

	const sf::FloatRect bodyRect(
		{ m_popupBounds.position.x + 80.f, m_popupBounds.position.y + 106.f },
		{ m_popupBounds.size.x - 160.f, m_popupBounds.size.y - 162.f });

	sf::Text bodyText(m_font);
	bodyText.setString(getControlsPopupBody());
	bodyText.setCharacterSize(m_controlsPopupPage == 0 ? 19 : 18);
	bodyText.setFillColor(sf::Color(230, 230, 230));
	bodyText.setOutlineColor(sf::Color::Black);
	bodyText.setOutlineThickness(1.25f);
	bodyText.setLineSpacing(1.12f);

	const sf::FloatRect bodyBounds = bodyText.getLocalBounds();

	bodyText.setPosition({
		bodyRect.position.x - bodyBounds.position.x,
		bodyRect.position.y + (bodyRect.size.y - bodyBounds.size.y) * 0.5f - bodyBounds.position.y
		});

	target.draw(bodyText);

	sf::RectangleShape previousButton;
	previousButton.setPosition(m_popupPreviousPageButtonBounds.position);
	previousButton.setSize(m_popupPreviousPageButtonBounds.size);
	previousButton.setFillColor(sf::Color(45, 45, 70));
	previousButton.setOutlineColor(sf::Color::White);
	previousButton.setOutlineThickness(2.f);
	target.draw(previousButton);
	drawTextCentered(target, "<", 34, m_popupPreviousPageButtonBounds, sf::Color::White, 2.f);

	sf::RectangleShape nextButton;
	nextButton.setPosition(m_popupNextPageButtonBounds.position);
	nextButton.setSize(m_popupNextPageButtonBounds.size);
	nextButton.setFillColor(sf::Color(45, 45, 70));
	nextButton.setOutlineColor(sf::Color::White);
	nextButton.setOutlineThickness(2.f);
	target.draw(nextButton);
	drawTextCentered(target, ">", 34, m_popupNextPageButtonBounds, sf::Color::White, 2.f);

	drawTextCentered(
		target,
		"Page " + std::to_string(m_controlsPopupPage + 1) + " / 2",
		16,
		sf::FloatRect(
			{ m_popupBounds.position.x, m_popupBounds.position.y + m_popupBounds.size.y - 46.f },
			{ m_popupBounds.size.x, 28.f }),
		sf::Color(255, 230, 120),
		1.5f);
}

bool GAME1_Menu::isControlsPopupOpen() const
{
	return m_controlsPopupOpen;
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
