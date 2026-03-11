#include "GAME2_Menu.h"

bool GAME2_Menu::load(const std::string& logoPath,
	const std::string& playPath)
{
	m_lastError.clear();

	if (!m_logoTexture.loadFromFile(logoPath))
	{
		m_lastError = "Failed to load Game 2 logo texture: " + logoPath;
		return false;
	}

	if (!m_playTexture.loadFromFile(playPath))
	{
		m_lastError = "Failed to load Game 2 play button texture: " + playPath;
		return false;
	}

	m_logoSprite.emplace(m_logoTexture);
	m_playSprite.emplace(m_playTexture);

	return true;
}

void GAME2_Menu::layout(const sf::RenderWindow& window)
{
	if (!m_logoSprite || !m_playSprite)
		return;

	const float windowWidth = static_cast<float>(window.getSize().x);
	const float windowHeight = static_cast<float>(window.getSize().y);

	// Keep the logo and play button consistently sized.
	scaleToWidth(*m_logoSprite, 420.f);
	scaleToWidth(*m_playSprite, 240.f);

	const sf::FloatRect logoBounds = m_logoSprite->getGlobalBounds();
	const sf::FloatRect playBounds = m_playSprite->getGlobalBounds();

	const float spacing = 36.f;
	const float totalHeight = logoBounds.size.y + spacing + playBounds.size.y;
	const float startY = (windowHeight - totalHeight) * 0.5f;

	m_logoSprite->setPosition({
		(windowWidth - logoBounds.size.x) * 0.5f,
		startY
		});

	m_playSprite->setPosition({
		(windowWidth - playBounds.size.x) * 0.5f,
		startY + logoBounds.size.y + spacing
		});
}

GAME2_MenuAction GAME2_Menu::handleClick(sf::Vector2f mousePosition) const
{
	if (!m_playSprite)
		return GAME2_MenuAction::None;

	if (containsPoint(m_playSprite->getGlobalBounds(), mousePosition))
		return GAME2_MenuAction::Play;

	return GAME2_MenuAction::None;
}

void GAME2_Menu::draw(sf::RenderWindow& window) const
{
	if (m_logoSprite)
		window.draw(*m_logoSprite);

	if (m_playSprite)
		window.draw(*m_playSprite);
}

const std::string& GAME2_Menu::getLastError() const
{
	return m_lastError;
}

void GAME2_Menu::scaleToWidth(sf::Sprite& sprite, float targetWidth)
{
	const sf::FloatRect localBounds = sprite.getLocalBounds();

	if (localBounds.size.x <= 0.f || localBounds.size.y <= 0.f)
		return;

	const float scale = targetWidth / localBounds.size.x;
	sprite.setScale({ scale, scale });
}

bool GAME2_Menu::containsPoint(const sf::FloatRect& bounds, sf::Vector2f point)
{
	return point.x >= bounds.position.x &&
		point.x <= bounds.position.x + bounds.size.x &&
		point.y >= bounds.position.y &&
		point.y <= bounds.position.y + bounds.size.y;
}