#include "GAME1_BombermanWindow.h"

#include "ArcadeInput.h"


#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>

namespace
{
	std::string ToLower(std::string value)
	{
		std::transform(value.begin(), value.end(), value.begin(),
			[](unsigned char c)
			{
				return static_cast<char>(std::tolower(c));
			});

		return value;
	}

	bool IsPngFile(const std::filesystem::path& path)
	{
		if (!path.has_extension())
			return false;

		return ToLower(path.extension().string()) == ".png";
	}

	std::optional<int> ExtractTrailingNumber(const std::filesystem::path& path)
	{
		const std::string stem = path.stem().string();

		if (stem.empty())
			return std::nullopt;

		int end = static_cast<int>(stem.size()) - 1;

		if (!std::isdigit(static_cast<unsigned char>(stem[end])))
			return std::nullopt;

		int start = end;

		while (start > 0 && std::isdigit(static_cast<unsigned char>(stem[start - 1])))
		{
			--start;
		}

		try
		{
			return std::stoi(stem.substr(
				static_cast<std::size_t>(start),
				static_cast<std::size_t>(end - start + 1)));
		}
		catch (...)
		{
			return std::nullopt;
		}
	}

	bool NaturalFrameSort(const std::filesystem::path& a, const std::filesystem::path& b)
	{
		const std::optional<int> numberA = ExtractTrailingNumber(a);
		const std::optional<int> numberB = ExtractTrailingNumber(b);

		if (numberA.has_value() && numberB.has_value() && numberA.value() != numberB.value())
			return numberA.value() < numberB.value();

		return a.filename().string() < b.filename().string();
	}

	bool TryParseWorldMetadata(const std::string& line, int& outWorldNumber)
	{
		const std::string prefix = "#WORLD=";

		if (line.rfind(prefix, 0) != 0)
			return false;

		try
		{
			const int parsedWorld = std::stoi(line.substr(prefix.size()));

			if (parsedWorld >= 1)
				outWorldNumber = parsedWorld;
		}
		catch (...)
		{
		}

		return true;
	}

	int ManhattanDistance(BombermanGridPosition a, BombermanGridPosition b)
	{
		return std::abs(a.col - b.col) + std::abs(a.row - b.row);
	}
}

bool GAME1_BombermanWindow::load(const std::string& fontPath, const std::string& bombermanRootDirectory)
{
	namespace fs = std::filesystem;

	m_lastError.clear();

	m_bombermanRootDirectory = bombermanRootDirectory;
	m_resourcesDirectory = (fs::path(m_bombermanRootDirectory) / "Resources").string();
	m_mapsDirectory = (fs::path(m_bombermanRootDirectory) / "Maps").string();
	m_currentMapPath = (fs::path(m_mapsDirectory) / "level01.txt").string();

	if (!m_font.openFromFile(fontPath))
	{
		m_lastError = "Failed to load Bomberman font: " + fontPath;
		return false;
	}

	m_statusText.emplace(m_font);
	m_statusText->setCharacterSize(44);
	m_statusText->setFillColor(sf::Color::White);
	m_statusText->setOutlineColor(sf::Color::Black);
	m_statusText->setOutlineThickness(3.f);

	m_helpText.emplace(m_font);
	m_helpText->setCharacterSize(20);
	m_helpText->setFillColor(sf::Color(230, 230, 230));
	m_helpText->setOutlineColor(sf::Color::Black);
	m_helpText->setOutlineThickness(1.5f);

	m_statsText.emplace(m_font);
	m_statsText->setCharacterSize(22);
	m_statsText->setFillColor(sf::Color::White);
	m_statsText->setOutlineColor(sf::Color::Black);
	m_statsText->setOutlineThickness(2.f);

	m_objectiveText.emplace(m_font);
	m_objectiveText->setCharacterSize(22);
	m_objectiveText->setFillColor(sf::Color(255, 230, 120));
	m_objectiveText->setOutlineColor(sf::Color::Black);
	m_objectiveText->setOutlineThickness(2.f);

	m_powerUpHudText.emplace(m_font);
	m_powerUpHudText->setCharacterSize(20);
	m_powerUpHudText->setFillColor(sf::Color::White);
	m_powerUpHudText->setOutlineColor(sf::Color::Black);
	m_powerUpHudText->setOutlineThickness(2.f);

	const fs::path audioDirectory = fs::path(m_resourcesDirectory) / "Audio";

	if (!loadSound(m_bombExplodesBuffer, m_bombExplodesSound, (audioDirectory / "Bomb_Explodes.wav").string(), "bomb explosion"))
		return false;

	if (!loadSound(m_bombermanDiesBuffer, m_bombermanDiesSound, (audioDirectory / "Bomberman_Dies.wav").string(), "Bomberman death"))
		return false;

	if (!loadSound(m_itemGetBuffer, m_itemGetSound, (audioDirectory / "Item_Get.wav").string(), "item pickup"))
		return false;

	if (!loadSound(m_placeBombBuffer, m_placeBombSound, (audioDirectory / "Place_Bomb.wav").string(), "place bomb"))
		return false;

	if (!loadSound(m_punchBombBuffer, m_punchBombSound, (audioDirectory / "Punch_Bomb.wav").string(), "punch bomb"))
		return false;

	if (!loadSound(m_walkingBuffer, m_walkingSound, (audioDirectory / "Walking_2.wav").string(), "walking"))
		return false;

	if (m_walkingSound.has_value())
	{
		m_walkingSound->setLooping(false);
		m_walkingSound->setPitch(1.f);

		m_walkingSecondSound.emplace(m_walkingBuffer);
		m_walkingSecondSound->setVolume(100.f);
		m_walkingSecondSound->setPitch(1.f);
		m_walkingSecondSound->setLooping(false);

		m_walkingThirdSound.emplace(m_walkingBuffer);
		m_walkingThirdSound->setVolume(100.f);
		m_walkingThirdSound->setPitch(1.f);
		m_walkingThirdSound->setLooping(false);

		m_walkingFourthSound.emplace(m_walkingBuffer);
		m_walkingFourthSound->setVolume(100.f);
		m_walkingFourthSound->setPitch(1.f);
		m_walkingFourthSound->setLooping(false);
	}

	const fs::path bombsDirectory = fs::path(m_resourcesDirectory) / "Bombs";
	const fs::path bombAnimationDirectory = bombsDirectory / "BombAnim";
	const fs::path explosionAnimationDirectory = bombsDirectory / "ExplosionAnim";

	if (!loadAnimationFramesFromDirectory(m_bombAnimation, bombAnimationDirectory.string(), "bomb animation"))
		return false;

	buildBombAnimationSequence();

	if (!loadAnimationFramesFromDirectory(m_explosionCenterAnimation, (explosionAnimationDirectory / "Center").string(), "explosion center animation"))
		return false;

	if (!loadAnimationFramesFromDirectory(m_explosionHorizontalAnimation, (explosionAnimationDirectory / "Horizontal").string(), "explosion horizontal animation"))
		return false;

	if (!loadAnimationFramesFromDirectory(m_explosionHorizontalEndAnimation, (explosionAnimationDirectory / "HorizontalEnd").string(), "explosion horizontal end animation"))
		return false;

	if (!loadAnimationFramesFromDirectory(m_explosionVerticalAnimation, (explosionAnimationDirectory / "Vertical").string(), "explosion vertical animation"))
		return false;

	if (!loadAnimationFramesFromDirectory(m_explosionVerticalEndAnimation, (explosionAnimationDirectory / "VerticalEnd").string(), "explosion vertical end animation"))
		return false;

	const fs::path teleportDirectory = fs::path(m_resourcesDirectory) / "Player" / "Blue" / "Teleport";

	if (!loadAnimationFramesFromDirectory(m_teleportAnimation, teleportDirectory.string(), "player teleport animation"))
		return false;

	const fs::path powerUpsDirectory = fs::path(m_resourcesDirectory) / "PowerUps";

	tryLoadPowerUpTexture(m_fireUpTexture,
		{
			(powerUpsDirectory / "fire_up.png").string(),
			(powerUpsDirectory / "FireUp.png").string(),
			(powerUpsDirectory / "fire.png").string(),
			(powerUpsDirectory / "powerup_fire.png").string()
		});

	tryLoadPowerUpTexture(m_bombUpTexture,
		{
			(powerUpsDirectory / "bomb_up.png").string(),
			(powerUpsDirectory / "BombUp.png").string(),
			(powerUpsDirectory / "bombup.png").string(),
			(powerUpsDirectory / "powerup_bomb.png").string()
		});

	tryLoadPowerUpTexture(m_speedUpTexture,
		{
			(powerUpsDirectory / "speed_up.png").string(),
			(powerUpsDirectory / "SpeedUp.png").string(),
			(powerUpsDirectory / "speed.png").string(),
			(powerUpsDirectory / "powerup_speed.png").string()
		});

	if (!m_player.load((fs::path(m_resourcesDirectory) / "Player").string()))
	{
		m_lastError = m_player.getLastError();
		return false;
	}

	if (!loadLevelAndActors(true))
		return false;

	refreshUiText({ 1024, 640 });

	return true;
}

