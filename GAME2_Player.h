#pragma once

#include <SFML/Graphics.hpp>
#include <array>
#include <optional>
#include <string>
#include <vector>

class GAME2_Bullet;

// Game 2 player ship.
// Handles:
// - WASD movement
// - diagonal movement
// - staying inside the gameplay area
// - forward / left / right animation logic
// - left/right transition animations
// - lives
// - temporary invulnerability after taking damage
// - player shooting
class GAME2_Player
{
public:
	bool load(const std::string& playerDirectory, const sf::FloatRect& playBounds);
	void reset(const sf::FloatRect& playBounds);

	void update(float deltaTime, const sf::FloatRect& playBounds);
	void updateShooting(float deltaTime, std::vector<GAME2_Bullet>& bullets);

	void draw(sf::RenderWindow& window) const;

	sf::FloatRect getBounds() const;
	sf::FloatRect getCollisionBounds() const;

	void takeHit();
	int getLives() const;
	bool isGameOver() const;

	const std::string& getLastError() const;

private:
	enum class HorizontalIntent
	{
		Neutral,
		Left,
		Right
	};

	enum class AnimationState
	{
		ForwardLoop,
		ToLeft,
		LeftLoop,
		FromLeft,
		ToRight,
		RightLoop,
		FromRight
	};

private:
	bool loadAnimationSet(const std::string& directory);
	void setAnimationState(AnimationState newState);
	void advanceAnimationState();
	void applyCurrentTexture();
	const sf::Texture& getTextureForCurrentStateFrame() const;

private:
	std::array<sf::Texture, 2> m_forwardFrames;
	std::array<sf::Texture, 2> m_transitionLeftFrames;   // PlayerRL-1/2
	std::array<sf::Texture, 2> m_leftFrames;             // PlayerLeft-1/2
	std::array<sf::Texture, 2> m_transitionRightFrames;  // PlayerRR-1/2
	std::array<sf::Texture, 2> m_rightFrames;            // PlayerRight-1/2

	std::optional<sf::Sprite> m_sprite;

	sf::Vector2f m_position{ 0.f, 0.f };
	float m_moveSpeed = 360.f;

	AnimationState m_animationState = AnimationState::ForwardLoop;
	std::size_t m_frameIndex = 0;
	float m_frameTimer = 0.f;
	float m_frameDuration = 0.12f;

	// Combat / health
	int m_lives = 3;
	float m_shotCooldown = 0.f;
	float m_shotInterval = 0.20f;
	float m_invulnerabilityTimer = 0.f;
	float m_invulnerabilityDuration = 0.85f;

	std::string m_lastError;
};