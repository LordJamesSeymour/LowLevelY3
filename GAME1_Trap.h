#pragma once

#include <SFML/Graphics.hpp>

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace GAME1_TrapTuning
{
	constexpr float TileSize = 64.f;
	constexpr float PlayerMoveSpeed = 330.f;
	constexpr float PlayerJumpForce = 650.f;
	constexpr float PlayerGravity = 1500.f;
	constexpr float FallingPlatformSpeed = PlayerGravity * 0.5f;
	constexpr float FallingPlatformMaxDrop = TileSize * 2.f;
	constexpr float BrownPlatformSpeed = PlayerMoveSpeed * 0.5f;
	constexpr float GreyPlatformSpeed = PlayerMoveSpeed * 1.5f;
	constexpr float TrapAnimationFrameDuration = 0.08f;
	constexpr float FireFrameDuration = 0.09f;
	constexpr int FireReloadLoopCount = 3;
	constexpr int FireActiveLoopCount = 2;
	constexpr int FireDamage = 50;
}

enum class GAME1_TrapType
{
	FallingPlatform,
	Fan,
	Fire,
	Chain,
	BrownMovingPlatform,
	GreyMovingPlatform
};

enum class GAME1_TrapOrientation
{
	Up = 0,
	Right = 1,
	Down = 2,
	Left = 3
};

struct GAME1_TrapSpawn
{
	GAME1_TrapType type = GAME1_TrapType::FallingPlatform;
	sf::Vector2i gridPosition{ 0, 0 };
	GAME1_TrapOrientation orientation = GAME1_TrapOrientation::Up;
};

struct GAME1_TrapPlatformContact
{
	std::size_t trapIndex = 0;
	sf::FloatRect bounds{ { 0.f, 0.f }, { 0.f, 0.f } };
	sf::Vector2f delta{ 0.f, 0.f };
};

const std::vector<GAME1_TrapType>& GAME1_GetAllTrapTypes();
std::string GAME1_GetTrapTypeName(GAME1_TrapType type);
std::string GAME1_GetTrapEditorLabel(GAME1_TrapType type);
sf::Color GAME1_GetTrapFallbackColor(GAME1_TrapType type);
bool GAME1_TryParseTrapType(const std::string& value, GAME1_TrapType& outType);
int GAME1_GetTrapOrientationValue(GAME1_TrapOrientation orientation);
GAME1_TrapOrientation GAME1_NormalizeTrapOrientation(int value);
GAME1_TrapOrientation GAME1_RotateTrapOrientation(GAME1_TrapOrientation orientation, int quarterTurnsClockwise);
sf::Vector2f GAME1_GetTrapOrientationDirection(GAME1_TrapOrientation orientation);
bool GAME1_IsRotatableTrapType(GAME1_TrapType type);
bool GAME1_IsMovingPlatformTrapType(GAME1_TrapType type);
bool GAME1_TryParseTrapObjectLine(const std::string& line, GAME1_TrapSpawn& outSpawn);
std::string GAME1_BuildTrapObjectLine(const GAME1_TrapSpawn& spawn);

class GAME1_TrapAssets
{
public:
	bool load(const std::string& resourcesDirectory);

	const std::vector<sf::Texture>& getFallingPlatformFrames() const;
	const std::vector<sf::Texture>& getFanFrames() const;
	const std::vector<sf::Texture>& getFireReloadFrames() const;
	const std::vector<sf::Texture>& getFireOnFrames() const;
	const std::vector<sf::Texture>& getBrownPlatformFrames() const;
	const std::vector<sf::Texture>& getGreyPlatformFrames() const;