bool GAME1_BombermanWindow::loadMapFromFile(const std::string& mapPath)
{
	m_currentMapPath = mapPath;
	return loadLevelAndActors(true);
}

bool GAME1_BombermanWindow::loadAnimationFramesFromDirectory(AnimationFrames& animation,
	const std::string& directoryPath,
	const std::string& readableName)
{
	namespace fs = std::filesystem;

	animation.frames.clear();

	const fs::path directory(directoryPath);

	if (!fs::exists(directory) || !fs::is_directory(directory))
	{
		m_lastError = "Failed to load Bomberman " + readableName + ": folder does not exist: " + directoryPath;
		return false;
	}

	std::vector<fs::path> framePaths;

	for (const auto& entry : fs::directory_iterator(directory))
	{
		if (!entry.is_regular_file())
			continue;

		if (!IsPngFile(entry.path()))
			continue;

		framePaths.push_back(entry.path());
	}

	std::sort(framePaths.begin(), framePaths.end(), NaturalFrameSort);

	if (framePaths.empty())
	{
		m_lastError = "Failed to load Bomberman " + readableName + ": no PNG frames found in: " + directoryPath;
		return false;
	}

	animation.frames.reserve(framePaths.size());

	for (const fs::path& framePath : framePaths)
	{
		sf::Texture texture;

		if (!texture.loadFromFile(framePath.string()))
		{
			m_lastError = "Failed to load Bomberman " + readableName + " frame: " + framePath.string();
			return false;
		}

		animation.frames.push_back(std::move(texture));
	}

	return true;
}

bool GAME1_BombermanWindow::loadSound(sf::SoundBuffer& buffer,
	std::optional<sf::Sound>& sound,
	const std::string& path,
	const std::string& readableName)
{
	if (!buffer.loadFromFile(path))
	{
		m_lastError = "Failed to load Bomberman " + readableName + " sound: " + path;
		return false;
	}

	sound.emplace(buffer);
	sound->setVolume(100.f);
	sound->setPitch(1.f);

	return true;
}

void GAME1_BombermanWindow::playSound(std::optional<sf::Sound>& sound)
{
	if (!sound.has_value())
		return;

	sound->stop();
	sound->setPitch(1.f);
	sound->play();
}

void GAME1_BombermanWindow::updateWalkingSound(float deltaTime)
{
	if (!m_walkingSound.has_value() ||
		!m_walkingSecondSound.has_value() ||
		!m_walkingThirdSound.has_value() ||
		!m_walkingFourthSound.has_value())
	{
		return;
	}

	const bool shouldPlayWalking =
		m_playState == PlayState::Playing &&
		m_player.isAlive() &&
		isWalkingInputHeld();

	if (!shouldPlayWalking)
	{
		stopWalkingSound();

		m_footstepTimer = 0.f;

		m_secondFootstepTimer = 0.f;
		m_thirdFootstepTimer = 0.f;
		m_fourthFootstepTimer = 0.f;

		m_secondFootstepPending = false;
		m_thirdFootstepPending = false;
		m_fourthFootstepPending = false;

		return;
	}

	auto playFootstepChannel = [](std::optional<sf::Sound>& sound)
		{
			if (!sound.has_value())
				return;

			sound->stop();
			sound->setPitch(1.f);
			sound->play();
		};

	if (m_secondFootstepPending)
	{
		m_secondFootstepTimer -= deltaTime;

		if (m_secondFootstepTimer <= 0.f)
		{
			playFootstepChannel(m_walkingSecondSound);
			m_secondFootstepPending = false;
			m_secondFootstepTimer = 0.f;
		}
	}

	if (m_thirdFootstepPending)
	{
		m_thirdFootstepTimer -= deltaTime;

		if (m_thirdFootstepTimer <= 0.f)
		{
			playFootstepChannel(m_walkingThirdSound);
			m_thirdFootstepPending = false;
			m_thirdFootstepTimer = 0.f;
		}
	}

	if (m_fourthFootstepPending)
	{
		m_fourthFootstepTimer -= deltaTime;

		if (m_fourthFootstepTimer <= 0.f)
		{
			playFootstepChannel(m_walkingFourthSound);
			m_fourthFootstepPending = false;
			m_fourthFootstepTimer = 0.f;
		}
	}

	m_footstepTimer -= deltaTime;

	if (m_footstepTimer <= 0.f)
	{
		playFootstepChannel(m_walkingSound);

		m_secondFootstepPending = true;
		m_thirdFootstepPending = true;
		m_fourthFootstepPending = true;

		m_secondFootstepTimer = m_extraFootstepDelay;
		m_thirdFootstepTimer = m_extraFootstepDelay * 2.f;
		m_fourthFootstepTimer = m_extraFootstepDelay * 3.f;

		m_footstepTimer = m_footstepInterval;
	}
}

void GAME1_BombermanWindow::stopWalkingSound()
{
	if (m_walkingSound.has_value() &&
		m_walkingSound->getStatus() == sf::SoundSource::Status::Playing)
	{
		m_walkingSound->stop();
	}

	if (m_walkingSecondSound.has_value() &&
		m_walkingSecondSound->getStatus() == sf::SoundSource::Status::Playing)
	{
		m_walkingSecondSound->stop();
	}

	if (m_walkingThirdSound.has_value() &&
		m_walkingThirdSound->getStatus() == sf::SoundSource::Status::Playing)
	{
		m_walkingThirdSound->stop();
	}

	if (m_walkingFourthSound.has_value() &&
		m_walkingFourthSound->getStatus() == sf::SoundSource::Status::Playing)
	{
		m_walkingFourthSound->stop();
	}
}

bool GAME1_BombermanWindow::isWalkingInputHeld() const
{
	return
		ArcadeInput::isMoveUpHeld() ||
		ArcadeInput::isMoveDownHeld() ||
		ArcadeInput::isMoveLeftHeld() ||
		ArcadeInput::isMoveRightHeld();
}

bool GAME1_BombermanWindow::tryLoadPowerUpTexture(PowerUpTexture& target,
	const std::vector<std::string>& candidatePaths)
{
	for (const std::string& path : candidatePaths)
	{
		if (target.texture.loadFromFile(path))
		{
			target.loaded = true;
			return true;
		}
	}

	target.loaded = false;
	return false;
}

bool GAME1_BombermanWindow::loadLevelAndActors(bool resetPerks)
{
	namespace fs = std::filesystem;

	const int savedBombRange = m_bombRange;
	const int savedMaxActiveBombs = m_maxActiveBombs;
	const int savedFireUpLevel = m_fireUpLevel;
	const int savedBombUpLevel = m_bombUpLevel;
	const int savedSpeedUpLevel = m_speedUpLevel;
	const float savedPlayerSpeed = m_player.getMoveSpeed();

	m_bombs.clear();
	m_explosions.clear();
	m_powerUps.clear();
	m_enemies.clear();

	m_hiddenExitPosition = { 0, 0 };
	m_hiddenExitAssigned = false;

	m_teleportTimer = 0.f;
	m_teleportGridPosition = { 0, 0 };

	if (m_currentMapPath.empty())
	{
		m_currentMapPath = (fs::path(m_mapsDirectory) / "level01.txt").string();
	}

	if (!m_level.loadFromFile(m_currentMapPath, m_resourcesDirectory))
	{
		m_lastError = m_level.getLastError();
		return false;
	}

	std::vector<BombermanGridPosition> forbiddenBreakablePositions;
	forbiddenBreakablePositions.push_back(m_level.getPlayerSpawn());

	for (const BombermanGridPosition& enemySpawn : m_level.getEnemySpawns())
	{
		forbiddenBreakablePositions.push_back(enemySpawn);
	}

	m_level.generateRandomBreakableBlocks(
		m_rng,
		5,
		0.20f,
		forbiddenBreakablePositions);

	if (!assignHiddenExitToBreakableBlock())
		return false;

	m_player.reset(m_level.getPlayerSpawn(), m_level);

	m_playerLives = 3;

	if (resetPerks)
	{
		resetPerksToDefaults();
	}
	else
	{
		m_bombRange = savedBombRange;
		m_maxActiveBombs = savedMaxActiveBombs;
		m_fireUpLevel = savedFireUpLevel;
		m_bombUpLevel = savedBombUpLevel;
		m_speedUpLevel = savedSpeedUpLevel;
		m_player.setMoveSpeed(savedPlayerSpeed);
	}

	const fs::path enemiesDirectory = fs::path(m_resourcesDirectory) / "Enemies";

	for (const BombermanEnemySpawn& spawnEntry : m_level.getEnemySpawnEntries())
	{
		std::string enemyFolderName = "Copter";

		switch (spawnEntry.type)
		{
		case BombermanEnemyType::Lamp:
			enemyFolderName = "Lamp";
			break;

		case BombermanEnemyType::Tree:
			enemyFolderName = "Tree";
			break;

		case BombermanEnemyType::Bomber:
			enemyFolderName = "Bomber";
			break;

		case BombermanEnemyType::Chomper:
			enemyFolderName = "Chomper";
			break;

		case BombermanEnemyType::Copter:
		default:
			enemyFolderName = "Copter";
			break;
		}

		const fs::path enemyDirectory = enemiesDirectory / enemyFolderName;

		m_enemies.emplace_back();

		if (!m_enemies.back().load(
			enemyDirectory.string(),
			spawnEntry.type,
			spawnEntry.gridPosition,
			m_level))
		{
			m_lastError = m_enemies.back().getLastError();
			m_enemies.pop_back();
			return false;
		}
	}

	m_playState = PlayState::Playing;
	m_spaceHeldLastFrame = false;
	m_punchHeldLastFrame = false;
	m_restartHeldLastFrame = false;

	m_bombAnimationSequenceIndex = 0;
	m_bombAnimationTimer = 0.f;

	stopWalkingSound();

	m_footstepTimer = 0.f;

	m_secondFootstepTimer = 0.f;
	m_thirdFootstepTimer = 0.f;
	m_fourthFootstepTimer = 0.f;

	m_secondFootstepPending = false;
	m_thirdFootstepPending = false;
	m_fourthFootstepPending = false;

	return true;
}

