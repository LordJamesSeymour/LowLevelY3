#include "GAME1_Player.h"

#include "GAME1_Level.h"

#include "GAME1_SurfersQuestAudio.h"

#include "ArcadeInput.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
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

	std::filesystem::path ResolveFirstExistingDirectory(const std::vector<std::filesystem::path>& paths)
	{
		for (const std::filesystem::path& path : paths)
		{
			if (std::filesystem::exists(path) && std::filesystem::is_directory(path))
				return path;
		}

		return paths.empty() ? std::filesystem::path() : paths.front();
	}

	int GetLeftTile(const sf::FloatRect& bounds)
	{
		return static_cast<int>(std::floor(bounds.position.x / static_cast<float>(GAME1_Level::TileSize)));
	}

	int GetRightTile(const sf::FloatRect& bounds)
	{
		return static_cast<int>(std::floor((bounds.position.x + bounds.size.x - 0.1f) / static_cast<float>(GAME1_Level::TileSize)));
	}

	int GetTopTile(const sf::FloatRect& bounds)
	{
		return static_cast<int>(std::floor(bounds.position.y / static_cast<float>(GAME1_Level::TileSize)));
	}

	int GetBottomTile(const sf::FloatRect& bounds)
	{
		return static_cast<int>(std::floor((bounds.position.y + bounds.size.y - 0.1f) / static_cast<float>(GAME1_Level::TileSize)));
	}

	float SignNonZero(float value)
	{
		return value < 0.f ? -1.f : 1.f;
	}
}

bool GAME1_Player::load(const std::string& playerIdleDirectory, sf::Vector2f startPosition)
{
	namespace fs = std::filesystem;

	m_lastError.clear();

	const fs::path idleDirectory(playerIdleDirectory);
	const fs::path playerRootDirectory = idleDirectory.parent_path();

	const fs::path runDirectory = playerRootDirectory / "PlayerRun";
	const fs::path jumpDirectory = playerRootDirectory / "PlayerJump";
	const fs::path doubleJumpDirectory = playerRootDirectory / "PlayerDoubleJump";
	const fs::path fallDirectory = playerRootDirectory / "PlayerFall";

	const fs::path wallGrabDirectory = ResolveFirstExistingDirectory(
		{
			playerRootDirectory / "PlayerWallgrab",
			playerRootDirectory / "PlayerWallGrab",
			playerRootDirectory / "PlayerWallgrabAnimation",
			playerRootDirectory / "Wallgrab",
			playerRootDirectory / "WallGrab"
		});

	const fs::path hitDirectory = ResolveFirstExistingDirectory(
		{
			playerRootDirectory / "PlayerHit",
			playerRootDirectory / "Hit"
		});

	if (!loadAnimationFramesFromDirectory(
		m_idleAnimation,
		idleDirectory.string(),
		"SurfersQuest player idle"))
	{
		return false;
	}

	if (!loadAnimationFramesFromDirectory(
		m_runAnimation,
		runDirectory.string(),
		"SurfersQuest player run"))
	{
		return false;
	}

	if (!loadAnimationFramesFromDirectory(
		m_jumpAnimation,
		jumpDirectory.string(),
		"SurfersQuest player jump"))
	{
		return false;
	}

	if (!loadAnimationFramesFromDirectory(
		m_doubleJumpAnimation,
		doubleJumpDirectory.string(),
		"SurfersQuest player double jump"))
	{
		return false;
	}

	if (!loadAnimationFramesFromDirectory(
		m_fallAnimation,
		fallDirectory.string(),
		"SurfersQuest player fall"))
	{
		return false;
	}

	if (!loadAnimationFramesFromDirectory(
		m_wallGrabAnimation,
		wallGrabDirectory.string(),
		"SurfersQuest player wall grab"))
	{
		return false;
	}

	if (!loadAnimationFramesFromDirectory(
		m_hitAnimation,
		hitDirectory.string(),
		"SurfersQuest player hit"))
	{
		return false;
	}

	m_idleAnimation.frameDuration = 0.035f;
	m_runAnimation.frameDuration = 0.035f;
	m_jumpAnimation.frameDuration = 0.035f;
	m_doubleJumpAnimation.frameDuration = 0.035f;
	m_fallAnimation.frameDuration = 0.035f;
	m_wallGrabAnimation.frameDuration = 0.055f;
	m_hitAnimation.frameDuration = 0.055f;

	m_hasUiFont = m_uiFont.openFromFile("assets/menu.ttf");

	fs::path resourcesDirectory = playerRootDirectory;

	if (ToLower(resourcesDirectory.filename().string()) != "resources")
	{
		const fs::path parentDirectory = playerRootDirectory.parent_path();

		if (!parentDirectory.empty())
			resourcesDirectory = parentDirectory;
	}

	GAME1_SurfersQuestAudio::initialise(resourcesDirectory.string());

	m_position = startPosition;
	m_previousPosition = startPosition;
	m_spawnPosition = startPosition;
	m_velocity = { 0.f, 0.f };

	m_health = m_maxHealth;
	m_lives = m_maxLives;
	m_gameOver = false;
	m_damageCooldownTimer = 0.f;
	m_hitAnimationPlaying = false;

	m_wallGrabActive = false;
	m_touchingWallLeft = false;
	m_touchingWallRight = false;
	m_wallJumpControlLockTimer = 0.f;

	m_dropThroughTimer = 0.f;
	m_groundedOnOneWayPlatform = false;

	m_onGround = false;
	m_jumpHeldLastFrame = false;
	m_phaseHeldLastFrame = false;
	resetMovementSoundTimers();

	m_coyoteTimer = 0.f;
	m_jumpBufferTimer = 0.f;

	m_canDoubleJump = true;
	m_doubleJumpAnimationPlaying = false;
	m_variableJumpActive = false;
	m_releasedJumpGravityActive = false;

	m_facingDirection = FacingDirection::Right;
	m_animationState = AnimationState::Idle;
	m_horizontalInputHeld = false;

	m_animationTimer = 0.f;
	m_currentFrameIndex = 0;

	m_respawning = false;
	m_respawnTimer = 0.f;

	return true;
}

