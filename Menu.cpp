#include "Menu.h"

bool Menu::load(const std::string& closeButtonPath,
    const std::string& logoPath,
    const std::string& playPath,
    const std::string& levelEditorPath)
{
    m_lastError.clear();

    if (!m_closeTexture.loadFromFile(closeButtonPath))
    {
        m_lastError = "Failed to load close button texture: " + closeButtonPath;
        return false;
    }

    if (!m_logoTexture.loadFromFile(logoPath))
    {
        m_lastError = "Failed to load logo texture: " + logoPath;
        return false;
    }

    if (!m_playTexture.loadFromFile(playPath))
    {
        m_lastError = "Failed to load play button texture: " + playPath;
        return false;
    }

    if (!m_levelEditorTexture.loadFromFile(levelEditorPath))
    {
        m_lastError = "Failed to load level editor texture: " + levelEditorPath;
        return false;
    }

    m_closeSprite.emplace(m_closeTexture);
    m_logoSprite.emplace(m_logoTexture);
    m_playSprite.emplace(m_playTexture);
    m_levelEditorSprite.emplace(m_levelEditorTexture);

    return true;
}

void Menu::layout(const sf::RenderWindow& window)
{
    if (!m_closeSprite || !m_logoSprite || !m_playSprite || !m_levelEditorSprite)
        return;

    const sf::Vector2u windowSize = window.getSize();
    const float windowWidth = static_cast<float>(windowSize.x);
    const float windowHeight = static_cast<float>(windowSize.y);

    scaleToSize(*m_closeSprite, 48.f, 48.f);
    scaleToWidth(*m_logoSprite, 420.f);
    scaleToWidth(*m_playSprite, 240.f);
    scaleToWidth(*m_levelEditorSprite, 240.f);

    const sf::FloatRect logoBounds = m_logoSprite->getGlobalBounds();
    const sf::FloatRect playBounds = m_playSprite->getGlobalBounds();
    const sf::FloatRect editorBounds = m_levelEditorSprite->getGlobalBounds();
    const sf::FloatRect closeBounds = m_closeSprite->getGlobalBounds();

    const float spacingAfterLogo = 35.f;
    const float spacingBetweenButtons = 20.f;

    const float totalHeight =
        logoBounds.size.y +
        spacingAfterLogo +
        playBounds.size.y +
        spacingBetweenButtons +
        editorBounds.size.y;

    const float startY = (windowHeight - totalHeight) * 0.5f;

    m_logoSprite->setPosition({
        (windowWidth - logoBounds.size.x) * 0.5f,
        startY
        });

    m_playSprite->setPosition({
        (windowWidth - playBounds.size.x) * 0.5f,
        startY + logoBounds.size.y + spacingAfterLogo
        });

    m_levelEditorSprite->setPosition({
        (windowWidth - editorBounds.size.x) * 0.5f,
        m_playSprite->getPosition().y + playBounds.size.y + spacingBetweenButtons
        });

    m_closeSprite->setPosition({
        windowWidth - closeBounds.size.x - 20.f,
        20.f
        });
}

MenuAction Menu::handleClick(sf::Vector2f mousePosition) const
{
    if (!m_closeSprite || !m_playSprite || !m_levelEditorSprite)
        return MenuAction::None;

    if (containsPoint(*m_closeSprite, mousePosition))
        return MenuAction::Quit;

    if (containsPoint(*m_playSprite, mousePosition))
        return MenuAction::Play;

    if (containsPoint(*m_levelEditorSprite, mousePosition))
        return MenuAction::LevelEditor;

    return MenuAction::None;
}

void Menu::draw(sf::RenderWindow& window) const
{
    if (m_logoSprite) window.draw(*m_logoSprite);
    if (m_playSprite) window.draw(*m_playSprite);
    if (m_levelEditorSprite) window.draw(*m_levelEditorSprite);
    if (m_closeSprite) window.draw(*m_closeSprite);
}

const std::string& Menu::getLastError() const
{
    return m_lastError;
}

void Menu::scaleToWidth(sf::Sprite& sprite, float targetWidth)
{
    const sf::FloatRect localBounds = sprite.getLocalBounds();

    if (localBounds.size.x <= 0.f || localBounds.size.y <= 0.f)
        return;

    const float scale = targetWidth / localBounds.size.x;
    sprite.setScale({ scale, scale });
}

void Menu::scaleToSize(sf::Sprite& sprite, float targetWidth, float targetHeight)
{
    const sf::FloatRect localBounds = sprite.getLocalBounds();

    if (localBounds.size.x <= 0.f || localBounds.size.y <= 0.f)
        return;

    sprite.setScale({
        targetWidth / localBounds.size.x,
        targetHeight / localBounds.size.y
        });
}

bool Menu::containsPoint(const sf::Sprite& sprite, sf::Vector2f point)
{
    const sf::FloatRect bounds = sprite.getGlobalBounds();

    return point.x >= bounds.position.x &&
        point.x <= bounds.position.x + bounds.size.x &&
        point.y >= bounds.position.y &&
        point.y <= bounds.position.y + bounds.size.y;
}