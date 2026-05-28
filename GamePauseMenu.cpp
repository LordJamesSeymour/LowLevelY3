#include "GamePauseMenu.h"

#include "ArcadeInput.h"

#include <algorithm>

bool GamePauseMenu::load(const std::string& fontPath,
	const std::string& settingsIconPath)
{
	m_lastError.clear();
	m_hasSettingsIcon = false;

	if (!m_font.openFromFile(fontPath))
	{
		m_lastError = "Failed to load pause menu font: " + fontPath;
		return false;
	}

	if (!settingsIconPath.empty())
	{
		if (m_settingsIconTexture.loadFromFile(settingsIconPath))
		{
			m_hasSettingsIcon = true;
			m_settingsIconTexture.setSmooth(true);
		}
	}

	return true;
}

void GamePauseMenu::layout(const sf::RenderWindow& window)
{
	m_lastLayoutSize = window.getSize();
	const float width = static_cast<float>(m_lastLayoutSize.x);
	const float height = static_cast<float>(m_lastLayoutSize.y);

	const float panelWidth = std::min(520.f, std::max(360.f, width - 200.f));
	const float panelHeight = std::min(360.f, std::max(280.f, height - 200.f));
	const float panelX = (width - panelWidth) * 0.5f;
	const float panelY = (height - panelHeight) * 0.5f;

	m_panelBounds = sf::FloatRect({ panelX, panelY }, { panelWidth, panelHeight });

	const float buttonWidth = panelWidth - 120.f;
	const float buttonHeight = 58.f;
	const float buttonX = panelX + (panelWidth - buttonWidth) * 0.5f;

	const float buttonsTop = panelY + 130.f;
	const float buttonGap = 24.f;

	m_resumeBounds = sf::FloatRect(
		{ buttonX, buttonsTop },
		{ buttonWidth, buttonHeight }
	);

	m_quitBounds = sf::FloatRect(
		{ buttonX, buttonsTop + buttonHeight + buttonGap },
		{ buttonWidth, buttonHeight }
	);

	const float settingsSize = 48.f;
	const float settingsMargin = 14.f;

	m_settingsBounds = sf::FloatRect(
		{
			panelX + panelWidth - settingsSize - settingsMargin,
			panelY + settingsMargin
		},
		{ settingsSize, settingsSize }
	);
}

bool GamePauseMenu::isOpen() const
{
	return m_isOpen;
}

void GamePauseMenu::open()
{
	m_isOpen = true;
	m_selectedItem = Item::Resume;
	ArcadeInput::consumePressedState();
}

void GamePauseMenu::close()
{
	m_isOpen = false;
	ArcadeInput::consumePressedState();
}

bool GamePauseMenu::containsPoint(const sf::FloatRect& rect, sf::Vector2f point)
{
	return point.x >= rect.position.x &&
		point.x <= rect.position.x + rect.size.x &&
		point.y >= rect.position.y &&
		point.y <= rect.position.y + rect.size.y;
}

GamePauseMenu::Action GamePauseMenu::activate(Item item) const
{
	switch (item)
	{
	case Item::Resume:   return Action::Resume;
	case Item::Quit:     return Action::Quit;
	case Item::Settings: return Action::OpenSettings;
	default:             return Action::None;
	}
}

void GamePauseMenu::moveSelection(int delta)
{
	int selection = static_cast<int>(m_selectedItem) + delta;
	const int count = static_cast<int>(Item::Count);

	if (selection < 0)
		selection = count - 1;

	if (selection >= count)
		selection = 0;

	m_selectedItem = static_cast<Item>(selection);
}