	const sf::Texture* getChainTexture() const;
	const sf::Texture* getEditorIconTexture(GAME1_TrapType type) const;
	const std::string& getLastWarning() const;

private:
	void loadFrames(const std::filesystem::path& directory,
		const std::string& prefix,
		int firstIndex,
		int lastIndex,
		std::vector<sf::Texture>& outFrames);
	void loadSingleTexture(const std::filesystem::path& path,
		sf::Texture& outTexture,
		bool& outLoaded);
	void warn(const std::string& message);

private:
	std::vector<sf::Texture> m_fallingPlatformFrames;
	std::vector<sf::Texture> m_fanFrames;
	std::vector<sf::Texture> m_fireReloadFrames;
	std::vector<sf::Texture> m_fireOnFrames;
	std::vector<sf::Texture> m_brownPlatformFrames;
	std::vector<sf::Texture> m_greyPlatformFrames;
	std::vector<sf::Texture> m_emptyFrames;

	sf::Texture m_chainTexture;
	bool m_hasChainTexture = false;

	std::string m_lastWarning;
};

class GAME1_Trap
{
public:
	GAME1_Trap() = default;
	explicit GAME1_Trap(const GAME1_TrapSpawn& spawn);

	void resetRuntime();
	void configureChainPath(sf::Vector2i startGrid,
		sf::Vector2i endGrid,
		bool horizontal,
		bool enabled);
	void update(float deltaTime);
	void draw(sf::RenderTarget& target, const GAME1_TrapAssets& assets) const;

	GAME1_TrapType getType() const;
	sf::Vector2i getGridPosition() const;
	GAME1_TrapOrientation getOrientation() const;
	sf::Vector2f getPosition() const;
	sf::Vector2f getDelta() const;

	bool isChain() const;
	bool isPlatform() const;
	bool isMovingPlatform() const;
	bool isActiveFire() const;

	void markPlayerStanding();

	std::optional<GAME1_TrapPlatformContact> getPlatformContact(
		std::size_t trapIndex,
		const sf::FloatRect& playerBounds,
		float previousPlayerBottom) const;

	std::optional<sf::FloatRect> getFanForceBounds() const;
	std::optional<sf::FloatRect> getFireDamageBounds() const;
	sf::Vector2f getFanDirection() const;

private:
	enum class FirePhase
	{
		Reload,
		Active
	};

private:
	void updateLoopingAnimation(float deltaTime, std::size_t frameCount);
	void updateFireAnimation(float deltaTime);

	float getMovingPlatformSpeed() const;
	sf::FloatRect getTileBounds() const;
	sf::FloatRect getPlatformBounds() const;
	sf::FloatRect getFireActiveBounds() const;

	void drawTextureFitted(sf::RenderTarget& target,
		const sf::Texture& texture,
		const sf::FloatRect& bounds,
		GAME1_TrapOrientation orientation) const;
	void drawFallback(sf::RenderTarget& target,
		const sf::FloatRect& bounds,
		sf::Color color) const;

	static bool rectsIntersect(const sf::FloatRect& a, const sf::FloatRect& b);
	static float moveTowards(float current, float target, float maxDelta);

private:
	GAME1_TrapType m_type = GAME1_TrapType::FallingPlatform;
	sf::Vector2i m_gridPosition{ 0, 0 };
	GAME1_TrapOrientation m_orientation = GAME1_TrapOrientation::Up;

	sf::Vector2f m_originalPosition{ 0.f, 0.f };
	sf::Vector2f m_position{ 0.f, 0.f };
	sf::Vector2f m_previousPosition{ 0.f, 0.f };
	sf::Vector2f m_delta{ 0.f, 0.f };

	bool m_playerStandingThisFrame = false;

	bool m_chainPathEnabled = false;
	bool m_chainPathHorizontal = true;
	sf::Vector2i m_chainPathStart{ 0, 0 };
	sf::Vector2i m_chainPathEnd{ 0, 0 };
	float m_chainDirection = 1.f;

	float m_animationTimer = 0.f;
	std::size_t m_frameIndex = 0;

	FirePhase m_firePhase = FirePhase::Reload;
	int m_fireLoopsCompleted = 0;
};
