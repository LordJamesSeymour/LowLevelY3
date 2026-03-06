#pragma once

#include <SFML/Graphics.hpp>
#include <optional>
#include <string>

class Level;

class Player
{
public:
    bool load(const std::string& texturePath, sf::Vector2f startPosition);
    void update(float deltaTime, Level& level, unsigned int windowWidth);
    void draw(sf::RenderWindow& window) const;

    sf::FloatRect getBounds() const;
    const std::string& getLastError() const;

private:
    void handleInput(float deltaTime);
    void moveHorizontal(float deltaTime, Level& level, unsigned int windowWidth);
    void moveVertical(float deltaTime, Level& level);
    void updateBreakBlockTimer(Level& level, float deltaTime);

private:
    sf::Texture m_texture;
    std::optional<sf::Sprite> m_sprite;

    sf::Vector2f m_velocity{ 0.f, 0.f };

    float m_moveSpeed = 360.f;
    float m_jumpSpeed = 650.f;
    float m_gravity = 1400.f;

    bool m_onGround = false;
    bool m_jumpHeldLastFrame = false;

    float m_coyoteTime = 0.12f;
    float m_coyoteTimer = 0.f;

    float m_jumpBufferTime = 0.12f;
    float m_jumpBufferTimer = 0.f;

    int m_standingBreakCol = -1;
    int m_standingBreakRow = -1;
    float m_breakStandTimer = 0.f;

    std::string m_lastError;
};