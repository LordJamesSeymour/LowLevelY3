#pragma once

#include "BombermanBomb.h"
#include "BombermanEnemy.h"
#include "BombermanLevel.h"
#include "BombermanPlayer.h"
#include "BombermanTypes.h"

#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <random>
#include <string>
#include <vector>

class GAME1_BombermanWindow
{
public:
	bool load(const std::string& fontPath, const std::string& bombermanRootDirectory);

	bool loadMapFromFile(const std::string& mapPath);

	void reset();

	void update(float deltaTime, sf::Vector2u windowSize);
	void layout(const sf::RenderWindow& window);
	void draw(sf::RenderWindow& window) const;

	const std::string& getLastError() const;

private:
	enum class PlayState
	{
		Playing,
		TeleportingOut,
		TeleportingIn,
		GameOver,
		Victory
	};

	enum class PowerUpType
	{
		FireUp,
		BombUp,
		SpeedUp
	};

	struct AnimationFrames
	{
		std::vector<sf::Texture> frames;
	};

	struct ActiveExplosion
	{
		std::vector<BombermanExplosionTile> tiles;
		float timer = 0.38f;
		float duration = 0.38f;
	};

	struct ActivePowerUp
	{
		PowerUpType type = PowerUpType::FireUp;
		BombermanGridPosition gridPosition{ 0, 0 };
		bool collected = false;
	};

	struct PowerUpTexture
	{
		sf::Texture texture;
		bool loaded = false;
	};

private:
	bool loadAnimationFramesFromDirectory(AnimationFrames& animation,
		const std::string& directoryPath,
		const std::string& readableName);

	bool loadSound(sf::SoundBuffer& buffer,
		std::optional<sf::Sound>& sound,
		const std::string& path,
		const std::string& readableName);

	void playSound(std::optional<sf::Sound>& sound);
	void updateWalkingSound(float deltaTime);
	void stopWalkingSound();
	bool isWalkingInputHeld() const;

	bool tryLoadPowerUpTexture(PowerUpTexture& target, const std::vector<std::string>& candidatePaths);

	bool loadLevelAndActors(bool resetPerks = true);
	void resetPerksToDefaults();

	bool assignHiddenExitToBreakableBlock();
	bool isHiddenExitBlock(BombermanGridPosition gridPosition) const;
	void revealHiddenExitAt(BombermanGridPosition gridPosition);

	void processCompletedBrokenBlocks();
	void handleCompletedBrokenBlock(BombermanGridPosition gridPosition);

	void buildBombAnimationSequence();
	void updateBombAnimation(float deltaTime);
	const sf::Texture* getCurrentBombTexture() const;

	std::size_t getExplosionFrameIndex(const ActiveExplosion& explosion,
		const AnimationFrames& animation) const;

	const AnimationFrames& getExplosionAnimationForTile(BombermanExplosionTileType type) const;

	void placeBomb();
	bool isBombAtTile(BombermanGridPosition gridPosition) const;

	bool isTileBlockedForPlayer(int col, int row) const;
	bool isTileBlockedForEnemies(int col, int row) const;
	bool isTileBlockedForLamp(int col, int row) const;

	void refreshBombPassThroughState();

	void damagePlayer();
	void respawnPlayerImmediately();

	void updateBombs(float deltaTime);
	void explodeBomb(BombermanBomb& bomb);

	void maybeSpawnPowerUpAt(BombermanGridPosition gridPosition);
	bool isPowerUpAtTile(BombermanGridPosition gridPosition) const;

	void collectPowerUps();
	void applyPowerUp(PowerUpType type);

	void addExplosionTile(std::vector<BombermanExplosionTile>& tiles,
		BombermanGridPosition position,
		BombermanExplosionTileType type,
		bool flipX = false,
		bool flipY = false);

	void updateExplosions(float deltaTime);
	void applyExplosionDamage();
	bool isGridPositionCurrentlyExploding(BombermanGridPosition gridPosition) const;

	void updateEnemies(float deltaTime);
	void checkPlayerEnemyCollision();

	void updateWinLoseState();
	bool isPlayerStandingOnExit() const;

	void beginTeleportOut();
	void updateTeleport(float deltaTime);
	void finishTeleportOut();
	void beginTeleportIn();
	void finishTeleportIn();

	const sf::Texture* getCurrentTeleportTexture() const;
	std::size_t getCurrentTeleportFrameIndex() const;

	std::vector<std::string> getSortedLevelPaths() const;
	bool getNextLevelPath(std::string& outNextLevelPath) const;
	int getWorldIndexForLevelPath(const std::string& levelPath) const;

	static bool isValidPlayableLevelFile(const std::filesystem::path& path);
	static int extractLevelNumber(const std::filesystem::path& path);
	static std::string normalizePathForCompare(const std::filesystem::path& path);