void GAME1_BombermanWindow::resetPerksToDefaults()
{
	m_bombRange = 2;
	m_maxActiveBombs = 1;

	m_fireUpLevel = 0;
	m_bombUpLevel = 0;
	m_speedUpLevel = 0;

	m_player.setMoveSpeed(m_basePlayerSpeed);
}

bool GAME1_BombermanWindow::assignHiddenExitToBreakableBlock()
{
	const std::vector<BombermanGridPosition> breakableBlocks = m_level.getBreakableBlockPositions();

	if (breakableBlocks.empty())
	{
		m_lastError = "Bomberman level error: hidden exit needs at least one breakable block.";
		return false;
	}

	std::uniform_int_distribution<int> blockDistribution(
		0,
		static_cast<int>(breakableBlocks.size()) - 1);

	m_hiddenExitPosition = breakableBlocks[blockDistribution(m_rng)];
	m_hiddenExitAssigned = true;

	return true;
}

bool GAME1_BombermanWindow::isHiddenExitBlock(BombermanGridPosition gridPosition) const
{
	return m_hiddenExitAssigned && gridPosition == m_hiddenExitPosition;
}

void GAME1_BombermanWindow::revealHiddenExitAt(BombermanGridPosition gridPosition)
{
	if (!isHiddenExitBlock(gridPosition))
		return;

	m_level.revealExitAt(gridPosition.col, gridPosition.row);
	m_hiddenExitAssigned = false;
}

void GAME1_BombermanWindow::processCompletedBrokenBlocks()
{
	const std::vector<BombermanGridPosition> completedBlocks = m_level.consumeCompletedBrokenBlocks();

	for (const BombermanGridPosition& gridPosition : completedBlocks)
	{
		handleCompletedBrokenBlock(gridPosition);
	}
}

void GAME1_BombermanWindow::handleCompletedBrokenBlock(BombermanGridPosition gridPosition)
{
	if (isHiddenExitBlock(gridPosition))
	{
		revealHiddenExitAt(gridPosition);
		return;
	}

	maybeSpawnPowerUpAt(gridPosition);
}

void GAME1_BombermanWindow::buildBombAnimationSequence()
{
	m_bombAnimationSequence.clear();

	if (m_bombAnimation.frames.empty())
		return;

	if (m_bombAnimation.frames.size() >= 3)
	{
		m_bombAnimationSequence =
		{
			0,
			1,
			2,
			1,
			0
		};
	}
	else if (m_bombAnimation.frames.size() == 2)
	{
		m_bombAnimationSequence =
		{
			0,
			1,
			0
		};
	}
	else
	{
		m_bombAnimationSequence =
		{
			0
		};
	}

	m_bombAnimationSequenceIndex = 0;
	m_bombAnimationTimer = 0.f;
}

void GAME1_BombermanWindow::updateBombAnimation(float deltaTime)
{
	if (m_bombAnimationSequence.size() <= 1)
		return;

	m_bombAnimationTimer += deltaTime;

	while (m_bombAnimationTimer >= m_bombAnimationFrameDuration)
	{
		m_bombAnimationTimer -= m_bombAnimationFrameDuration;
		m_bombAnimationSequenceIndex =
			(m_bombAnimationSequenceIndex + 1) % m_bombAnimationSequence.size();
	}
}

const sf::Texture* GAME1_BombermanWindow::getCurrentBombTexture() const
{
	if (m_bombAnimation.frames.empty())
		return nullptr;

	if (m_bombAnimationSequence.empty())
		return &m_bombAnimation.frames[0];

	const std::size_t frameIndex =
		m_bombAnimationSequence[m_bombAnimationSequenceIndex % m_bombAnimationSequence.size()];

	return &m_bombAnimation.frames[frameIndex % m_bombAnimation.frames.size()];
}

std::size_t GAME1_BombermanWindow::getExplosionFrameIndex(const ActiveExplosion& explosion,
	const AnimationFrames& animation) const
{
	if (animation.frames.empty())
		return 0;

	if (animation.frames.size() == 1)
		return 0;

	const float duration = std::max(0.001f, explosion.duration);
	const float elapsed = std::clamp(duration - explosion.timer, 0.f, duration);
	const float normalized = std::clamp(elapsed / duration, 0.f, 1.f);

	std::size_t frameIndex = static_cast<std::size_t>(
		normalized * static_cast<float>(animation.frames.size()));

	if (frameIndex >= animation.frames.size())
		frameIndex = animation.frames.size() - 1;

	return frameIndex;
}

const GAME1_BombermanWindow::AnimationFrames& GAME1_BombermanWindow::getExplosionAnimationForTile(
	BombermanExplosionTileType type) const
{
	switch (type)
	{
	case BombermanExplosionTileType::Center:
		return m_explosionCenterAnimation;

	case BombermanExplosionTileType::Horizontal:
		return m_explosionHorizontalAnimation;

	case BombermanExplosionTileType::HorizontalEnd:
		return m_explosionHorizontalEndAnimation;

	case BombermanExplosionTileType::Vertical:
		return m_explosionVerticalAnimation;

	case BombermanExplosionTileType::VerticalEnd:
	default:
		return m_explosionVerticalEndAnimation;
	}
}

void GAME1_BombermanWindow::reset()
{
	m_lastError.clear();

	stopWalkingSound();

	m_footstepTimer = 0.f;

	m_secondFootstepTimer = 0.f;
	m_thirdFootstepTimer = 0.f;
	m_fourthFootstepTimer = 0.f;

	m_secondFootstepPending = false;
	m_thirdFootstepPending = false;
	m_fourthFootstepPending = false;

	loadLevelAndActors(true);
}

void GAME1_BombermanWindow::update(float deltaTime, sf::Vector2u windowSize)
{
	const bool restartHeld = ArcadeInput::isRestartHeld();

	if (restartHeld && !m_restartHeldLastFrame)
	{
		reset();
	}

	m_restartHeldLastFrame = restartHeld;

	m_level.updateAnimations(deltaTime);
	processCompletedBrokenBlocks();
	updateBombAnimation(deltaTime);

	if (m_playState == PlayState::Playing)
	{
		const bool spaceHeld = ArcadeInput::isPrimaryHeld();
		const bool punchHeld = ArcadeInput::isSecondaryHeld();

		if (spaceHeld && !m_spaceHeldLastFrame)
		{
			placeBomb();
		}

		if (punchHeld && !m_punchHeldLastFrame)
		{
			m_player.startPunch();

			const bool punchedBomb = tryPunchBomb();

			if (!punchedBomb)
			{
				tryPunchBreakableBlock();
			}
		}

		m_spaceHeldLastFrame = spaceHeld;
		m_punchHeldLastFrame = punchHeld;

		m_player.update(
			deltaTime,
			m_level,
			[this](int col, int row)
			{
				return isTileBlockedForPlayer(col, row);
			});

		updateWalkingSound(deltaTime);

		refreshBombPassThroughState();
		collectPowerUps();

		updateEnemies(deltaTime);
		updateBombs(deltaTime);
		updateExplosions(deltaTime);

		applyExplosionDamage();
		checkPlayerEnemyCollision();
		updateWinLoseState();
	}
	else if (m_playState == PlayState::TeleportingOut ||
		m_playState == PlayState::TeleportingIn)
	{
		m_spaceHeldLastFrame = ArcadeInput::isPrimaryHeld();
		m_punchHeldLastFrame = ArcadeInput::isSecondaryHeld();

		stopWalkingSound();

		m_footstepTimer = 0.f;

		m_secondFootstepTimer = 0.f;
		m_thirdFootstepTimer = 0.f;
		m_fourthFootstepTimer = 0.f;

		m_secondFootstepPending = false;
		m_thirdFootstepPending = false;
		m_fourthFootstepPending = false;

		updateTeleport(deltaTime);
		updateExplosions(deltaTime);
	}
	else
	{
		m_spaceHeldLastFrame = ArcadeInput::isPrimaryHeld();
		m_punchHeldLastFrame = ArcadeInput::isSecondaryHeld();

		stopWalkingSound();

		m_footstepTimer = 0.f;

		m_secondFootstepTimer = 0.f;
		m_thirdFootstepTimer = 0.f;
		m_fourthFootstepTimer = 0.f;

		m_secondFootstepPending = false;
		m_thirdFootstepPending = false;
		m_fourthFootstepPending = false;

		updateExplosions(deltaTime);
	}

	refreshUiText(windowSize);
}

