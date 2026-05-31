#pragma once

#include <SFML/Graphics.hpp>

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

// Saw is a 2x2 tile moving hazard for SurfersQuest. It rides a chain segment,
// sliding back and forth between the two ends forever while its blade
// animation loops, and it deals contact damage to any player it touches. It is
// registered as GAME1_TrapType::Saw so the editor/level format treat it as a
// normal trap object placed on top of a chain tile ("OBJECT Saw col row 0"),
// but - like GAME1_SpikeHead - its 2x2 runtime lives here rather than in the
// single-tile GAME1_Trap model. The chain tile underneath is never replaced; it
// stays as the visible track.
namespace GAME1_SawTuning
{
	constexpr float TileSize = 64.f;
	constexpr int FootprintTiles = 2;                       // 2x2 tiles.
	constexpr float BodySize = TileSize * FootprintTiles;   // 128 px square.
	constexpr float MoveSpeed = 260.f;                      // px/s along the chain.
	constexpr float FrameDuration = 0.06f;                  // Spinning blade loop.
	constexpr int ContactDamage = 50;
	// Inset applied to the 2x2 body when producing the player-damage rectangle so
	// clipping the rounded blade's empty corners is not punished.
	constexpr float HazardInset = 12.f;
}

// Loads and owns the 8 looping Saw frames once so instances can share them.
class GAME1_SawAssets
{
public:
	bool load(const std::string& resourcesDirectory);

	const std::vector<sf::Texture>& getFrames() const;
	const sf::Texture* getEditorIcon() const;

	const std::string& getLastWarning() const;

private:
	void warn(const std::string& message);

private:
	std::vector<sf::Texture> m_frames;
	std::string m_lastWarning;
};

class GAME1_Saw
{
public:
	GAME1_Saw() = default;
	explicit GAME1_Saw(sf::Vector2i anchorGridPosition);

	void setAssets(const GAME1_SawAssets* assets);
	void reset();

	// Defines the chain track the saw rides. The 2x2 body is centred on the
	// anchor tile and slides so its centre rides each end tile's centre, clamped
	// to stay inside the level. startGrid == endGrid leaves the saw stationary.
	void configureTrack(sf::Vector2i startGrid,
		sf::Vector2i endGrid,
		bool horizontal,
		int levelWidthTiles,
		int levelHeightTiles);

	void update(float deltaTime);
	void draw(sf::RenderTarget& target) const;

	sf::Vector2i getAnchorGridPosition() const;
	sf::FloatRect getBodyBounds() const;

	// World-space 2x2 contact hitbox (lightly inset). Built only from the saw's
	// current world position and TileSize, so it is always a small rect around
	// the saw - never level-sized, never at a stale origin. Callers must still
	// intersect it against the player hurtbox before applying damage.
	sf::FloatRect getHazardBounds() const;

private:
	const sf::Texture* currentTexture() const;
	static float clampValue(float value, float low, float high);

private:
	sf::Vector2i m_anchorGrid{ 0, 0 };       // Chain tile the saw was placed on.
	sf::Vector2f m_position{ 0.f, 0.f };      // World px of the 2x2 body top-left.

	float m_minX = 0.f;                       // Travel range for m_position.
	float m_maxX = 0.f;
	float m_minY = 0.f;
	float m_maxY = 0.f;
	bool m_trackHorizontal = true;
	float m_direction = 1.f;

	float m_animationTimer = 0.f;
	std::size_t m_frameIndex = 0;

	const GAME1_SawAssets* m_assets = nullptr;
};