bool GAME1_Player::loadAnimationFramesFromDirectory(AnimationSet& animation,
	const std::string& directoryPath,
	const std::string& readableName)
{
	namespace fs = std::filesystem;

	animation.frames.clear();

	const fs::path directory(directoryPath);

	if (!fs::exists(directory) || !fs::is_directory(directory))
	{
		m_lastError =
			"Failed to load " + readableName + " animation: folder does not exist:\n" +
			directoryPath;
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
		m_lastError =
			"Failed to load " + readableName + " animation: no PNG frames found in:\n" +
			directoryPath;
		return false;
	}

	animation.frames.reserve(framePaths.size());

	for (const fs::path& framePath : framePaths)
	{
		sf::Texture texture;

		if (!texture.loadFromFile(framePath.string()))
		{
			m_lastError =
				"Failed to load " + readableName + " animation frame:\n" +
				framePath.string();
			return false;
		}

		animation.frames.push_back(std::move(texture));
	}

	return true;
}

void GAME1_Player::update(float deltaTime, GAME1_Level& level)
{
	if (ArcadeInput::isBackHeld())
	{
		stopMusic();
	}

	if (m_gameOver)
	{
		GAME1_SurfersQuestAudio::playDeath();

		if (ArcadeInput::isConfirmHeld())
		{
			resetGame();
		}
		else
		{
			setAnimationState(AnimationState::Idle);
			updateAnimation(deltaTime);
		}

		return;
	}

	GAME1_SurfersQuestAudio::playGameplay();

	if (m_respawning)
	{
		setAnimationState(AnimationState::Idle);
		updateRespawn(deltaTime);
		updateAnimation(deltaTime);
		return;
	}

	m_previousPosition = m_position;

	m_damageCooldownTimer = std::max(0.f, m_damageCooldownTimer - deltaTime);
	m_dropThroughTimer = std::max(0.f, m_dropThroughTimer - deltaTime);
	m_wallJumpControlLockTimer = std::max(0.f, m_wallJumpControlLockTimer - deltaTime);

	m_touchingWallLeft = false;
	m_touchingWallRight = false;

	updateWallGrabState(level);

	handleInput(deltaTime);
	applyGravity(deltaTime);

	moveHorizontal(deltaTime, level);
	updateWallGrabState(level);

	moveVertical(deltaTime, level);
	updateWallGrabState(level);

	checkSpikeTrapCollisions(level);

	updateAnimationState();
	updateAnimation(deltaTime);
	updateMovementSounds(deltaTime, level);

	if (m_position.y > 1200.f)
	{
		loseLifeAndRespawn();
	}
}

void GAME1_Player::handleInput(float deltaTime)
{
	m_horizontalInputHeld = false;

	const bool leftHeld = ArcadeInput::isMoveLeftHeld();
	const bool rightHeld = ArcadeInput::isMoveRightHeld();

	const bool dropThroughHeld = isDropThroughHeld();
	const bool phasePressedThisFrame = dropThroughHeld && !m_phaseHeldLastFrame;

	if (dropThroughHeld && m_onGround)
	{
		m_dropThroughTimer = m_dropThroughDuration;

		if (phasePressedThisFrame && m_groundedOnOneWayPlatform)
		{
			GAME1_SurfersQuestAudio::playPlayerPhase();
		}
	}

	const bool wallJumpAvailable =
		!m_onGround &&
		(m_wallGrabActive || m_touchingWallLeft || m_touchingWallRight);

	if (m_wallJumpControlLockTimer <= 0.f)
	{
		float targetVelocityX = 0.f;

		if (leftHeld && !rightHeld)
		{
			targetVelocityX = -m_moveSpeed;
			m_facingDirection = FacingDirection::Left;
			m_horizontalInputHeld = true;
		}
		else if (rightHeld && !leftHeld)
		{
			targetVelocityX = m_moveSpeed;
			m_facingDirection = FacingDirection::Right;
			m_horizontalInputHeld = true;
		}
		else
		{
			targetVelocityX = 0.f;
			m_horizontalInputHeld = false;
		}

		const float accelerationPerSecond =
			m_moveSpeed / std::max(0.001f, m_momentumBuildTime);

		const float noInputFrictionPerSecond =
			m_moveSpeed / std::max(0.001f, m_frictionStopTime);

		if (m_horizontalInputHeld)
		{
			m_velocity.x = moveTowards(
				m_velocity.x,
				targetVelocityX,
				accelerationPerSecond * deltaTime
			);
		}
		else
		{
			m_velocity.x = moveTowards(
				m_velocity.x,
				0.f,
				noInputFrictionPerSecond * deltaTime
			);

			if (std::abs(m_velocity.x) < 0.01f)
			{
				m_velocity.x = 0.f;
			}
		}
	}

	const bool technicallyGrounded =
		m_onGround ||
		m_wallGrabActive ||
		m_touchingWallLeft ||
		m_touchingWallRight;

	if (technicallyGrounded)
	{
		m_coyoteTimer = m_coyoteTime;
		m_canDoubleJump = true;
	}
	else
	{
		m_coyoteTimer = std::max(0.f, m_coyoteTimer - deltaTime);
	}

	m_jumpBufferTimer = std::max(0.f, m_jumpBufferTimer - deltaTime);

	const bool jumpHeld = ArcadeInput::isPrimaryHeld();
	const bool jumpPressedThisFrame = jumpHeld && !m_jumpHeldLastFrame;
	const bool jumpReleasedThisFrame = !jumpHeld && m_jumpHeldLastFrame;

	if (jumpPressedThisFrame)
	{
		m_jumpBufferTimer = m_jumpBufferTime;
	}

	if (jumpReleasedThisFrame &&
		m_variableJumpActive &&
		m_velocity.y < 0.f)
	{
		m_releasedJumpGravityActive = true;
	}

	if (m_velocity.y >= 0.f)
	{
		m_variableJumpActive = false;
		m_releasedJumpGravityActive = false;
	}

	if (m_jumpBufferTimer > 0.f)
	{
		if (wallJumpAvailable)
		{
			performWallJump();
		}
		else if (m_onGround || m_coyoteTimer > 0.f)
		{
			performGroundJump();
		}
		else if (m_canDoubleJump)
		{
			performDoubleJump();
		}
	}

	m_jumpHeldLastFrame = jumpHeld;
	m_phaseHeldLastFrame = dropThroughHeld;
}