void GAME1_BombermanWindow::layout(const sf::RenderWindow& window)
{
	refreshUiText(window.getSize());
}

void GAME1_BombermanWindow::placeBomb()
{
	if (!m_player.isAlive())
		return;

	const int activeBombCount = static_cast<int>(std::count_if(
		m_bombs.begin(),
		m_bombs.end(),
		[](const BombermanBomb& bomb)
		{
			return !bomb.hasExploded();
		}));

	if (activeBombCount >= m_maxActiveBombs)
		return;

	const BombermanGridPosition bombPosition = m_player.getGridPosition(m_level);

	if (isBombAtTile(bombPosition))
		return;

	BombermanBomb bomb(bombPosition, 2.0f, m_bombRange);
	bomb.setPlayerCanPassThrough(true);

	m_bombs.push_back(bomb);

	playSound(m_placeBombSound);
}

bool GAME1_BombermanWindow::tryPunchBomb()
{
	if (!m_player.isAlive())
		return false;

	const BombermanGridPosition playerGrid = m_player.getGridPosition(m_level);
	const BombermanGridPosition punchDirection = m_player.getFacingDirectionDelta();

	const BombermanGridPosition punchedTile{
		playerGrid.col + punchDirection.col,
		playerGrid.row + punchDirection.row
	};

	for (std::size_t bombIndex = 0; bombIndex < m_bombs.size(); ++bombIndex)
	{
		BombermanBomb& bomb = m_bombs[bombIndex];

		if (bomb.hasExploded())
			continue;

		if (bomb.isSliding())
			continue;

		if (bomb.getGridPosition() != punchedTile)
			continue;

		const int maxTilesToSlide = std::max(0, static_cast<int>(std::round(m_punchForceTiles)));

		BombermanGridPosition targetPosition = bomb.getGridPosition();

		for (int step = 0; step < maxTilesToSlide; ++step)
		{
			const BombermanGridPosition nextPosition{
				targetPosition.col + punchDirection.col,
				targetPosition.row + punchDirection.row
			};

			if (isTileBlockedForSlidingBomb(nextPosition.col, nextPosition.row, bombIndex))
				break;

			targetPosition = nextPosition;
		}

		if (targetPosition == bomb.getGridPosition())
			return false;

		bomb.startSlide(targetPosition, m_punchedBombSlideSpeed);
		playSound(m_punchBombSound);
		return true;
	}

	return false;
}

bool GAME1_BombermanWindow::tryPunchBreakableBlock()
{
	if (!m_player.isAlive())
		return false;

	const BombermanGridPosition playerGrid = m_player.getGridPosition(m_level);
	const BombermanGridPosition punchDirection = m_player.getFacingDirectionDelta();

	const BombermanGridPosition punchedTile{
		playerGrid.col + punchDirection.col,
		playerGrid.row + punchDirection.row
	};

	return m_level.startBreakingBlock(punchedTile.col, punchedTile.row);
}

bool GAME1_BombermanWindow::isBombAtTile(BombermanGridPosition gridPosition) const
{
	for (const BombermanBomb& bomb : m_bombs)
	{
		if (!bomb.hasExploded() && bomb.occupiesGridPosition(gridPosition))
			return true;
	}

	return false;
}

bool GAME1_BombermanWindow::isBombAtTileIgnoringIndex(BombermanGridPosition gridPosition, std::size_t ignoredBombIndex) const
{
	for (std::size_t i = 0; i < m_bombs.size(); ++i)
	{
		if (i == ignoredBombIndex)
			continue;

		const BombermanBomb& bomb = m_bombs[i];

		if (!bomb.hasExploded() && bomb.occupiesGridPosition(gridPosition))
			return true;
	}

	return false;
}

bool GAME1_BombermanWindow::isTileBlockedForPlayer(int col, int row) const
{
	if (m_level.isBlockedForMovement(col, row))
		return true;

	for (const BombermanBomb& bomb : m_bombs)
	{
		if (bomb.hasExploded())
			continue;

		if (!bomb.occupiesGridPosition({ col, row }))
			continue;

		if (bomb.canPlayerPassThrough())
			continue;

		return true;
	}

	return false;
}

bool GAME1_BombermanWindow::isTileBlockedForEnemies(int col, int row) const
{
	if (m_level.isBlockedForMovement(col, row))
		return true;

	for (const BombermanBomb& bomb : m_bombs)
	{
		if (bomb.hasExploded())
			continue;

		if (bomb.occupiesGridPosition({ col, row }))
			return true;
	}

	return false;
}

bool GAME1_BombermanWindow::isTileBlockedForLamp(int col, int row) const
{
	if (m_level.isWall(col, row))
		return true;

	for (const BombermanBomb& bomb : m_bombs)
	{
		if (bomb.hasExploded())
			continue;

		if (bomb.occupiesGridPosition({ col, row }))
			return true;
	}

	return false;
}

bool GAME1_BombermanWindow::isTileBlockedForSlidingBomb(int col, int row, std::size_t ignoredBombIndex) const
{
	if (m_level.isBlockedForMovement(col, row))
		return true;

	if (isBombAtTileIgnoringIndex({ col, row }, ignoredBombIndex))
		return true;

	for (const BombermanEnemy& enemy : m_enemies)
	{
		if (!enemy.isAlive())
			continue;

		if (enemy.getGridPosition(m_level) == BombermanGridPosition{ col, row })
			return true;
	}

	return false;
}

void GAME1_BombermanWindow::refreshBombPassThroughState()
{
	if (!m_player.isAlive())
		return;

	const sf::FloatRect playerCollisionBounds = m_player.getCollisionBounds();

	for (BombermanBomb& bomb : m_bombs)
	{
		if (bomb.hasExploded())
			continue;

		if (!bomb.canPlayerPassThrough())
			continue;

		const sf::FloatRect bombTileBounds = getTileBounds(bomb.getGridPosition());

		if (!rectsIntersect(playerCollisionBounds, bombTileBounds))
		{
			bomb.setPlayerCanPassThrough(false);
		}
	}
}

void GAME1_BombermanWindow::damagePlayer()
{
	if (!m_player.isAlive())
		return;

	if (m_player.isInvincible())
		return;

	stopWalkingSound();

	m_footstepTimer = 0.f;

	m_secondFootstepTimer = 0.f;
	m_thirdFootstepTimer = 0.f;
	m_fourthFootstepTimer = 0.f;

	m_secondFootstepPending = false;
	m_thirdFootstepPending = false;
	m_fourthFootstepPending = false;

	playSound(m_bombermanDiesSound);

	m_player.kill();
	--m_playerLives;

	if (m_playerLives <= 0)
	{
		m_playerLives = 0;
		m_playState = PlayState::GameOver;
		return;
	}

	respawnPlayerImmediately();
}

void GAME1_BombermanWindow::respawnPlayerImmediately()
{
	m_bombs.clear();
	m_explosions.clear();

	m_player.reset(m_level.getPlayerSpawn(), m_level);
	m_player.beginInvincibility(3.0f);
}

void GAME1_BombermanWindow::updateBombs(float deltaTime)
{
	for (BombermanBomb& bomb : m_bombs)
	{
		bomb.update(deltaTime);

		if (bomb.shouldExplode())
		{
			explodeBomb(bomb);
			bomb.markExploded();
		}
	}

	m_bombs.erase(
		std::remove_if(
			m_bombs.begin(),
			m_bombs.end(),
			[](const BombermanBomb& bomb)
			{
				return bomb.hasExploded();
			}),
		m_bombs.end());
}

