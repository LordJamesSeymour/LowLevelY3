#pragma once

#include <SFML/Graphics.hpp>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

class GAME1_Level;

// Surfers Quest local co-op input binding.
// singlePlayer = true (default) uses the original keyboard + all joystick aggregate.
// When false, source and joystickIndex identify the exact device this player is locked to.
enum class GAME1_InputSource { Keyboard, Joystick };

struct GAME1_PlayerBinding
{
	bool singlePlayer = true;
	GAME1_InputSource source = GAME1_InputSource::Keyboard;
	unsigned int joystickIndex = 0;
};

class GAME1_Player
{
public:
	bool load(const std::string& playerIdleDirectory, sf::Vector2f startPosition);

	void update(float deltaTime, GAME1_Level& level);
	void draw(sf::RenderTarget& target) const;

	void resetGame();
	void stopMusic();

	sf::FloatRect getBounds() const;
	sf::Vector2f getPosition() const;

	void setSpawnPosition(sf::Vector2f spawnPosition);
	sf::Vector2f getSpawnPosition() const;

	void setPosition(sf::Vector2f position);

	bool isRespawning() const;
	int getRespawnCountdown() const;

	int getHealth() const;
	int getMaxHealth() const;

	sf::Vector2f getVelocity() const;
	void bounceAfterEnemyStomp();
	void takeEnemyDamage(const sf::FloatRect& enemyBounds, int damage);
	int getLives() const;
	int getMaxLives() const;
	bool isGameOver() const;

	// Returns true when the player is still part of the active gameplay (not
	// permanently out, and not mid-respawn).  Useful for camera/enemy logic in
	// co-op.
	bool isActive() const;

	// Co-op support.
	void setBinding(GAME1_PlayerBinding binding);
	GAME1_PlayerBinding getBinding() const;

	// When co-op mode is active, the player no longer manages global concerns
	// like music start/stop or the game-over restart prompt — those are owned
	// by main.cpp so they apply to the team as a whole.
	void setCoopMode(bool coopActive);

	// When false, the player no longer draws the on-screen Health/Lives HUD
	// (or the OUT OF LIVES popup).  main.cpp draws a combined HUD instead.
	void setDrawHud(bool drawHud);

	// Used by main.cpp to respawn the trailing player near the leading player
	// after an off-screen kill.  Consumed by the next respawn (both at
	// startRespawn and at the end of the respawn timer), then cleared.
	void setNextRespawnPosition(sf::Vector2f position);

	// Forces a death (loses one life, then respawns or triggers game-over for
	// this player only).  Combine with setNextRespawnPosition when the death
	// is caused by lagging off-screen behind the camera.
	void forceDeath();

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
		Fall,
		WallGrab,
		Hit
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
	void performWallJump();
	void performDoubleJump();

	void moveHorizontal(float deltaTime, GAME1_Level& level);
	void moveVertical(float deltaTime, GAME1_Level& level);

	void updateWallGrabState(GAME1_Level& level);
	bool detectWallContact(GAME1_Level& level, bool& touchingLeft, bool& touchingRight) const;

	void applyFanForces(float deltaTime, GAME1_Level& level);
	void checkSpikeTrapCollisions(GAME1_Level& level);
	void checkFireTrapCollisions(GAME1_Level& level);
	void takeSpikeDamage(const sf::FloatRect& spikeBounds);
	void takeTrapDamage(const sf::FloatRect& trapBounds, int damage);
	void updateMovementSounds(float deltaTime, const GAME1_Level& level);
	void resetMovementSoundTimers();
	void playDamageOrDeathSoundForDamage(int damage) const;
	bool wouldDamageCauseFinalDeath(int damage) const;
	void startHitAnimation();
	void applyKnockbackFromTile(const sf::FloatRect& tileBounds);

	void updateAnimation(float deltaTime);
	void updateAnimationState();
	void setAnimationState(AnimationState newState);

