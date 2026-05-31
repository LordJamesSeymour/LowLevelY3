#pragma once

#include <SFML/Graphics.hpp>

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

class GAME1_Level;

// SpikeHead is a 2x2 tile slam trap for SurfersQuest. It floats idle (randomly
// blinking), detects players in a cardinal cross 4 tiles out from its 2x2 body,
// waits briefly, then slams in the locked direction until a solid tile stops it,
// plays the matching directional impact animation, and rests at the new
// position - from where it re-acquires targets. It is registered as
// GAME1_TrapType::SpikeHead for editor/serialization purposes, but its runtime
// lives here (not in GAME1_Trap) because its state machine and 2x2 movement do
// not fit the simple per-tile trap model.
namespace GAME1_SpikeHeadTuning
{
	constexpr float TileSize = 64.f;
	constexpr int FootprintTiles = 2;                         // 2x2 tiles.
	constexpr float BodySize = TileSize * FootprintTiles;     // 128 px square.
	constexpr int DetectionRangeTiles = 4;                    // Cross arm reach.
	constexpr float AttackDelaySeconds = 1.f;                 // Windup before slam.
	constexpr float SlamSpeed = 900.f;                        // px/s while slamming.
	constexpr float BlinkFrameDuration = 0.07f;               // Quick, subtle blink.
	constexpr float HitFrameDuration = 0.06f;                 // Brief impact.
	constexpr float BlinkMinDelay = 1.6f;                     // Random idle blink gap.
	constexpr float BlinkMaxDelay = 4.5f;
	constexpr float PostHitIdleCooldown = 0.35f;              // Settle before re-detecting.
	constexpr int SlamContactDamage = 50;                     // Matches Fire damage.

	// Inset applied to the 2x2 body when producing the player-damage rectangle so
	// merely brushing the very edge of the cell is not punished.
	constexpr float HazardInset = 8.f;
}

enum class GAME1_SpikeHeadDirection
{
	Up,
	Down,
	Left,
	Right
};

// Loads and owns the SpikeHead textures once so instances can share them.
class GAME1_SpikeHeadAssets
{
public:
	bool load(const std::string& resourcesDirectory);

	const sf::Texture* getIdleTexture() const;
	const std::vector<sf::Texture>& getBlinkFrames() const;
	const std::vector<sf::Texture>& getTopHitFrames() const;
	const std::vector<sf::Texture>& getBottomHitFrames() const;
	const std::vector<sf::Texture>& getLeftHitFrames() const;
	const std::vector<sf::Texture>& getRightHitFrames() const;

	const std::string& getLastWarning() const;

private:
	void loadSlices(const std::filesystem::path& directory,
		int firstIndex,
		int lastIndex,
		std::vector<sf::Texture>& outFrames);
	void warn(const std::string& message);

private:
	sf::Texture m_idleTexture;
	bool m_hasIdleTexture = false;

	std::vector<sf::Texture> m_blinkFrames;
	std::vector<sf::Texture> m_topHitFrames;
	std::vector<sf::Texture> m_bottomHitFrames;
	std::vector<sf::Texture> m_leftHitFrames;
	std::vector<sf::Texture> m_rightHitFrames;
	std::vector<sf::Texture> m_emptyFrames;

	std::string m_lastWarning;
};

class GAME1_SpikeHead
{
public:
	GAME1_SpikeHead() = default;
	explicit GAME1_SpikeHead(sf::Vector2i gridPosition);

	void setAssets(const GAME1_SpikeHeadAssets* assets);
	void reset();

	// playerBounds holds the world-space collision rectangles of the active
	// players (1 in solo play, up to 2 in co-op). The closest player inside a
	// cardinal arm is targeted.
	void update(float deltaTime,
		const GAME1_Level& level,
		const std::vector<sf::FloatRect>& playerBounds);

	void draw(sf::RenderTarget& target) const;

	sf::Vector2i getGridPosition() const;
	sf::FloatRect getBodyBounds() const;

	// World-space 2x2 body hitbox (lightly inset). This is the ONLY area that can
	// damage a player - the cross-shaped detection range (armBounds) is used
	// solely to pick a slam direction and never deals damage. Built only from the
	// body's current world position and TileSize, so it is always a small rect
	// around the body, never the detection range and never level-sized. Callers
	// must still intersect it against the player hurtbox before applying damage.
	sf::FloatRect getHazardBounds() const;

private:
	enum class State
	{
		Idle,
		Windup,
		Slam,
		Hit
	};

	sf::FloatRect armBounds(GAME1_SpikeHeadDirection direction) const;
	bool tryAcquireTarget(const std::vector<sf::FloatRect>& playerBounds,
		GAME1_SpikeHeadDirection& outDirection) const;

	void updateIdle(float deltaTime, const std::vector<sf::FloatRect>& playerBounds);
	void updateBlink(float deltaTime);
	void beginWindup(GAME1_SpikeHeadDirection direction);
	void updateWindup(float deltaTime);
	void beginSlam(GAME1_SpikeHeadDirection direction);
	void updateSlam(float deltaTime, const GAME1_Level& level);
	void beginHit();
	void updateHit(float deltaTime);
	void snapToGrid();

	const std::vector<sf::Texture>& currentHitFrames() const;
	const sf::Texture* currentTexture() const;

	bool isSlamBlocked(const GAME1_Level& level, int col, int row) const;

	static bool rectsIntersect(const sf::FloatRect& a, const sf::FloatRect& b);
	static float randomFloat(float minValue, float maxValue);

private:
	sf::Vector2i m_gridPosition{ 0, 0 };          // Current resting top-left cell.
	sf::Vector2i m_originalGridPosition{ 0, 0 };  // Placed position (reset target).
	sf::Vector2f m_position{ 0.f, 0.f };          // World px of the body top-left.

	State m_state = State::Idle;
	GAME1_SpikeHeadDirection m_slamDirection = GAME1_SpikeHeadDirection::Down;
	GAME1_SpikeHeadDirection m_hitDirection = GAME1_SpikeHeadDirection::Down;

	bool m_blinking = false;
	float m_blinkDelayTimer = 0.f;
	float m_idleCooldown = 0.f;
	float m_attackDelayTimer = 0.f;

	float m_animationTimer = 0.f;
	std::size_t m_frameIndex = 0;

	const GAME1_SpikeHeadAssets* m_assets = nullptr;
};
