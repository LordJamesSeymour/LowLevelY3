#pragma once

#include <SFML/Graphics.hpp>
#include <optional>
#include <string>

class GAME1_Level;

// Player logic for Game 1.
// Handles movement, jumping, double jump window, coyote time,
// jump buffering, collision, falling death, and timed respawn.
class GAME1_Player
{
public:
	bool load(const std::string& texturePath, sf::Vector2f startPosition);
	void update(float deltaTime, GAME1_Level& level);
	void draw(sf::RenderWindow& window) const;

	bool isRespawning() const;
	int getRespawnCountdown() const;

	sf::FloatRect getBounds() const;
	const std::string& getLastError() const;

private:
	void handleInput();
	bool tryConsumeJumpBuffer();
	void startPrimaryJump();
	void startDoubleJump();

	void moveHorizontal(float deltaTime, GAME1_Level& level);
	void moveVertical(float deltaTime, GAME1_Level& level);
	void updateBreakBlockTimer(GAME1_Level& level, float deltaTime);

	void beginRespawn();
	void finishRespawn();

private:
	sf::Texture m_texture;
	std::optional<sf::Sprite> m_sprite;

	sf::Vector2f m_spawnPosition{ 0.f, 0.f };
	sf::Vector2f m_velocity{ 0.f, 0.f };

	float m_moveSpeed = 360.f;
	float m_jumpSpeed = 650.f;
	float m_gravity = 1400.f;

	bool m_onGround = false;
	bool m_jumpHeldLastFrame = false;

	float m_coyoteTime = 0.30f;
	float m_coyoteTimer = 0.f;

	float m_jumpBufferTime = 0.20f;
	float m_jumpBufferTimer = 0.f;

	bool m_primaryJumpStarted = false;
	bool m_usedDoubleJump = false;
	float m_primaryJumpTimer = 0.f;
	float m_doubleJumpWindow = 0.65f;

	bool m_isRespawning = false;
	float m_respawnDuration = 3.f;
	float m_respawnTimer = 0.f;

	int m_standingBreakCol = -1;
	int m_standingBreakRow = -1;
	float m_breakStandTimer = 0.f;

	std::string m_lastError;
};