void GAME1_BombermanWindow::explodeBomb(BombermanBomb& bomb)
{
	ActiveExplosion explosion;
	explosion.timer = m_explosionDuration;
	explosion.duration = m_explosionDuration;

	const BombermanGridPosition center = bomb.getGridPosition();

	addExplosionTile(explosion.tiles, center, BombermanExplosionTileType::Center);

	constexpr int directionCount = 4;

	const int directionCols[directionCount] = { 1, -1, 0, 0 };
	const int directionRows[directionCount] = { 0, 0, 1, -1 };

	for (int directionIndex = 0; directionIndex < directionCount; ++directionIndex)
	{
		const int directionCol = directionCols[directionIndex];
		const int directionRow = directionRows[directionIndex];

		for (int distance = 1; distance <= bomb.getExplosionRange(); ++distance)
		{
			const BombermanGridPosition current{
				center.col + directionCol * distance,
				center.row + directionRow * distance
			};

			if (!m_level.isInside(current.col, current.row))
				break;

			if (m_level.isWall(current.col, current.row))
				break;

			const bool isBreakable = m_level.isBreakableBlock(current.col, current.row);

			const BombermanGridPosition next{
				current.col + directionCol,
				current.row + directionRow
			};

			const bool reachedMaxRange = distance == bomb.getExplosionRange();
			const bool nextBlockedByWall =
				!m_level.isInside(next.col, next.row) ||
				m_level.isWall(next.col, next.row);

			const bool shouldUseEndCap =
				isBreakable ||
				reachedMaxRange ||
				nextBlockedByWall;

			BombermanExplosionTileType visualType;
			bool flipX = false;
			bool flipY = false;

			if (directionRow == 0)
			{
				if (shouldUseEndCap)
				{
					visualType = BombermanExplosionTileType::HorizontalEnd;
					flipX = directionCol < 0;
				}
				else
				{
					visualType = BombermanExplosionTileType::Horizontal;
				}
			}
			else
			{
				if (shouldUseEndCap)
				{
					visualType = BombermanExplosionTileType::VerticalEnd;
					flipY = directionRow > 0;
				}
				else
				{
					visualType = BombermanExplosionTileType::Vertical;
				}
			}

			addExplosionTile(explosion.tiles, current, visualType, flipX, flipY);

			for (BombermanBomb& otherBomb : m_bombs)
			{
				if (!otherBomb.hasExploded() && otherBomb.occupiesGridPosition(current))
				{
					otherBomb.triggerNow();
				}
			}

			if (isBreakable)
			{
				m_level.startBreakingBlock(current.col, current.row);
				break;
			}

			if (shouldUseEndCap)
				break;
		}
	}

	m_explosions.push_back(std::move(explosion));

	playSound(m_bombExplodesSound);
}

void GAME1_BombermanWindow::maybeSpawnPowerUpAt(BombermanGridPosition gridPosition)
{
	if (isPowerUpAtTile(gridPosition))
		return;

	std::uniform_real_distribution<float> dropDistribution(0.f, 1.f);

	if (dropDistribution(m_rng) > m_powerUpDropChance)
		return;

	std::uniform_int_distribution<int> typeDistribution(0, 2);

	ActivePowerUp powerUp;
	powerUp.gridPosition = gridPosition;
	powerUp.collected = false;

	const int typeValue = typeDistribution(m_rng);

	if (typeValue == 0)
		powerUp.type = PowerUpType::FireUp;
	else if (typeValue == 1)
		powerUp.type = PowerUpType::BombUp;
	else
		powerUp.type = PowerUpType::SpeedUp;

	m_powerUps.push_back(powerUp);
}

bool GAME1_BombermanWindow::isPowerUpAtTile(BombermanGridPosition gridPosition) const
{
	for (const ActivePowerUp& powerUp : m_powerUps)
	{
		if (!powerUp.collected && powerUp.gridPosition == gridPosition)
			return true;
	}

	return false;
}

void GAME1_BombermanWindow::collectPowerUps()
{
	if (!m_player.isAlive())
		return;

	const BombermanGridPosition playerGrid = m_player.getGridPosition(m_level);

	for (ActivePowerUp& powerUp : m_powerUps)
	{
		if (powerUp.collected)
			continue;

		if (powerUp.gridPosition == playerGrid)
		{
			powerUp.collected = true;
			applyPowerUp(powerUp.type);
			playSound(m_itemGetSound);
		}
	}

	m_powerUps.erase(
		std::remove_if(
			m_powerUps.begin(),
			m_powerUps.end(),
			[](const ActivePowerUp& powerUp)
			{
				return powerUp.collected;
			}),
		m_powerUps.end());
}

void GAME1_BombermanWindow::applyPowerUp(PowerUpType type)
{
	switch (type)
	{
	case PowerUpType::FireUp:
		++m_fireUpLevel;
		m_bombRange = std::min(8, m_bombRange + 1);
		break;

	case PowerUpType::BombUp:
		++m_bombUpLevel;
		m_maxActiveBombs = std::min(8, m_maxActiveBombs + 1);
		break;

	case PowerUpType::SpeedUp:
		++m_speedUpLevel;
		m_player.setMoveSpeed(std::min(m_maxPlayerSpeed, m_player.getMoveSpeed() + m_speedUpAmount));
		break;
	}
}

void GAME1_BombermanWindow::addExplosionTile(std::vector<BombermanExplosionTile>& tiles,
	BombermanGridPosition position,
	BombermanExplosionTileType type,
	bool flipX,
	bool flipY)
{
	const auto alreadyExists = std::any_of(
		tiles.begin(),
		tiles.end(),
		[position](const BombermanExplosionTile& tile)
		{
			return tile.gridPosition == position;
		});

	if (alreadyExists)
		return;

	BombermanExplosionTile tile;
	tile.gridPosition = position;
	tile.type = type;
	tile.flipX = flipX;
	tile.flipY = flipY;

	tiles.push_back(tile);
}

void GAME1_BombermanWindow::updateExplosions(float deltaTime)
{
	for (ActiveExplosion& explosion : m_explosions)
	{
		explosion.timer -= deltaTime;
	}

	m_explosions.erase(
		std::remove_if(
			m_explosions.begin(),
			m_explosions.end(),
			[](const ActiveExplosion& explosion)
			{
				return explosion.timer <= 0.f;
			}),
		m_explosions.end());
}

void GAME1_BombermanWindow::applyExplosionDamage()
{
	static std::vector<const BombermanEnemy*> enemiesCurrentlyTouchingExplosion;

	if (m_explosions.empty())
	{
		enemiesCurrentlyTouchingExplosion.clear();
	}

	if (m_player.isAlive() && !m_player.isInvincible())
	{
		const BombermanGridPosition playerGrid = m_player.getGridPosition(m_level);

		if (isGridPositionCurrentlyExploding(playerGrid))
		{
			damagePlayer();
		}
	}

	for (BombermanEnemy& enemy : m_enemies)
	{
		const BombermanEnemy* enemyPointer = &enemy;

		if (!enemy.isAlive())
		{
			enemiesCurrentlyTouchingExplosion.erase(
				std::remove(enemiesCurrentlyTouchingExplosion.begin(), enemiesCurrentlyTouchingExplosion.end(), enemyPointer),
				enemiesCurrentlyTouchingExplosion.end());

			continue;
		}

		const BombermanGridPosition enemyGrid = enemy.getGridPosition(m_level);
		const bool isTouchingExplosion = isGridPositionCurrentlyExploding(enemyGrid);

		const auto found = std::find(
			enemiesCurrentlyTouchingExplosion.begin(),
			enemiesCurrentlyTouchingExplosion.end(),
			enemyPointer);

		if (isTouchingExplosion)
		{
			if (found == enemiesCurrentlyTouchingExplosion.end())
			{
				enemy.takeHit();
				enemiesCurrentlyTouchingExplosion.push_back(enemyPointer);
			}
		}
		else
		{
			if (found != enemiesCurrentlyTouchingExplosion.end())
			{
				enemiesCurrentlyTouchingExplosion.erase(found);
			}
		}
	}
}

bool GAME1_BombermanWindow::isGridPositionCurrentlyExploding(BombermanGridPosition gridPosition) const
{
	for (const ActiveExplosion& explosion : m_explosions)
	{
		for (const BombermanExplosionTile& tile : explosion.tiles)
		{
			if (tile.gridPosition == gridPosition)
				return true;
		}
	}

	return false;
}