GamePauseMenu::Action GamePauseMenu::handleKeyReleased(sf::Keyboard::Key key)
{
	if (!m_isOpen)
		return Action::None;

	if (key == sf::Keyboard::Key::Up ||
		key == sf::Keyboard::Key::W ||
		key == sf::Keyboard::Key::Left ||
		key == sf::Keyboard::Key::A)
	{
		moveSelection(-1);
		return Action::None;
	}

	if (key == sf::Keyboard::Key::Down ||
		key == sf::Keyboard::Key::S ||
		key == sf::Keyboard::Key::Right ||
		key == sf::Keyboard::Key::D)
	{
		moveSelection(+1);
		return Action::None;
	}

	if (key == sf::Keyboard::Key::Enter ||
		key == sf::Keyboard::Key::Space)
	{
		return activate(m_selectedItem);
	}

	return Action::None;
}

GamePauseMenu::Action GamePauseMenu::handleMousePressed(sf::Vector2f mousePosition)
{
	if (!m_isOpen)
		return Action::None;

	if (containsPoint(m_settingsBounds, mousePosition))
	{
		m_selectedItem = Item::Settings;
		return Action::OpenSettings;
	}

	if (containsPoint(m_resumeBounds, mousePosition))
	{
		m_selectedItem = Item::Resume;
		return Action::Resume;
	}

	if (containsPoint(m_quitBounds, mousePosition))
	{
		m_selectedItem = Item::Quit;
		return Action::Quit;
	}

	return Action::None;
}

void GamePauseMenu::handleMouseMoved(sf::Vector2f mousePosition)
{
	if (!m_isOpen)
		return;

	if (containsPoint(m_resumeBounds, mousePosition))
	{
		m_selectedItem = Item::Resume;
	}
	else if (containsPoint(m_quitBounds, mousePosition))
	{
		m_selectedItem = Item::Quit;
	}
	else if (containsPoint(m_settingsBounds, mousePosition))
	{
		m_selectedItem = Item::Settings;
	}
}

GamePauseMenu::Action GamePauseMenu::handleControllerInput()
{
	if (!m_isOpen)
		return Action::None;

	if (ArcadeInput::isControllerMoveUpPressed() ||
		ArcadeInput::isControllerMoveLeftPressed())
	{
		moveSelection(-1);
		ArcadeInput::consumePressedState();
		return Action::None;
	}

	if (ArcadeInput::isControllerMoveDownPressed() ||
		ArcadeInput::isControllerMoveRightPressed())
	{
		moveSelection(+1);
		ArcadeInput::consumePressedState();
		return Action::None;
	}

	if (ArcadeInput::isControllerConfirmPressed() ||
		ArcadeInput::isControllerPrimaryPressed())
	{
		const Action action = activate(m_selectedItem);
		ArcadeInput::consumePressedState();
		return action;
	}

	return Action::None;
}

void GamePauseMenu::drawButton(sf::RenderTarget& target,
	const sf::FloatRect& bounds,
	const std::string& label,
	sf::Color baseColor,
	bool selected) const
{
	sf::RectangleShape box;
	box.setPosition(bounds.position);
	box.setSize(bounds.size);

	if (selected)
	{
		box.setFillColor(sf::Color(
			static_cast<std::uint8_t>(std::min(255, baseColor.r + 60)),
			static_cast<std::uint8_t>(std::min(255, baseColor.g + 60)),
			static_cast<std::uint8_t>(std::min(255, baseColor.b + 60)),
			255));
		box.setOutlineColor(sf::Color(255, 220, 80));
		box.setOutlineThickness(3.f);
	}
	else
	{
		box.setFillColor(baseColor);
		box.setOutlineColor(sf::Color(230, 230, 230));
		box.setOutlineThickness(2.f);
	}

	target.draw(box);

	sf::Text text(m_font);
	text.setString(label);
	text.setCharacterSize(30);
	text.setFillColor(sf::Color::White);
	text.setOutlineColor(sf::Color::Black);
	text.setOutlineThickness(2.f);

	const sf::FloatRect textBounds = text.getLocalBounds();
	text.setPosition({
		bounds.position.x + (bounds.size.x - textBounds.size.x) * 0.5f - textBounds.position.x,
		bounds.position.y + (bounds.size.y - textBounds.size.y) * 0.5f - textBounds.position.y
		});
	target.draw(text);
}