	void drawTextureInTile(sf::RenderTarget& target,
		const sf::Texture& texture,
		BombermanGridPosition gridPosition) const;

	void drawTextureInTile(sf::RenderTarget& target,
		const sf::Texture& texture,
		BombermanGridPosition gridPosition,
		bool flipX,
		bool flipY) const;

	void drawPowerUpInTile(sf::RenderTarget& target, const ActivePowerUp& powerUp) const;
	void drawPowerUpIcon(sf::RenderTarget& target, PowerUpType type, sf::Vector2f position, float size) const;

	const PowerUpTexture& getPowerUpTexture(PowerUpType type) const;
	sf::Color getFallbackPowerUpColor(PowerUpType type) const;
	std::string getPowerUpShortName(PowerUpType type) const;

	void refreshUiText(sf::Vector2u windowSize);

	sf::FloatRect getTileBounds(BombermanGridPosition gridPosition) const;
	bool rectsIntersect(const sf::FloatRect& a, const sf::FloatRect& b) const;

private:
	std::string m_bombermanRootDirectory;
	std::string m_resourcesDirectory;
	std::string m_mapsDirectory;
	std::string m_currentMapPath;

	BombermanLevel m_level;
	BombermanPlayer m_player;
	std::vector<BombermanEnemy> m_enemies;

	std::vector<BombermanBomb> m_bombs;
	std::vector<ActiveExplosion> m_explosions;
	std::vector<ActivePowerUp> m_powerUps;

	AnimationFrames m_bombAnimation;
	std::vector<std::size_t> m_bombAnimationSequence;
	std::size_t m_bombAnimationSequenceIndex = 0;
	float m_bombAnimationTimer = 0.f;
	float m_bombAnimationFrameDuration = 0.16f;

	AnimationFrames m_explosionCenterAnimation;
	AnimationFrames m_explosionHorizontalAnimation;
	AnimationFrames m_explosionHorizontalEndAnimation;
	AnimationFrames m_explosionVerticalAnimation;
	AnimationFrames m_explosionVerticalEndAnimation;

	float m_explosionDuration = 0.38f;

	AnimationFrames m_teleportAnimation;
	BombermanGridPosition m_teleportGridPosition{ 0, 0 };
	float m_teleportTimer = 0.f;
	float m_teleportFrameDuration = 0.08f;

	PowerUpTexture m_fireUpTexture;
	PowerUpTexture m_bombUpTexture;
	PowerUpTexture m_speedUpTexture;

	sf::SoundBuffer m_bombExplodesBuffer;
	sf::SoundBuffer m_bombermanDiesBuffer;
	sf::SoundBuffer m_itemGetBuffer;
	sf::SoundBuffer m_placeBombBuffer;
	sf::SoundBuffer m_walkingBuffer;

	std::optional<sf::Sound> m_bombExplodesSound;
	std::optional<sf::Sound> m_bombermanDiesSound;
	std::optional<sf::Sound> m_itemGetSound;
	std::optional<sf::Sound> m_placeBombSound;

	std::optional<sf::Sound> m_walkingSound;
	std::optional<sf::Sound> m_walkingSecondSound;
	std::optional<sf::Sound> m_walkingThirdSound;
	std::optional<sf::Sound> m_walkingFourthSound;

	float m_footstepTimer = 0.f;
	float m_footstepInterval = 1.0f;
	float m_extraFootstepDelay = 0.25f;

	float m_secondFootstepTimer = 0.f;
	float m_thirdFootstepTimer = 0.f;
	float m_fourthFootstepTimer = 0.f;

	bool m_secondFootstepPending = false;
	bool m_thirdFootstepPending = false;
	bool m_fourthFootstepPending = false;

	sf::Font m_font;

	std::optional<sf::Text> m_statusText;
	std::optional<sf::Text> m_helpText;
	std::optional<sf::Text> m_statsText;
	std::optional<sf::Text> m_objectiveText;
	std::optional<sf::Text> m_powerUpHudText;

	PlayState m_playState = PlayState::Playing;

	int m_playerLives = 3;
	int m_bombRange = 2;
	int m_maxActiveBombs = 1;

	int m_fireUpLevel = 0;
	int m_bombUpLevel = 0;
	int m_speedUpLevel = 0;

	float m_basePlayerSpeed = 150.f;
	float m_speedUpAmount = 18.f;
	float m_maxPlayerSpeed = 240.f;

	float m_powerUpDropChance = 0.35f;

	BombermanGridPosition m_hiddenExitPosition{ 0, 0 };
	bool m_hiddenExitAssigned = false;

	std::mt19937 m_rng{ std::random_device{}() };

	bool m_spaceHeldLastFrame = false;
	bool m_restartHeldLastFrame = false;

	std::string m_lastError;
};