void GAME1_BombermanWindow::updateEnemies(float deltaTime)
{
	const BombermanGridPosition playerGridPosition = m_player.getGridPosition(m_level);

	auto isEnemyAtTileIgnoringIndex =
		[this](BombermanGridPosition gridPosition, std::size_t ignoredIndex)
		{
			for (std::size_t i = 0; i < m_enemies.size(); ++i)
			{
				if (i == ignoredIndex)
					continue;

				const BombermanEnemy& otherEnemy = m_enemies[i];

				if (!otherEnemy.isAlive())
					continue;

				if (otherEnemy.getGridPosition(m_level) == gridPosition)
					return true;
			}

			return false;
		};

	auto explodeAt = [this](BombermanGridPosition center, int range)
		{
			ActiveExplosion explosion;
			explosion.timer = m_explosionDuration;
			explosion.duration = m_explosionDuration;

			addExplosionTile(explosion.tiles, center, BombermanExplosionTileType::Center);

			constexpr int directionCount = 4;

			const int directionCols[directionCount] = { 1, -1, 0, 0 };
			const int directionRows[directionCount] = { 0, 0, 1, -1 };

			for (int directionIndex = 0; directionIndex < directionCount; ++directionIndex)
			{
				const int directionCol = directionCols[directionIndex];
				const int directionRow = directionRows[directionIndex];

				for (int distance = 1; distance <= range; ++distance)
				{
					const BombermanGridPosition current{
						center.col + directionCol * distance,
						center.row + directionRow * distance
					};

					if (!m_level.isInside(current.col, current.row))
						break;

					if (m_level.isWall(current.col, current.row))
						break;

					const bool isBreakable = m_level.isBreakableBlock(current.col, current.row);

					const BombermanGridPosition next{
						current.col + directionCol,
						current.row + directionRow
					};

					const bool reachedMaxRange = distance == range;
					const bool nextBlockedByWall =
						!m_level.isInside(next.col, next.row) ||
						m_level.isWall(next.col, next.row);

					const bool shouldUseEndCap =
						isBreakable ||
						reachedMaxRange ||
						nextBlockedByWall;

					BombermanExplosionTileType visualType;
					bool flipX = false;
					bool flipY = false;

					if (directionRow == 0)
					{
						if (shouldUseEndCap)
						{
							visualType = BombermanExplosionTileType::HorizontalEnd;
							flipX = directionCol < 0;
						}
						else
						{
							visualType = BombermanExplosionTileType::Horizontal;
						}
					}
					else
					{
						if (shouldUseEndCap)
						{
							visualType = BombermanExplosionTileType::VerticalEnd;
							flipY = directionRow > 0;
						}
						else
						{
							visualType = BombermanExplosionTileType::Vertical;
						}
					}

					addExplosionTile(explosion.tiles, current, visualType, flipX, flipY);

					for (BombermanBomb& otherBomb : m_bombs)
					{
						if (!otherBomb.hasExploded() && otherBomb.occupiesGridPosition(current))
						{
							otherBomb.triggerNow();
						}
					}

					if (isBreakable)
					{
						m_level.startBreakingBlock(current.col, current.row);
						break;
					}

					if (shouldUseEndCap)
						break;
				}
			}

			m_explosions.push_back(std::move(explosion));
			playSound(m_bombExplodesSound);
		};

	for (std::size_t enemyIndex = 0; enemyIndex < m_enemies.size(); ++enemyIndex)
	{
		BombermanEnemy& enemy = m_enemies[enemyIndex];

		if (!enemy.isAlive())
			continue;

		BombermanGridPosition enemyTarget = playerGridPosition;

		if (enemy.wantsToEatBombs())
		{
			const BombermanGridPosition enemyGrid = enemy.getGridPosition(m_level);

			int bestDistance = std::numeric_limits<int>::max();
			bool foundBombTarget = false;

			for (const BombermanBomb& bomb : m_bombs)
			{
				if (bomb.hasExploded())
					continue;

				const BombermanGridPosition bombGrid = bomb.getGridPosition();
				const int distance = ManhattanDistance(enemyGrid, bombGrid);

				if (distance < bestDistance)
				{
					bestDistance = distance;
					enemyTarget = bombGrid;
					foundBombTarget = true;
				}
			}

			if (!foundBombTarget)
				enemyTarget = playerGridPosition;
		}

		const bool isLamp = enemy.canPassThroughBreakableBlocks();
		const bool isChomper = enemy.wantsToEatBombs();

		enemy.update(
			deltaTime,
			enemyTarget,
			[this, enemyIndex, isLamp, isChomper, &isEnemyAtTileIgnoringIndex](int col, int row)
			{
				if (isLamp)
				{
					if (m_level.isWall(col, row))
						return true;
				}
				else
				{
					if (m_level.isBlockedForMovement(col, row))
						return true;
				}

				if (!isChomper)
				{
					for (const BombermanBomb& bomb : m_bombs)
					{
						if (bomb.hasExploded())
							continue;

						if (bomb.occupiesGridPosition({ col, row }))
							return true;
					}
				}

				if (isEnemyAtTileIgnoringIndex({ col, row }, enemyIndex))
					return true;

				return false;
			});

		if (enemy.consumePendingBomberExplosion())
		{
			explodeAt(enemy.getGridPosition(m_level), enemy.getBomberExplosionRange());
		}
	}

	for (BombermanEnemy& enemy : m_enemies)
	{
		if (!enemy.isAlive())
			continue;

		if (!enemy.wantsToEatBombs())
			continue;

		const BombermanGridPosition enemyGrid = enemy.getGridPosition(m_level);

		m_bombs.erase(
			std::remove_if(
				m_bombs.begin(),
				m_bombs.end(),
				[enemyGrid](const BombermanBomb& bomb)
				{
					return !bomb.hasExploded() && bomb.occupiesGridPosition(enemyGrid);
				}),
			m_bombs.end());
	}
}

void GAME1_BombermanWindow::checkPlayerEnemyCollision()
{
	if (!m_player.isAlive() || m_player.isInvincible())
		return;

	for (const BombermanEnemy& enemy : m_enemies)
	{
		if (!enemy.isAlive())
			continue;

		if (rectsIntersect(m_player.getCollisionBounds(), enemy.getBounds()))
		{
			damagePlayer();
			return;
		}
	}
}

void GAME1_BombermanWindow::updateWinLoseState()
{
	if (m_playState == PlayState::GameOver ||
		m_playState == PlayState::Victory ||
		m_playState == PlayState::TeleportingOut ||
		m_playState == PlayState::TeleportingIn)
	{
		return;
	}

	if (m_level.hasExit() && isPlayerStandingOnExit())
	{
		beginTeleportOut();
	}
}

bool GAME1_BombermanWindow::isPlayerStandingOnExit() const
{
	if (!m_player.isAlive())
		return false;

	if (!m_level.hasExit())
		return false;

	return m_player.getGridPosition(m_level) == m_level.getExitPosition();
}

void GAME1_BombermanWindow::beginTeleportOut()
{
	stopWalkingSound();

	m_bombs.clear();
	m_explosions.clear();

	m_teleportGridPosition = m_player.getGridPosition(m_level);
	m_teleportTimer = 0.f;
	m_playState = PlayState::TeleportingOut;
}

void GAME1_BombermanWindow::updateTeleport(float deltaTime)
{
	if (m_teleportAnimation.frames.empty())
	{
		if (m_playState == PlayState::TeleportingOut)
			finishTeleportOut();
		else if (m_playState == PlayState::TeleportingIn)
			finishTeleportIn();

		return;
	}

	m_teleportTimer += deltaTime;

	const float totalDuration =
		static_cast<float>(m_teleportAnimation.frames.size()) * m_teleportFrameDuration;

	if (m_teleportTimer >= totalDuration)
	{
		if (m_playState == PlayState::TeleportingOut)
		{
			finishTeleportOut();
		}
		else if (m_playState == PlayState::TeleportingIn)
		{
			finishTeleportIn();
		}
	}
}

void GAME1_BombermanWindow::finishTeleportOut()
{
	std::string nextLevelPath;

	if (!getNextLevelPath(nextLevelPath))
	{
		m_playState = PlayState::Victory;
		return;
	}

	const int currentWorldIndex = getWorldIndexForLevelPath(m_currentMapPath);
	const int nextWorldIndex = getWorldIndexForLevelPath(nextLevelPath);

	const bool sameWorld = currentWorldIndex == nextWorldIndex;

	m_currentMapPath = nextLevelPath;

	if (!loadLevelAndActors(!sameWorld))
	{
		m_playState = PlayState::GameOver;
		return;
	}

	beginTeleportIn();
}

void GAME1_BombermanWindow::beginTeleportIn()
{
	stopWalkingSound();

	m_bombs.clear();
	m_explosions.clear();

	m_teleportGridPosition = m_level.getPlayerSpawn();
	m_teleportTimer = 0.f;
	m_playState = PlayState::TeleportingIn;
}

void GAME1_BombermanWindow::finishTeleportIn()
{
	m_teleportTimer = 0.f;
	m_playState = PlayState::Playing;
	m_player.beginInvincibility(1.0f);
}

const sf::Texture* GAME1_BombermanWindow::getCurrentTeleportTexture() const
{
	if (m_teleportAnimation.frames.empty())
		return nullptr;

	return &m_teleportAnimation.frames[getCurrentTeleportFrameIndex()];
}

std::size_t GAME1_BombermanWindow::getCurrentTeleportFrameIndex() const
{
	if (m_teleportAnimation.frames.empty())
		return 0;

	const std::size_t frameCount = m_teleportAnimation.frames.size();

	std::size_t forwardIndex = static_cast<std::size_t>(m_teleportTimer / m_teleportFrameDuration);

	if (forwardIndex >= frameCount)
		forwardIndex = frameCount - 1;

	if (m_playState == PlayState::TeleportingIn)
	{
		return frameCount - 1 - forwardIndex;
	}

	return forwardIndex;
}

std::vector<std::string> GAME1_BombermanWindow::getSortedLevelPaths() const
{
	namespace fs = std::filesystem;

	std::vector<fs::path> paths;

	const fs::path mapsDirectory(m_mapsDirectory);

	if (!fs::exists(mapsDirectory) || !fs::is_directory(mapsDirectory))
		return {};

	for (const auto& entry : fs::directory_iterator(mapsDirectory))
	{
		if (entry.is_regular_file() && isValidPlayableLevelFile(entry.path()))
		{
			paths.push_back(entry.path());
		}
	}

	std::sort(paths.begin(), paths.end(),
		[](const fs::path& a, const fs::path& b)
		{
			return extractLevelNumber(a) < extractLevelNumber(b);
		});

	std::vector<std::string> result;
	result.reserve(paths.size());

	for (const fs::path& path : paths)
	{
		result.push_back(path.string());
	}

	return result;
}