void GAME1_Player::applyGravity(float deltaTime)
{
	float gravityMultiplier = 1.f;

	if (isWithinApexGravityWindow())
	{
		gravityMultiplier *= m_apexGravityMultiplier;
	}

	if (m_variableJumpActive &&
		m_releasedJumpGravityActive &&
		m_velocity.y < 0.f)
	{
		gravityMultiplier *= m_releasedJumpGravityMultiplier;
	}

	m_velocity.y += m_gravity * gravityMultiplier * deltaTime;

	if (m_velocity.y >= 0.f)
	{
		m_variableJumpActive = false;
		m_releasedJumpGravityActive = false;
	}
}

void GAME1_Player::performGroundJump()
{
	GAME1_SurfersQuestAudio::playPlayerJump();
	m_velocity.y = -m_jumpSpeed;
	m_onGround = false;
	m_wallGrabActive = false;

	m_coyoteTimer = 0.f;
	m_jumpBufferTimer = 0.f;

	m_canDoubleJump = true;
	m_doubleJumpAnimationPlaying = false;

	m_variableJumpActive = true;
	m_releasedJumpGravityActive = false;

	setAnimationState(AnimationState::Jump);
}

void GAME1_Player::performWallJump()
{
	GAME1_SurfersQuestAudio::playPlayerJump();
	m_velocity.y = -m_jumpSpeed;

	if (m_touchingWallLeft && !m_touchingWallRight)
	{
		m_position.x += m_wallJumpNudgeDistance;
		m_velocity.x = m_wallJumpHorizontalSpeed;
		m_facingDirection = FacingDirection::Right;
	}
	else if (m_touchingWallRight && !m_touchingWallLeft)
	{
		m_position.x -= m_wallJumpNudgeDistance;
		m_velocity.x = -m_wallJumpHorizontalSpeed;
		m_facingDirection = FacingDirection::Left;
	}
	else
	{
		if (m_facingDirection == FacingDirection::Left)
		{
			m_position.x += m_wallJumpNudgeDistance;
			m_velocity.x = m_wallJumpHorizontalSpeed;
			m_facingDirection = FacingDirection::Right;
		}
		else
		{
			m_position.x -= m_wallJumpNudgeDistance;
			m_velocity.x = -m_wallJumpHorizontalSpeed;
			m_facingDirection = FacingDirection::Left;
		}
	}

	m_onGround = false;
	m_wallGrabActive = false;
	m_touchingWallLeft = false;
	m_touchingWallRight = false;

	m_wallJumpControlLockTimer = m_wallJumpControlLockDuration;

	m_coyoteTimer = 0.f;
	m_jumpBufferTimer = 0.f;

	m_canDoubleJump = true;
	m_doubleJumpAnimationPlaying = false;

	m_variableJumpActive = true;
	m_releasedJumpGravityActive = false;

	setAnimationState(AnimationState::Jump);
}

void GAME1_Player::performDoubleJump()
{
	GAME1_SurfersQuestAudio::playPlayerDoubleJump();
	m_velocity.y = -m_jumpSpeed;
	m_onGround = false;
	m_wallGrabActive = false;

	m_coyoteTimer = 0.f;
	m_jumpBufferTimer = 0.f;

	m_canDoubleJump = false;
	m_doubleJumpAnimationPlaying = true;

	m_variableJumpActive = false;
	m_releasedJumpGravityActive = false;

	setAnimationState(AnimationState::DoubleJump);
}