	bool loadAnimationFramesFromDirectory(AnimationSet& animation,
		const std::string& directoryPath,
		const std::string& readableName);

	void loseLifeAndRespawn();
	void beginGameOver();
	void startRespawn();
	void updateRespawn(float deltaTime);

	const sf::Texture* getCurrentTexture() const;
	const AnimationSet& getCurrentAnimationSet() const;

	bool isWithinApexGravityWindow() const;
	bool isDropThroughHeld() const;

	// Input-source aware queries (driven by m_inputProfile).
	bool inputMoveLeftHeld() const;
	bool inputMoveRightHeld() const;
	bool inputJumpHeld() const;
	bool inputDropThroughHeld() const;

	// Consumes m_pendingRespawnPosition if set, otherwise returns m_spawnPosition.
	sf::Vector2f resolveRespawnPosition();

	static bool rectsIntersect(const sf::FloatRect& a, const sf::FloatRect& b);
	static float moveTowards(float current, float target, float maxDelta);

private:
	AnimationSet m_idleAnimation;
	AnimationSet m_runAnimation;
	AnimationSet m_jumpAnimation;
	AnimationSet m_doubleJumpAnimation;
	AnimationSet m_fallAnimation;
	AnimationSet m_wallGrabAnimation;
	AnimationSet m_hitAnimation;

	sf::Vector2f m_position{ 100.f, 100.f };
	sf::Vector2f m_previousPosition{ 100.f, 100.f };
	sf::Vector2f m_spawnPosition{ 100.f, 100.f };
	sf::Vector2f m_velocity{ 0.f, 0.f };

	float m_moveSpeed = 330.f;
	float m_jumpSpeed = 650.f;
	float m_gravity = 1500.f;

	float m_momentumBuildTime = 0.35f;
	float m_frictionStopTime = 0.55f;

	float m_apexGravityTimeWindow = 0.15f;
	float m_apexGravityMultiplier = 0.65f;

	float m_wallGrabBaseFallSpeed = 700.f;
	float m_wallGrabFallSpeedMultiplier = 0.25f;

	float m_wallJumpHorizontalSpeed = 520.f;
	float m_wallJumpNudgeDistance = 5.f;
	float m_wallJumpControlLockTimer = 0.f;
	float m_wallJumpControlLockDuration = 0.16f;

	bool m_wallGrabActive = false;
	bool m_touchingWallLeft = false;
	bool m_touchingWallRight = false;

	float m_dropThroughTimer = 0.f;
	float m_dropThroughDuration = 0.22f;
	bool m_groundedOnOneWayPlatform = false;

	int m_maxHealth = 100;
	int m_health = 100;

	int m_maxLives = 3;
	int m_lives = 3;
	bool m_gameOver = false;
	int m_spikeDamage = 25;
	float m_damageCooldownTimer = 0.f;
	float m_damageCooldownDuration = 0.75f;
	float m_spikeKnockbackHorizontal = 420.f;
	float m_spikeKnockbackVertical = 520.f;

	bool m_hitAnimationPlaying = false;

	bool m_onGround = false;
	bool m_jumpHeldLastFrame = false;
	bool m_phaseHeldLastFrame = false;

	float m_coyoteTimer = 0.f;
	float m_coyoteTime = 0.30f;

	float m_jumpBufferTimer = 0.f;
	float m_jumpBufferTime = 0.50f;

	bool m_canDoubleJump = true;
	bool m_doubleJumpAnimationPlaying = false;

	bool m_variableJumpActive = false;

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

	float m_footstepTimer = 0.f;
	float m_footstepInterval = 0.24f;
	float m_wallgrabSoundTimer = 0.f;
	float m_wallgrabSoundInterval = 0.18f;

	sf::Font m_uiFont;
	bool m_hasUiFont = false;

	GAME1_PlayerBinding m_binding{};
	bool m_coopMode = false;
	bool m_drawHud = true;

	std::optional<sf::Vector2f> m_pendingRespawnPosition;

	std::string m_lastError;
};