bool GAME1_BombermanWindow::getNextLevelPath(std::string& outNextLevelPath) const
{
	const std::vector<std::string> levels = getSortedLevelPaths();

	if (levels.empty())
		return false;

	const std::string currentNormalized = normalizePathForCompare(m_currentMapPath);

	for (std::size_t i = 0; i < levels.size(); ++i)
	{
		if (normalizePathForCompare(levels[i]) == currentNormalized)
		{
			if (i + 1 < levels.size())
			{
				outNextLevelPath = levels[i + 1];
				return true;
			}

			return false;
		}
	}

	const int currentNumber = extractLevelNumber(std::filesystem::path(m_currentMapPath));

	for (std::size_t i = 0; i < levels.size(); ++i)
	{
		if (extractLevelNumber(std::filesystem::path(levels[i])) == currentNumber)
		{
			if (i + 1 < levels.size())
			{
				outNextLevelPath = levels[i + 1];
				return true;
			}

			return false;
		}
	}

	return false;
}

int GAME1_BombermanWindow::getWorldIndexForLevelPath(const std::string& levelPath) const
{
	std::ifstream file(levelPath);

	if (!file.is_open())
		return 1;

	std::string line;
	int worldNumber = 1;

	while (std::getline(file, line))
	{
		if (!line.empty() && line.back() == '\r')
			line.pop_back();

		if (TryParseWorldMetadata(line, worldNumber))
			return worldNumber;

		if (!line.empty() && line[0] != '#')
			break;
	}

	return worldNumber;
}

bool GAME1_BombermanWindow::isValidPlayableLevelFile(const std::filesystem::path& path)
{
	if (!path.has_filename() || path.extension() != ".txt")
		return false;

	const std::string stem = path.stem().string();

	if (stem.rfind("leveltemplate", 0) == 0)
		return false;

	if (stem.rfind("level", 0) != 0)
		return false;

	if (stem.size() <= 5)
		return false;

	for (std::size_t i = 5; i < stem.size(); ++i)
	{
		if (!std::isdigit(static_cast<unsigned char>(stem[i])))
			return false;
	}

	return true;
}

int GAME1_BombermanWindow::extractLevelNumber(const std::filesystem::path& path)
{
	const std::string stem = path.stem().string();

	if (stem.rfind("level", 0) != 0 || stem.size() <= 5)
		return 0;

	try
	{
		return std::stoi(stem.substr(5));
	}
	catch (...)
	{
		return 0;
	}
}

std::string GAME1_BombermanWindow::normalizePathForCompare(const std::filesystem::path& path)
{
	namespace fs = std::filesystem;

	try
	{
		return fs::absolute(path).lexically_normal().generic_string();
	}
	catch (...)
	{
		return path.lexically_normal().generic_string();
	}
}

void GAME1_BombermanWindow::drawTextureInTile(sf::RenderTarget& target,
	const sf::Texture& texture,
	BombermanGridPosition gridPosition) const
{
	drawTextureInTile(target, texture, gridPosition, false, false);
}

void GAME1_BombermanWindow::drawTextureInTile(sf::RenderTarget& target,
	const sf::Texture& texture,
	BombermanGridPosition gridPosition,
	bool flipX,
	bool flipY) const
{
	sf::Sprite sprite(texture);

	const sf::FloatRect localBounds = sprite.getLocalBounds();

	if (localBounds.size.x <= 0.f || localBounds.size.y <= 0.f)
		return;

	const float scaleX = static_cast<float>(BombermanLevel::TileSize) / localBounds.size.x;
	const float scaleY = static_cast<float>(BombermanLevel::TileSize) / localBounds.size.y;

	sprite.setScale({
		flipX ? -scaleX : scaleX,
		flipY ? -scaleY : scaleY
		});

	float drawX = static_cast<float>(gridPosition.col * BombermanLevel::TileSize);
	float drawY = static_cast<float>(gridPosition.row * BombermanLevel::TileSize);

	if (flipX)
		drawX += static_cast<float>(BombermanLevel::TileSize);

	if (flipY)
		drawY += static_cast<float>(BombermanLevel::TileSize);

	sprite.setPosition({ drawX, drawY });

	target.draw(sprite);
}

void GAME1_BombermanWindow::drawTextureAtWorldPosition(sf::RenderTarget& target,
	const sf::Texture& texture,
	sf::Vector2f worldPosition) const
{
	sf::Sprite sprite(texture);

	const sf::FloatRect localBounds = sprite.getLocalBounds();

	if (localBounds.size.x <= 0.f || localBounds.size.y <= 0.f)
		return;

	const float scaleX = static_cast<float>(BombermanLevel::TileSize) / localBounds.size.x;
	const float scaleY = static_cast<float>(BombermanLevel::TileSize) / localBounds.size.y;

	sprite.setScale({ scaleX, scaleY });
	sprite.setPosition(worldPosition);

	target.draw(sprite);
}

void GAME1_BombermanWindow::drawPowerUpInTile(sf::RenderTarget& target, const ActivePowerUp& powerUp) const
{
	if (powerUp.collected)
		return;

	const PowerUpTexture& powerUpTexture = getPowerUpTexture(powerUp.type);

	if (powerUpTexture.loaded)
	{
		drawTextureInTile(target, powerUpTexture.texture, powerUp.gridPosition);
		return;
	}

	const sf::FloatRect tileBounds = getTileBounds(powerUp.gridPosition);

	sf::CircleShape fallbackCircle;
	fallbackCircle.setRadius(15.f);
	fallbackCircle.setPointCount(24);
	fallbackCircle.setFillColor(getFallbackPowerUpColor(powerUp.type));
	fallbackCircle.setOutlineColor(sf::Color::White);
	fallbackCircle.setOutlineThickness(2.f);
	fallbackCircle.setPosition({
		tileBounds.position.x + tileBounds.size.x * 0.5f - 15.f,
		tileBounds.position.y + tileBounds.size.y * 0.5f - 15.f
		});

	target.draw(fallbackCircle);

	sf::Text label(m_font);
	label.setString(getPowerUpShortName(powerUp.type));
	label.setCharacterSize(15);
	label.setFillColor(sf::Color::Black);
	label.setStyle(sf::Text::Bold);

	const sf::FloatRect bounds = label.getLocalBounds();

	label.setPosition({
		tileBounds.position.x + (tileBounds.size.x - bounds.size.x) * 0.5f - bounds.position.x,
		tileBounds.position.y + (tileBounds.size.y - bounds.size.y) * 0.5f - bounds.position.y - 2.f
		});

	target.draw(label);
}

void GAME1_BombermanWindow::drawPowerUpIcon(sf::RenderTarget& target,
	PowerUpType type,
	sf::Vector2f position,
	float size) const
{
	const PowerUpTexture& powerUpTexture = getPowerUpTexture(type);

	if (powerUpTexture.loaded)
	{
		sf::Sprite sprite(powerUpTexture.texture);

		const sf::FloatRect localBounds = sprite.getLocalBounds();

		if (localBounds.size.x > 0.f && localBounds.size.y > 0.f)
		{
			sprite.setScale({
				size / localBounds.size.x,
				size / localBounds.size.y
				});

			sprite.setPosition(position);
			target.draw(sprite);
			return;
		}
	}

	sf::CircleShape fallbackCircle;
	fallbackCircle.setRadius(size * 0.5f);
	fallbackCircle.setPointCount(24);
	fallbackCircle.setFillColor(getFallbackPowerUpColor(type));
	fallbackCircle.setOutlineColor(sf::Color::White);
	fallbackCircle.setOutlineThickness(1.5f);
	fallbackCircle.setPosition(position);

	target.draw(fallbackCircle);
}

const GAME1_BombermanWindow::PowerUpTexture& GAME1_BombermanWindow::getPowerUpTexture(PowerUpType type) const
{
	switch (type)
	{
	case PowerUpType::FireUp:
		return m_fireUpTexture;

	case PowerUpType::BombUp:
		return m_bombUpTexture;

	case PowerUpType::SpeedUp:
	default:
		return m_speedUpTexture;
	}
}

sf::Color GAME1_BombermanWindow::getFallbackPowerUpColor(PowerUpType type) const
{
	switch (type)
	{
	case PowerUpType::FireUp:
		return sf::Color(255, 100, 50);

	case PowerUpType::BombUp:
		return sf::Color(90, 170, 255);

	case PowerUpType::SpeedUp:
	default:
		return sf::Color(120, 255, 120);
	}
}

std::string GAME1_BombermanWindow::getPowerUpShortName(PowerUpType type) const
{
	switch (type)
	{
	case PowerUpType::FireUp:
		return "F";

	case PowerUpType::BombUp:
		return "B";

	case PowerUpType::SpeedUp:
	default:
		return "S";
	}
}