void GAME1_Player::moveHorizontal(float deltaTime, GAME1_Level& level)
{
	m_position.x += m_velocity.x * deltaTime;

	sf::FloatRect bounds = getBounds();

	const int topTile = GetTopTile(bounds);
	const int bottomTile = GetBottomTile(bounds);

	if (m_velocity.x > 0.f)
	{
		const int rightTile = GetRightTile(bounds);

		for (int row = topTile; row <= bottomTile; ++row)
		{
			if (level.isSolidTile(rightTile, row))
			{
				m_position.x =
					static_cast<float>(rightTile * GAME1_Level::TileSize) -
					bounds.size.x;

				m_velocity.x = 0.f;
				m_touchingWallRight = true;
				break;
			}
		}
	}
	else if (m_velocity.x < 0.f)
	{
		const int leftTile = GetLeftTile(bounds);

		for (int row = topTile; row <= bottomTile; ++row)
		{
			if (level.isSolidTile(leftTile, row))
			{
				m_position.x =
					static_cast<float>((leftTile + 1) * GAME1_Level::TileSize);

				m_velocity.x = 0.f;
				m_touchingWallLeft = true;
				break;
			}
		}
	}
}

void GAME1_Player::updateWallGrabState(GAME1_Level& level)
{
	m_wallGrabActive = false;

	bool touchingLeft = false;
	bool touchingRight = false;
	detectWallContact(level, touchingLeft, touchingRight);

	m_touchingWallLeft = m_touchingWallLeft || touchingLeft;
	m_touchingWallRight = m_touchingWallRight || touchingRight;

	if (m_onGround)
		return;

	if (m_velocity.y <= 0.f)
		return;

	if (!m_touchingWallLeft && !m_touchingWallRight)
		return;

	m_wallGrabActive = true;

	m_coyoteTimer = m_coyoteTime;
	m_canDoubleJump = true;
	m_doubleJumpAnimationPlaying = false;

	const float wallGrabMaxFallSpeed =
		std::max(1.f, m_wallGrabBaseFallSpeed * m_wallGrabFallSpeedMultiplier);

	if (m_velocity.y > wallGrabMaxFallSpeed)
	{
		m_velocity.y = wallGrabMaxFallSpeed;
	}

	if (m_touchingWallLeft && !m_touchingWallRight)
	{
		m_facingDirection = FacingDirection::Left;
	}
	else if (m_touchingWallRight && !m_touchingWallLeft)
	{
		m_facingDirection = FacingDirection::Right;
	}
}

bool GAME1_Player::detectWallContact(GAME1_Level& level, bool& touchingLeft, bool& touchingRight) const
{
	touchingLeft = false;
	touchingRight = false;

	const sf::FloatRect bounds = getBounds();

	const int topTile = GetTopTile(bounds);
	const int bottomTile = GetBottomTile(bounds);

	const int leftProbeTile =
		static_cast<int>(std::floor((bounds.position.x - 2.f) / static_cast<float>(GAME1_Level::TileSize)));

	const int rightProbeTile =
		static_cast<int>(std::floor((bounds.position.x + bounds.size.x + 2.f) / static_cast<float>(GAME1_Level::TileSize)));

	for (int row = topTile; row <= bottomTile; ++row)
	{
		if (level.isSolidTile(leftProbeTile, row))
		{
			touchingLeft = true;
		}

		if (level.isSolidTile(rightProbeTile, row))
		{
			touchingRight = true;
		}
	}

	return touchingLeft || touchingRight;
}

void GAME1_Player::moveVertical(float deltaTime, GAME1_Level& level)
{
	m_onGround = false;
	m_groundedOnOneWayPlatform = false;

	m_position.y += m_velocity.y * deltaTime;

	sf::FloatRect bounds = getBounds();

	const int leftTile = GetLeftTile(bounds);
	const int rightTile = GetRightTile(bounds);

	if (m_velocity.y > 0.f)
	{
		const int bottomTile = GetBottomTile(bounds);
		const float previousBottom = m_previousPosition.y + bounds.size.y;
		const bool dropThroughActive = m_dropThroughTimer > 0.f || isDropThroughHeld();

		for (int col = leftTile; col <= rightTile; ++col)
		{
			const bool solidTile = level.isSolidTile(col, bottomTile);

			bool oneWayPlatformCollision = false;

			if (!solidTile && level.isOneWayPlatformTile(col, bottomTile))
			{
				const float platformTop = static_cast<float>(bottomTile * GAME1_Level::TileSize);
				const bool cameFromAbove = previousBottom <= platformTop + 4.f;
				oneWayPlatformCollision = cameFromAbove && !dropThroughActive;
			}

			if (solidTile || oneWayPlatformCollision)
			{
				m_position.y =
					static_cast<float>(bottomTile * GAME1_Level::TileSize) -
					bounds.size.y;

				m_velocity.y = 0.f;
				m_onGround = true;
				m_wallGrabActive = false;
				m_groundedOnOneWayPlatform = oneWayPlatformCollision;

				m_coyoteTimer = m_coyoteTime;
				m_canDoubleJump = true;
				m_doubleJumpAnimationPlaying = false;
				m_variableJumpActive = false;
				m_releasedJumpGravityActive = false;

				break;
			}
		}
	}
	else if (m_velocity.y < 0.f)
	{
		const int topTile = GetTopTile(bounds);

		for (int col = leftTile; col <= rightTile; ++col)
		{
			if (level.isSolidTile(col, topTile))
			{
				m_position.y =
					static_cast<float>((topTile + 1) * GAME1_Level::TileSize);

				m_velocity.y = 0.f;
				m_coyoteTimer = 0.f;
				m_variableJumpActive = false;
				m_releasedJumpGravityActive = false;

				break;
			}
		}
	}
}