void GamePauseMenu::drawSettingsButton(sf::RenderTarget& target, bool selected) const
{
	sf::RectangleShape plate;
	plate.setPosition(m_settingsBounds.position);
	plate.setSize(m_settingsBounds.size);
	plate.setFillColor(sf::Color(20, 20, 30, 220));

	if (selected)
	{
		plate.setOutlineColor(sf::Color(255, 220, 80));
		plate.setOutlineThickness(3.f);
	}
	else
	{
		plate.setOutlineColor(sf::Color(220, 220, 220));
		plate.setOutlineThickness(2.f);
	}

	target.draw(plate);

	if (m_hasSettingsIcon)
	{
		const sf::Vector2u textureSize = m_settingsIconTexture.getSize();

		if (textureSize.x > 0 && textureSize.y > 0)
		{
			const float scale = std::min(
				(m_settingsBounds.size.x - 10.f) / static_cast<float>(textureSize.x),
				(m_settingsBounds.size.y - 10.f) / static_cast<float>(textureSize.y)
			);

			sf::Sprite icon(m_settingsIconTexture);
			icon.setScale({ scale, scale });
			icon.setPosition({
				m_settingsBounds.position.x + (m_settingsBounds.size.x - textureSize.x * scale) * 0.5f,
				m_settingsBounds.position.y + (m_settingsBounds.size.y - textureSize.y * scale) * 0.5f
				});
			target.draw(icon);
		}
	}
	else
	{
		sf::Text fallback(m_font);
		fallback.setString("[*]");
		fallback.setCharacterSize(20);
		fallback.setFillColor(sf::Color::White);
		const sf::FloatRect b = fallback.getLocalBounds();
		fallback.setPosition({
			m_settingsBounds.position.x + (m_settingsBounds.size.x - b.size.x) * 0.5f - b.position.x,
			m_settingsBounds.position.y + (m_settingsBounds.size.y - b.size.y) * 0.5f - b.position.y
			});
		target.draw(fallback);
	}
}

void GamePauseMenu::draw(sf::RenderTarget& target) const
{
	if (!m_isOpen)
		return;

	sf::RectangleShape dim;
	dim.setPosition({ 0.f, 0.f });
	dim.setSize({
		static_cast<float>(m_lastLayoutSize.x),
		static_cast<float>(m_lastLayoutSize.y)
		});
	dim.setFillColor(sf::Color(0, 0, 0, 160));
	target.draw(dim);

	sf::RectangleShape panel;
	panel.setPosition(m_panelBounds.position);
	panel.setSize(m_panelBounds.size);
	panel.setFillColor(sf::Color(20, 22, 32, 245));
	panel.setOutlineColor(sf::Color(230, 230, 230));
	panel.setOutlineThickness(3.f);
	target.draw(panel);

	{
		sf::Text title(m_font);
		title.setString("Paused");
		title.setCharacterSize(46);
		title.setFillColor(sf::Color::White);
		title.setOutlineColor(sf::Color::Black);
		title.setOutlineThickness(2.f);

		const sf::FloatRect bounds = title.getLocalBounds();
		title.setPosition({
			m_panelBounds.position.x + (m_panelBounds.size.x - bounds.size.x) * 0.5f - bounds.position.x,
			m_panelBounds.position.y + 40.f - bounds.position.y
			});
		target.draw(title);
	}

	drawButton(target, m_resumeBounds, "RESUME",
		sf::Color(40, 140, 60, 230),
		m_selectedItem == Item::Resume);

	drawButton(target, m_quitBounds, "QUIT",
		sf::Color(170, 50, 50, 230),
		m_selectedItem == Item::Quit);

	drawSettingsButton(target, m_selectedItem == Item::Settings);
}

const std::string& GamePauseMenu::getLastError() const
{
	return m_lastError;
}