void GAME1_BombermanWindow::draw(sf::RenderWindow& window) const
{
	const sf::Vector2u windowSize = window.getSize();

	const sf::View previousView = window.getView();

	sf::View screenView(
		sf::FloatRect(
			{ 0.f, 0.f },
			{ static_cast<float>(windowSize.x), static_cast<float>(windowSize.y) }
		)
	);

	window.setView(screenView);

	sf::RectangleShape background;
	background.setPosition({ 0.f, 0.f });
	background.setSize({
		static_cast<float>(windowSize.x),
		static_cast<float>(windowSize.y)
		});
	background.setFillColor(sf::Color(28, 28, 36));
	window.draw(background);

	sf::View worldView;
	worldView.setSize({
		static_cast<float>(windowSize.x),
		static_cast<float>(windowSize.y)
		});

	worldView.setCenter({
		m_level.getPixelWidth() * 0.5f,
		m_level.getPixelHeight() * 0.5f
		});

	window.setView(worldView);

	m_level.drawFloorLayer(window);

	const BombermanGridPosition playerGrid = m_player.getGridPosition(m_level);
	const sf::Texture* currentBombTexture = getCurrentBombTexture();
	const sf::Texture* currentTeleportTexture = getCurrentTeleportTexture();

	for (int row = 0; row < m_level.getHeightInTiles(); ++row)
	{
		for (int col = 0; col < m_level.getWidthInTiles(); ++col)
		{
			m_level.drawWorldTileAt(window, col, row);
		}

		for (const ActivePowerUp& powerUp : m_powerUps)
		{
			if (!powerUp.collected && powerUp.gridPosition.row == row)
			{
				drawPowerUpInTile(window, powerUp);
			}
		}

		if (currentBombTexture != nullptr)
		{
			for (const BombermanBomb& bomb : m_bombs)
			{
				const int bombDrawRow = static_cast<int>(
					std::floor((bomb.getDrawPosition().y + static_cast<float>(BombermanLevel::TileSize) * 0.5f) /
						static_cast<float>(BombermanLevel::TileSize)));

				if (bombDrawRow == row)
				{
					drawTextureAtWorldPosition(window, *currentBombTexture, bomb.getDrawPosition());
				}
			}
		}

		for (const BombermanEnemy& enemy : m_enemies)
		{
			if (!enemy.isAlive())
				continue;

			const int enemyDrawRow = static_cast<int>(
				std::floor((enemy.getDrawPosition().y + static_cast<float>(BombermanLevel::TileSize) * 0.5f) /
					static_cast<float>(BombermanLevel::TileSize)));

			if (enemyDrawRow != row)
				continue;

			if (enemy.shouldDrawAsBomb())
			{
				if (currentBombTexture != nullptr)
				{
					drawTextureAtWorldPosition(window, *currentBombTexture, enemy.getDrawPosition());
				}
			}
			else
			{
				enemy.draw(window);
			}
		}

		if (m_playState != PlayState::TeleportingOut &&
			m_playState != PlayState::TeleportingIn &&
			m_player.isAlive() &&
			playerGrid.row == row)
		{
			m_player.draw(window);
		}

		if ((m_playState == PlayState::TeleportingOut ||
			m_playState == PlayState::TeleportingIn) &&
			currentTeleportTexture != nullptr &&
			m_teleportGridPosition.row == row)
		{
			drawTextureInTile(window, *currentTeleportTexture, m_teleportGridPosition);
		}

		for (const ActiveExplosion& explosion : m_explosions)
		{
			for (const BombermanExplosionTile& tile : explosion.tiles)
			{
				if (tile.gridPosition.row != row)
					continue;

				const AnimationFrames& animation = getExplosionAnimationForTile(tile.type);

				if (animation.frames.empty())
					continue;

				const std::size_t frameIndex = getExplosionFrameIndex(explosion, animation);

				drawTextureInTile(
					window,
					animation.frames[frameIndex],
					tile.gridPosition,
					tile.flipX,
					tile.flipY);
			}
		}
	}

	window.setView(screenView);

	if (m_statsText)
		window.draw(*m_statsText);

	if (m_objectiveText)
		window.draw(*m_objectiveText);

	const float perkIconX = static_cast<float>(windowSize.x) - 158.f;
	drawPowerUpIcon(window, PowerUpType::FireUp, { perkIconX, 58.f }, 28.f);
	drawPowerUpIcon(window, PowerUpType::BombUp, { perkIconX, 98.f }, 28.f);
	drawPowerUpIcon(window, PowerUpType::SpeedUp, { perkIconX, 138.f }, 28.f);

	if (m_powerUpHudText)
		window.draw(*m_powerUpHudText);

	if (m_playState != PlayState::Playing &&
		m_playState != PlayState::TeleportingOut &&
		m_playState != PlayState::TeleportingIn &&
		m_statusText)
	{
		sf::RectangleShape overlay;
		overlay.setPosition({ 0.f, 0.f });
		overlay.setSize({
			static_cast<float>(windowSize.x),
			static_cast<float>(windowSize.y)
			});
		overlay.setFillColor(sf::Color(0, 0, 0, 145));
		window.draw(overlay);

		window.draw(*m_statusText);
	}
window.setView(previousView);
}

void GAME1_BombermanWindow::refreshUiText(sf::Vector2u windowSize)
{
	const int livingEnemies = static_cast<int>(std::count_if(
		m_enemies.begin(),
		m_enemies.end(),
		[](const BombermanEnemy& enemy)
		{
			return enemy.isAlive();
		}));

	const std::string levelName = std::filesystem::path(m_currentMapPath).stem().string();

	if (m_statsText)
	{
		m_statsText->setString(
			"PLAYER STATS\n"
			"Level: " + levelName + "\n" +
			"World: " + std::to_string(m_level.getWorldNumber()) + "\n" +
			"Lives: " + std::to_string(std::max(0, m_playerLives)) + "\n" +
			"Bombs: " + std::to_string(m_maxActiveBombs) + "\n" +
			"Range: " + std::to_string(m_bombRange) + "\n" +
			"Speed: " + std::to_string(static_cast<int>(m_player.getMoveSpeed())) + "\n" +
			"Enemies: " + std::to_string(livingEnemies)
		);

		m_statsText->setPosition({ 18.f, 14.f });
	}

	if (m_objectiveText)
	{
		if (m_playState == PlayState::TeleportingOut)
		{
			m_objectiveText->setString("Objective complete: Teleporting...");
			m_objectiveText->setFillColor(sf::Color(120, 255, 255));
		}
		else if (m_playState == PlayState::TeleportingIn)
		{
			m_objectiveText->setString("Entering next level...");
			m_objectiveText->setFillColor(sf::Color(120, 255, 255));
		}
		else if (!m_level.hasExit())
		{
			m_objectiveText->setString("Objective: Destroy blocks to find the hidden exit");
			m_objectiveText->setFillColor(sf::Color(255, 230, 120));
		}
		else
		{
			m_objectiveText->setString("Objective: Exit found! Step on it to escape");
			m_objectiveText->setFillColor(sf::Color(120, 255, 140));
		}

		const sf::FloatRect bounds = m_objectiveText->getLocalBounds();

		m_objectiveText->setPosition({
			(static_cast<float>(windowSize.x) - bounds.size.x) * 0.5f - bounds.position.x,
			static_cast<float>(windowSize.y) - 42.f - bounds.position.y
			});
	}

	if (m_powerUpHudText)
	{
		m_powerUpHudText->setString(
			"PERKS\n"
			"Fire  x" + std::to_string(m_fireUpLevel) + "\n" +
			"Bomb  x" + std::to_string(m_bombUpLevel) + "\n" +
			"Speed x" + std::to_string(m_speedUpLevel)
		);

		const sf::FloatRect bounds = m_powerUpHudText->getLocalBounds();

		m_powerUpHudText->setPosition({
			static_cast<float>(windowSize.x) - bounds.size.x - 26.f - bounds.position.x,
			18.f - bounds.position.y
			});
	}

	if (m_helpText)
	{
		m_helpText->setString("");
	}

	if (m_statusText)
	{
		if (m_playState == PlayState::Victory)
		{
			m_statusText->setString("VICTORY!\nAll levels completed\nPress R to restart this level");
		}
		else if (m_playState == PlayState::GameOver)
		{
			m_statusText->setString("GAME OVER\nPress R to restart");
		}
		else
		{
			m_statusText->setString("");
		}

		const sf::FloatRect bounds = m_statusText->getLocalBounds();

		m_statusText->setPosition({
			(static_cast<float>(windowSize.x) - bounds.size.x) * 0.5f - bounds.position.x,
			(static_cast<float>(windowSize.y) - bounds.size.y) * 0.5f - bounds.position.y
			});
	}
}

sf::FloatRect GAME1_BombermanWindow::getTileBounds(BombermanGridPosition gridPosition) const
{
	return sf::FloatRect(
		{
			static_cast<float>(gridPosition.col * BombermanLevel::TileSize),
			static_cast<float>(gridPosition.row * BombermanLevel::TileSize)
		},
		{
			static_cast<float>(BombermanLevel::TileSize),
			static_cast<float>(BombermanLevel::TileSize)
		}
	);
}

bool GAME1_BombermanWindow::rectsIntersect(const sf::FloatRect& a, const sf::FloatRect& b) const
{
	return a.position.x < b.position.x + b.size.x &&
		a.position.x + a.size.x > b.position.x &&
		a.position.y < b.position.y + b.size.y &&
		a.position.y + a.size.y > b.position.y;
}

const std::string& GAME1_BombermanWindow::getLastError() const
{
	return m_lastError;
}