void GAME1_Player::checkSpikeTrapCollisions(GAME1_Level& level)
{
	if (m_damageCooldownTimer > 0.f)
		return;

	const sf::FloatRect playerBounds = getBounds();

	const int leftTile = GetLeftTile(playerBounds);
	const int rightTile = GetRightTile(playerBounds);
	const int topTile = GetTopTile(playerBounds);
	const int bottomTile = GetBottomTile(playerBounds);

	for (int row = topTile; row <= bottomTile; ++row)
	{
		for (int col = leftTile; col <= rightTile; ++col)
		{
			if (!level.isSpikeTrapTile(col, row))
				continue;

			const sf::FloatRect spikeBounds(
				{
					static_cast<float>(col * GAME1_Level::TileSize),
					static_cast<float>(row * GAME1_Level::TileSize)
				},
				{
					static_cast<float>(GAME1_Level::TileSize),
					static_cast<float>(GAME1_Level::TileSize)
				}
			);

			if (rectsIntersect(playerBounds, spikeBounds))
			{
				takeSpikeDamage(spikeBounds);
				return;
			}
		}
	}
}

void GAME1_Player::takeSpikeDamage(const sf::FloatRect& spikeBounds)
{
	if (m_damageCooldownTimer > 0.f)
		return;

	playDamageOrDeathSoundForDamage(m_spikeDamage);
	m_health = std::max(0, m_health - m_spikeDamage);
	m_damageCooldownTimer = m_damageCooldownDuration;

	startHitAnimation();
	applyKnockbackFromTile(spikeBounds);

	if (m_health <= 0)
	{
		loseLifeAndRespawn();
	}
}

void GAME1_Player::updateMovementSounds(float deltaTime, const GAME1_Level& level)
{
	if (m_gameOver || m_respawning)
	{
		resetMovementSoundTimers();
		return;
	}

	const bool movingOnGround =
		m_onGround &&
		!m_wallGrabActive &&
		std::abs(m_velocity.x) > 35.f;

	if (movingOnGround)
	{
		m_footstepTimer -= deltaTime;

		if (m_footstepTimer <= 0.f)
		{
			const sf::FloatRect bounds = getBounds();
			const sf::Vector2f footPosition(
				bounds.position.x + bounds.size.x * 0.5f,
				bounds.position.y + bounds.size.y + 2.f);

			GAME1_SurfersQuestAudio::playFootstep(level.getSurfaceTagAtWorldPosition(footPosition));
			m_footstepTimer = m_footstepInterval;
		}
	}
	else
	{
		m_footstepTimer = 0.f;
	}

	const bool activelyWallgrabbing =
		m_wallGrabActive &&
		std::abs(m_velocity.y) > 2.f;

	if (activelyWallgrabbing)
	{
		m_wallgrabSoundTimer -= deltaTime;

		if (m_wallgrabSoundTimer <= 0.f)
		{
			GAME1_SurfersQuestAudio::playWallgrab();
			m_wallgrabSoundTimer = m_wallgrabSoundInterval;
		}
	}
	else
	{
		m_wallgrabSoundTimer = 0.f;
	}
}

void GAME1_Player::resetMovementSoundTimers()
{
	m_footstepTimer = 0.f;
	m_wallgrabSoundTimer = 0.f;
}

void GAME1_Player::playDamageOrDeathSoundForDamage(int damage) const
{
	if (wouldDamageCauseFinalDeath(damage))
	{
		GAME1_SurfersQuestAudio::playPlayerDeath();
	}
	else
	{
		GAME1_SurfersQuestAudio::playPlayerDamage();
	}
}

bool GAME1_Player::wouldDamageCauseFinalDeath(int damage) const
{
	return m_health - std::max(0, damage) <= 0 && m_lives <= 1;
}

void GAME1_Player::startHitAnimation()
{
	m_hitAnimationPlaying = true;
	setAnimationState(AnimationState::Hit);
}

void GAME1_Player::applyKnockbackFromTile(const sf::FloatRect& tileBounds)
{
	const sf::FloatRect playerBounds = getBounds();

	const sf::Vector2f playerCenter(
		playerBounds.position.x + playerBounds.size.x * 0.5f,
		playerBounds.position.y + playerBounds.size.y * 0.5f);

	const sf::Vector2f tileCenter(
		tileBounds.position.x + tileBounds.size.x * 0.5f,
		tileBounds.position.y + tileBounds.size.y * 0.5f);

	sf::Vector2f direction(
		playerCenter.x - tileCenter.x,
		playerCenter.y - tileCenter.y);

	if (std::abs(m_velocity.x) > std::abs(m_velocity.y) && std::abs(m_velocity.x) > 5.f)
	{
		direction = { -SignNonZero(m_velocity.x), 0.f };
	}
	else if (std::abs(m_velocity.y) > 5.f)
	{
		direction = { 0.f, -SignNonZero(m_velocity.y) };
	}

	const float length = std::sqrt(direction.x * direction.x + direction.y * direction.y);

	if (length > 0.001f)
	{
		direction.x /= length;
		direction.y /= length;
	}
	else
	{
		direction = {
			m_facingDirection == FacingDirection::Right ? -1.f : 1.f,
			-0.5f
		};
	}

	if (std::abs(direction.x) < 0.15f)
	{
		direction.x = 0.f;
	}

	m_velocity.x = direction.x * m_spikeKnockbackHorizontal;

	if (direction.y < 0.f || std::abs(direction.y) < 0.15f)
	{
		m_velocity.y = -m_spikeKnockbackVertical;
	}
	else
	{
		m_velocity.y = m_spikeKnockbackVertical * 0.45f;
	}

	m_onGround = false;
	m_wallGrabActive = false;
	m_coyoteTimer = 0.f;
	m_variableJumpActive = false;
	m_releasedJumpGravityActive = false;
}

