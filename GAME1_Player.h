#pragma once

#include <SFML/Graphics.hpp>
#include <cstddef>
#include <string>
#include <vector>

class GAME1_Level;

class GAME1_Player
{
public:
	bool load(const std::string& playerIdleDirectory, sf::Vector2f startPosition);

	void update(float deltaTime, GAME1_Level& level);
	void draw(sf::RenderTarget& target) const;

	sf::FloatRect getBounds() const;

	bool isRespawning() const;
	int getRespawnCountdown() const;

	const std::string& getLastError() const;

private:
	enum class FacingDirection
	{
		Right,
		Left
	};

	enum class AnimationState
	{
		Idle,
		Run,
		Jump,
		DoubleJump,
		Fall
	};

	struct AnimationSet
	{
		std::vector<sf::Texture> frames;
		float frameDuration = 0.035f;
	};

private:
	void handleInput(float deltaTime);
	void applyGravity(float deltaTime);

	void performGroundJump();
	void performDoubleJump();

	void moveHorizontal(float deltaTime, GAME1_Level& level);
	void moveVertical(float deltaTime, GAME1_Level& level);

	void updateAnimation(float deltaTime);
	void updateAnimationState();
	void setAnimationState(AnimationState newState);

	bool loadAnimationFramesFromDirectory(AnimationSet& animation,
		const std::string& directoryPath,
		const std::string& readableName);

	void startRespawn();
	void updateRespawn(float deltaTime);

	const sf::Texture* getCurrentTexture() const;
	const AnimationSet& getCurrentAnimationSet() const;

	bool isWithinApexGravityWindow() const;

	static float moveTowards(float current, float target, float maxDelta);

private:
	AnimationSet m_idleAnimation;
	AnimationSet m_runAnimation;
	AnimationSet m_jumpAnimation;
	AnimationSet m_doubleJumpAnimation;
	AnimationSet m_fallAnimation;

	sf::Vector2f m_position{ 100.f, 100.f };
	sf::Vector2f m_spawnPosition{ 100.f, 100.f };
	sf::Vector2f m_velocity{ 0.f, 0.f };

	// 1.5x faster than the older 220 value.
	float m_moveSpeed = 330.f;

	// Current tuned jump strength.
	float m_jumpSpeed = 650.f;

	float m_gravity = 1500.f;

	// Slower acceleration so the build-up is actually visible.
	float m_momentumBuildTime = 0.35f;

	// Longer stop time so releasing input gives a visible slide.
	float m_frictionStopTime = 0.55f;

	// Apex modifier:
	// For roughly 0.15s before the apex and 0.15s after the apex,
	// gravity is reduced slightly so the player gets more air control
	// without receiving extra horizontal speed.
	float m_apexGravityTimeWindow = 0.15f;
	float m_apexGravityMultiplier = 0.65f;

	bool m_onGround = false;
	bool m_jumpHeldLastFrame = false;

	float m_coyoteTimer = 0.f;
	float m_coyoteTime = 0.30f;

	float m_jumpBufferTimer = 0.f;
	float m_jumpBufferTime = 0.50f;

	bool m_canDoubleJump = true;
	bool m_doubleJumpAnimationPlaying = false;

	// Variable jump only applies to the normal jump, never the double jump.
	bool m_variableJumpActive = false;

	// Instead of killing velocity on jump release, we increase gravity while rising.
	bool m_releasedJumpGravityActive = false;
	float m_releasedJumpGravityMultiplier = 4.0f;

	FacingDirection m_facingDirection = FacingDirection::Right;

	AnimationState m_animationState = AnimationState::Idle;

	bool m_horizontalInputHeld = false;

	float m_animationTimer = 0.f;
	std::size_t m_currentFrameIndex = 0;

	bool m_respawning = false;
	float m_respawnTimer = 0.f;
	float m_respawnDuration = 2.0f;

	float m_drawWidth = 48.f;
	float m_drawHeight = 48.f;

	std::string m_lastError;
};