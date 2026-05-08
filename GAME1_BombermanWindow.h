#pragma once

#include "BombermanBomb.h"
#include "BombermanEnemy.h"
#include "BombermanLevel.h"
#include "BombermanPlayer.h"
#include "BombermanTypes.h"

#include <SFML/Graphics.hpp>
#include <cstddef>
#include <optional>
#include <random>
#include <string>
#include <vector>

class GAME1_BombermanWindow
{
public:
	bool load(const std::string& fontPath, const std::string& bombermanRootDirectory);

	void reset();

	void update(float deltaTime, sf::Vector2u windowSize);
	void layout(const sf::RenderWindow& window);
	void draw(sf::RenderWindow& window) const;

	const std::string& getLastError() const;

private:
	enum class PlayState
	{
		Playing,
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

	bool tryLoadPowerUpTexture(PowerUpTexture& target, const std::vector<std::string>& candidatePaths);

	bool loadLevelAndActors();

	bool assignHiddenExitToBreakableBlock();
	bool isHiddenExitBlock(BombermanGridPosition gridPosition) const;
	void revealHiddenExitAt(BombermanGridPosition gridPosition);

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

	PowerUpTexture m_fireUpTexture;
	PowerUpTexture m_bombUpTexture;
	PowerUpTexture m_speedUpTexture;

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