void GAME1_Player::updateAnimationState()
{
	if (m_hitAnimationPlaying)
	{
		setAnimationState(AnimationState::Hit);
		return;
	}

	if (m_wallGrabActive && !m_wallGrabAnimation.frames.empty())
	{
		setAnimationState(AnimationState::WallGrab);
		return;
	}

	if (!m_onGround)
	{
		if (m_doubleJumpAnimationPlaying &&
			m_velocity.y < 0.f &&
			!m_doubleJumpAnimation.frames.empty())
		{
			setAnimationState(AnimationState::DoubleJump);
			return;
		}

		if (m_velocity.y < 0.f)
		{
			setAnimationState(AnimationState::Jump);
		}
		else
		{
			setAnimationState(AnimationState::Fall);
		}

		return;
	}

	m_doubleJumpAnimationPlaying = false;
	m_wallGrabActive = false;

	if (std::abs(m_velocity.x) > 5.f)
	{
		setAnimationState(AnimationState::Run);
	}
	else
	{
		setAnimationState(AnimationState::Idle);
	}
}

void GAME1_Player::setAnimationState(AnimationState newState)
{
	if (m_animationState == newState)
		return;

	m_animationState = newState;
	m_animationTimer = 0.f;
	m_currentFrameIndex = 0;
}

void GAME1_Player::updateAnimation(float deltaTime)
{
	const AnimationSet& animation = getCurrentAnimationSet();

	if (animation.frames.empty())
		return;

	if (m_hitAnimationPlaying)
	{
		m_animationTimer += deltaTime;

		while (m_animationTimer >= animation.frameDuration)
		{
			m_animationTimer -= animation.frameDuration;

			if (m_currentFrameIndex + 1 < animation.frames.size())
			{
				++m_currentFrameIndex;
			}
			else
			{
				m_hitAnimationPlaying = false;
				m_currentFrameIndex = 0;
				updateAnimationState();
				break;
			}
		}

		return;
	}

	if (m_animationState == AnimationState::DoubleJump && m_doubleJumpAnimationPlaying)
	{
		m_animationTimer += deltaTime;

		while (m_animationTimer >= animation.frameDuration)
		{
			m_animationTimer -= animation.frameDuration;

			if (m_currentFrameIndex + 1 < animation.frames.size())
			{
				++m_currentFrameIndex;
			}
			else
			{
				m_doubleJumpAnimationPlaying = false;
				m_currentFrameIndex = 0;
				updateAnimationState();
				break;
			}
		}

		return;
	}

	if (animation.frames.size() <= 1)
		return;

	m_animationTimer += deltaTime;

	while (m_animationTimer >= animation.frameDuration)
	{
		m_animationTimer -= animation.frameDuration;
		m_currentFrameIndex = (m_currentFrameIndex + 1) % animation.frames.size();
	}
}

const GAME1_Player::AnimationSet& GAME1_Player::getCurrentAnimationSet() const
{
	if (m_hitAnimationPlaying && !m_hitAnimation.frames.empty())
		return m_hitAnimation;

	switch (m_animationState)
	{
	case AnimationState::Run:
		if (!m_runAnimation.frames.empty())
			return m_runAnimation;
		break;

	case AnimationState::Jump:
		if (!m_jumpAnimation.frames.empty())
			return m_jumpAnimation;
		break;

	case AnimationState::DoubleJump:
		if (!m_doubleJumpAnimation.frames.empty())
			return m_doubleJumpAnimation;
		break;

	case AnimationState::Fall:
		if (!m_fallAnimation.frames.empty())
			return m_fallAnimation;
		break;

	case AnimationState::WallGrab:
		if (!m_wallGrabAnimation.frames.empty())
			return m_wallGrabAnimation;
		break;

	case AnimationState::Hit:
		if (!m_hitAnimation.frames.empty())
			return m_hitAnimation;
		break;

	case AnimationState::Idle:
	default:
		break;
	}

	return m_idleAnimation;
}

const sf::Texture* GAME1_Player::getCurrentTexture() const
{
	const AnimationSet& animation = getCurrentAnimationSet();

	if (animation.frames.empty())
		return nullptr;

	return &animation.frames[m_currentFrameIndex % animation.frames.size()];
}

