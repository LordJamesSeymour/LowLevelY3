#include "GAME2_BackgroundLayer.h"

bool GAME2_BackgroundLayer::load(const std::string& texturePath, float scrollSpeed, sf::Vector2u viewSize)
{
	m_lastError.clear();

	if (!m_texture.loadFromFile(texturePath))
	{
		m_lastError = "Failed to load background layer texture: " + texturePath;
		return false;
	}

	m_scrollSpeed = scrollSpeed;

	m_spriteA.emplace(m_texture);
	m_spriteB.emplace(m_texture);

	setViewSize(viewSize);
	reset();

	return true;
}

void GAME2_BackgroundLayer::setViewSize(sf::Vector2u viewSize)
{
	if (!m_spriteA || !m_spriteB)
		return;

	if (viewSize.x == 0 || viewSize.y == 0)
		return;

	// Do nothing if the size did not actually change.
	// This avoids resetting the parallax every frame.
	if (m_viewSize == viewSize && m_scaledHeight > 0.f)
		return;

	m_viewSize = viewSize;

	const sf::FloatRect localBounds = m_spriteA->getLocalBounds();
	if (localBounds.size.x <= 0.f || localBounds.size.y <= 0.f)
		return;

	// Preserve aspect ratio.
	// Scale based on height only, and center horizontally.
	const float uniformScale = static_cast<float>(viewSize.y) / localBounds.size.y;

	m_spriteA->setScale({ uniformScale, uniformScale });
	m_spriteB->setScale({ uniformScale, uniformScale });

	m_scaledWidth = localBounds.size.x * uniformScale;
	m_scaledHeight = localBounds.size.y * uniformScale;

	m_drawX = (static_cast<float>(viewSize.x) - m_scaledWidth) * 0.5f;

	reset();
}

void GAME2_BackgroundLayer::reset()
{
	if (!m_spriteA || !m_spriteB)
		return;

	m_spriteA->setPosition({ m_drawX, 0.f });
	m_spriteB->setPosition({ m_drawX, -m_scaledHeight });
}

void GAME2_BackgroundLayer::update(float deltaTime)
{
	if (!m_spriteA || !m_spriteB)
		return;

	const float movement = m_scrollSpeed * deltaTime;

	m_spriteA->move({ 0.f, movement });
	m_spriteB->move({ 0.f, movement });

	if (m_spriteA->getPosition().y >= m_scaledHeight)
	{
		m_spriteA->setPosition({
			m_drawX,
			m_spriteB->getPosition().y - m_scaledHeight
			});
	}

	if (m_spriteB->getPosition().y >= m_scaledHeight)
	{
		m_spriteB->setPosition({
			m_drawX,
			m_spriteA->getPosition().y - m_scaledHeight
			});
	}
}

void GAME2_BackgroundLayer::draw(sf::RenderWindow& window) const
{
	if (m_spriteA)
		window.draw(*m_spriteA);

	if (m_spriteB)
		window.draw(*m_spriteB);
}

sf::FloatRect GAME2_BackgroundLayer::getContentBounds() const
{
	return sf::FloatRect(
		{ m_drawX, 0.f },
		{ m_scaledWidth, static_cast<float>(m_viewSize.y) }
	);
}

const std::string& GAME2_BackgroundLayer::getLastError() const
{
	return m_lastError;
}