void GAME1_Player::draw(sf::RenderTarget& target) const
{
	const sf::Texture* texture = getCurrentTexture();

	if (texture != nullptr)
	{
		sf::Sprite sprite(*texture);

		const sf::FloatRect localBounds = sprite.getLocalBounds();

		if (localBounds.size.x > 0.f && localBounds.size.y > 0.f)
		{
			const float scaleX = m_drawWidth / localBounds.size.x;
			const float scaleY = m_drawHeight / localBounds.size.y;

			sprite.setScale({
				m_facingDirection == FacingDirection::Left ? -scaleX : scaleX,
				scaleY
				});

			sprite.setPosition({
				m_facingDirection == FacingDirection::Left ? m_position.x + m_drawWidth : m_position.x,
				m_position.y
				});

			target.draw(sprite);
		}
	}

	if (m_hasUiFont)
	{
		const sf::View view = target.getView();
		const sf::Vector2f viewSize = view.getSize();
		const sf::Vector2f viewCenter = view.getCenter();

		const sf::Vector2f topLeft(
			viewCenter.x - viewSize.x * 0.5f,
			viewCenter.y - viewSize.y * 0.5f);

		sf::Text healthText(m_uiFont);
		healthText.setString("Health: " + std::to_string(m_health) + " / " + std::to_string(m_maxHealth));
		healthText.setCharacterSize(22);
		healthText.setFillColor(sf::Color::White);
		healthText.setOutlineColor(sf::Color::Black);
		healthText.setOutlineThickness(2.f);
		healthText.setPosition({ topLeft.x + 18.f, topLeft.y + 16.f });

		target.draw(healthText);

		sf::Text livesText(m_uiFont);
		livesText.setString("Lives: " + std::to_string(m_lives) + " / " + std::to_string(m_maxLives));
		livesText.setCharacterSize(22);
		livesText.setFillColor(sf::Color::White);
		livesText.setOutlineColor(sf::Color::Black);
		livesText.setOutlineThickness(2.f);
		livesText.setPosition({ topLeft.x + 18.f, topLeft.y + 44.f });

		target.draw(livesText);

		if (m_gameOver)
		{
			const sf::FloatRect popupRect(
				{ topLeft.x + viewSize.x * 0.5f - 245.f, topLeft.y + viewSize.y * 0.5f - 82.f },
				{ 490.f, 164.f });

			sf::RectangleShape popupBox;
			popupBox.setPosition(popupRect.position);
			popupBox.setSize(popupRect.size);
			popupBox.setFillColor(sf::Color(130, 45, 45, 230));
			popupBox.setOutlineColor(sf::Color::White);
			popupBox.setOutlineThickness(3.f);
			target.draw(popupBox);

			auto drawCenteredText = [this, &target](const std::string& string, unsigned int size, sf::Vector2f position, sf::Color fill)
				{
					sf::Text text(m_uiFont);
					text.setString(string);
					text.setCharacterSize(size);
					text.setFillColor(fill);
					text.setOutlineColor(sf::Color::Black);
					text.setOutlineThickness(2.f);

					const sf::FloatRect bounds = text.getLocalBounds();
					text.setPosition({
						position.x - bounds.size.x * 0.5f - bounds.position.x,
						position.y - bounds.size.y * 0.5f - bounds.position.y
						});

					target.draw(text);
				};

			const float centerX = popupRect.position.x + popupRect.size.x * 0.5f;
			drawCenteredText("OUT OF LIVES", 34, { centerX, popupRect.position.y + 38.f }, sf::Color::White);
			drawCenteredText("ENTER  -  RESTART", 22, { centerX, popupRect.position.y + 91.f }, sf::Color(255, 230, 120));
			drawCenteredText("ESC  -  BACK TO MENU", 22, { centerX, popupRect.position.y + 127.f }, sf::Color(230, 230, 230));
		}
	}
}

sf::FloatRect GAME1_Player::getBounds() const
{
	return sf::FloatRect(
		{ m_position.x, m_position.y },
		{ m_drawWidth, m_drawHeight }
	);
}

void GAME1_Player::setSpawnPosition(sf::Vector2f spawnPosition)
{
	m_spawnPosition = spawnPosition;
}

sf::Vector2f GAME1_Player::getSpawnPosition() const
{
	return m_spawnPosition;
}

bool GAME1_Player::isRespawning() const
{
	return m_respawning;
}

int GAME1_Player::getRespawnCountdown() const
{
	if (!m_respawning)
		return 0;

	return std::max(0, static_cast<int>(std::ceil(m_respawnTimer)));
}

int GAME1_Player::getHealth() const
{
	return m_health;
}

int GAME1_Player::getMaxHealth() const
{
	return m_maxHealth;
}

sf::Vector2f GAME1_Player::getVelocity() const
{
	return m_velocity;
}

void GAME1_Player::bounceAfterEnemyStomp()
{
	m_velocity.y = -m_jumpSpeed * 0.62f;
	m_onGround = false;
	m_wallGrabActive = false;
	m_coyoteTimer = 0.f;
	m_variableJumpActive = true;
	m_releasedJumpGravityActive = false;
}

void GAME1_Player::takeEnemyDamage(const sf::FloatRect& enemyBounds, int damage)
{
	if (m_damageCooldownTimer > 0.f || m_gameOver)
		return;

	const int resolvedDamage = std::max(0, damage);
	playDamageOrDeathSoundForDamage(resolvedDamage);
	m_health = std::max(0, m_health - resolvedDamage);
	m_damageCooldownTimer = m_damageCooldownDuration;

	startHitAnimation();
	applyKnockbackFromTile(enemyBounds);

	if (m_health <= 0)
	{
		loseLifeAndRespawn();
	}
}

int GAME1_Player::getLives() const
{
	return m_lives;
}

int GAME1_Player::getMaxLives() const
{
	return m_maxLives;
}

bool GAME1_Player::isGameOver() const
{
	return m_gameOver;
}

void GAME1_Player::resetGame()
{
	m_gameOver = false;
	m_lives = m_maxLives;
	m_health = m_maxHealth;
	m_damageCooldownTimer = 0.f;
	m_hitAnimationPlaying = false;

	m_respawning = false;
	m_respawnTimer = 0.f;

	m_dropThroughTimer = 0.f;
	m_wallGrabActive = false;
	m_touchingWallLeft = false;
	m_touchingWallRight = false;
	m_wallJumpControlLockTimer = 0.f;
	m_groundedOnOneWayPlatform = false;

	m_position = m_spawnPosition;
	m_previousPosition = m_spawnPosition;
	m_velocity = { 0.f, 0.f };

	m_onGround = false;
	m_jumpHeldLastFrame = ArcadeInput::isPrimaryHeld();
	m_phaseHeldLastFrame = isDropThroughHeld();
	resetMovementSoundTimers();

	m_coyoteTimer = 0.f;
	m_jumpBufferTimer = 0.f;
	m_canDoubleJump = true;
	m_doubleJumpAnimationPlaying = false;
	m_variableJumpActive = false;
	m_releasedJumpGravityActive = false;

	setAnimationState(AnimationState::Idle);
	GAME1_SurfersQuestAudio::playGameplay();
}

void GAME1_Player::stopMusic()
{
	GAME1_SurfersQuestAudio::stopAll();
}

void GAME1_Player::loseLifeAndRespawn()
{
	if (m_gameOver)
		return;

	m_lives = std::max(0, m_lives - 1);

	if (m_lives <= 0)
	{
		beginGameOver();
		return;
	}

	startRespawn();
}

void GAME1_Player::beginGameOver()
{
	m_gameOver = true;
	m_respawning = false;
	m_respawnTimer = 0.f;
	m_health = 0;
	m_velocity = { 0.f, 0.f };
	m_damageCooldownTimer = 0.f;
	m_hitAnimationPlaying = false;
	m_wallGrabActive = false;
	m_touchingWallLeft = false;
	m_touchingWallRight = false;
	m_wallJumpControlLockTimer = 0.f;
	m_dropThroughTimer = 0.f;
	m_groundedOnOneWayPlatform = false;
	m_onGround = false;
	m_coyoteTimer = 0.f;
	m_jumpBufferTimer = 0.f;
	m_variableJumpActive = false;
	m_releasedJumpGravityActive = false;
	resetMovementSoundTimers();

	setAnimationState(AnimationState::Idle);
	GAME1_SurfersQuestAudio::playDeath();
}

void GAME1_Player::startRespawn()
{
	if (m_respawning)
		return;

	m_respawning = true;
	m_respawnTimer = m_respawnDuration;

	m_health = m_maxHealth;
	m_damageCooldownTimer = 0.f;
	m_hitAnimationPlaying = false;

	m_dropThroughTimer = 0.f;
	m_wallGrabActive = false;
	m_touchingWallLeft = false;
	m_touchingWallRight = false;
	m_wallJumpControlLockTimer = 0.f;
	m_groundedOnOneWayPlatform = false;

	m_velocity = { 0.f, 0.f };
	m_position = m_spawnPosition;
	m_previousPosition = m_spawnPosition;

	m_onGround = false;
	m_jumpHeldLastFrame = ArcadeInput::isPrimaryHeld();
	m_phaseHeldLastFrame = isDropThroughHeld();
	resetMovementSoundTimers();

	m_coyoteTimer = 0.f;
	m_jumpBufferTimer = 0.f;
	m_canDoubleJump = true;
	m_doubleJumpAnimationPlaying = false;
	m_variableJumpActive = false;
	m_releasedJumpGravityActive = false;

	setAnimationState(AnimationState::Idle);
}

void GAME1_Player::updateRespawn(float deltaTime)
{
	if (!m_respawning)
		return;

	m_respawnTimer -= deltaTime;

	if (m_respawnTimer <= 0.f)
	{
		m_respawning = false;
		m_respawnTimer = 0.f;

		m_health = m_maxHealth;
		m_damageCooldownTimer = 0.f;
		m_hitAnimationPlaying = false;

		m_dropThroughTimer = 0.f;
		m_wallGrabActive = false;
		m_touchingWallLeft = false;
		m_touchingWallRight = false;
		m_wallJumpControlLockTimer = 0.f;
		m_groundedOnOneWayPlatform = false;

		m_position = m_spawnPosition;
		m_previousPosition = m_spawnPosition;
		m_velocity = { 0.f, 0.f };

		m_onGround = false;
		m_jumpHeldLastFrame = ArcadeInput::isPrimaryHeld();
		m_phaseHeldLastFrame = isDropThroughHeld();
		resetMovementSoundTimers();

		m_coyoteTimer = 0.f;
		m_jumpBufferTimer = 0.f;
		m_canDoubleJump = true;
		m_doubleJumpAnimationPlaying = false;
		m_variableJumpActive = false;
		m_releasedJumpGravityActive = false;

		setAnimationState(AnimationState::Idle);
	}
}

bool GAME1_Player::isWithinApexGravityWindow() const
{
	if (m_gravity <= 0.f)
		return false;

	if (m_velocity.y >= 0.f)
		return m_velocity.y <= m_gravity * m_apexGravityTimeWindow;

	return std::abs(m_velocity.y) <= m_gravity * m_apexGravityTimeWindow;
}

bool GAME1_Player::isDropThroughHeld() const
{
	return ArcadeInput::isSecondaryHeld();
}

bool GAME1_Player::rectsIntersect(const sf::FloatRect& a, const sf::FloatRect& b)
{
	return a.position.x < b.position.x + b.size.x &&
		a.position.x + a.size.x > b.position.x &&
		a.position.y < b.position.y + b.size.y &&
		a.position.y + a.size.y > b.position.y;
}

float GAME1_Player::moveTowards(float current, float target, float maxDelta)
{
	if (std::abs(target - current) <= maxDelta)
		return target;

	if (target > current)
		return current + maxDelta;

	return current - maxDelta;
}

const std::string& GAME1_Player::getLastError() const
{
	return m_lastError;
}