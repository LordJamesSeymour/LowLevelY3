#include "GAME1_Player.h"

#include "GAME1_Level.h"

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

	m_idleAnimation.frameDuration = 0.035f;
	m_runAnimation.frameDuration = 0.035f;
	m_jumpAnimation.frameDuration = 0.035f;
	m_doubleJumpAnimation.frameDuration = 0.035f;
	m_fallAnimation.frameDuration = 0.035f;

	m_position = startPosition;
	m_spawnPosition = startPosition;
	m_velocity = { 0.f, 0.f };

	m_onGround = false;
	m_jumpHeldLastFrame = false;

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
	if (m_respawning)
	{
		setAnimationState(AnimationState::Idle);
		updateRespawn(deltaTime);
		updateAnimation(deltaTime);
		return;
	}

	handleInput(deltaTime);
	applyGravity(deltaTime);

	moveHorizontal(deltaTime, level);
	moveVertical(deltaTime, level);

	updateAnimationState();
	updateAnimation(deltaTime);

	if (m_position.y > 1200.f)
	{
		startRespawn();
	}
}

void GAME1_Player::handleInput(float deltaTime)
{
	m_horizontalInputHeld = false;

	const bool leftHeld =
		sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A) ||
		sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left);

	const bool rightHeld =
		sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D) ||
		sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right);

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

	if (m_onGround)
	{
		m_coyoteTimer = m_coyoteTime;
		m_canDoubleJump = true;
	}
	else
	{
		m_coyoteTimer = std::max(0.f, m_coyoteTimer - deltaTime);
	}

	m_jumpBufferTimer = std::max(0.f, m_jumpBufferTimer - deltaTime);

	const bool jumpHeld = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space);
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
		const bool canUseGroundOrCoyoteJump = m_onGround || m_coyoteTimer > 0.f;

		if (canUseGroundOrCoyoteJump)
		{
			performGroundJump();
		}
		else if (m_canDoubleJump)
		{
			performDoubleJump();
		}
	}

	m_jumpHeldLastFrame = jumpHeld;
}

void GAME1_Player::applyGravity(float deltaTime)
{
	float gravityMultiplier = 1.f;

	// Apex modifier:
	// Near the top of the jump arc, gravity is softened instead of boosting speed.
	// This gives the player a little extra air-control time without changing horizontal speed.
	if (isWithinApexGravityWindow())
	{
		gravityMultiplier *= m_apexGravityMultiplier;
	}

	// Variable jump height:
	// Releasing Space early still increases gravity while rising.
	// This stacks with the apex modifier if the release happens near the apex.
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
	m_velocity.y = -m_jumpSpeed;
	m_onGround = false;

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
	m_velocity.y = -m_jumpSpeed;
	m_onGround = false;

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
				break;
			}
		}
	}
}

void GAME1_Player::moveVertical(float deltaTime, GAME1_Level& level)
{
	m_onGround = false;

	m_position.y += m_velocity.y * deltaTime;

	sf::FloatRect bounds = getBounds();

	const int leftTile = GetLeftTile(bounds);
	const int rightTile = GetRightTile(bounds);

	if (m_velocity.y > 0.f)
	{
		const int bottomTile = GetBottomTile(bounds);

		for (int col = leftTile; col <= rightTile; ++col)
		{
			if (level.isSolidTile(col, bottomTile))
			{
				m_position.y =
					static_cast<float>(bottomTile * GAME1_Level::TileSize) -
					bounds.size.y;

				m_velocity.y = 0.f;
				m_onGround = true;

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

				if (level.isBreakTile(col, topTile))
				{
					level.breakTile(col, topTile);
				}

				break;
			}
		}
	}
}

void GAME1_Player::updateAnimationState()
{
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

	if (std::abs(m_velocity.x) > 8.f)
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

	if (m_animationState == AnimationState::DoubleJump && m_doubleJumpAnimationPlaying)
	{
		m_animationTimer += deltaTime;

		if (animation.frames.size() <= 1)
		{
			if (m_animationTimer >= animation.frameDuration)
			{
				m_animationTimer = 0.f;
				m_doubleJumpAnimationPlaying = false;

				if (!m_onGround && m_velocity.y < 0.f)
					setAnimationState(AnimationState::Jump);
				else if (!m_onGround)
					setAnimationState(AnimationState::Fall);
			}

			return;
		}

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

				if (!m_onGround && m_velocity.y < 0.f)
					setAnimationState(AnimationState::Jump);
				else if (!m_onGround)
					setAnimationState(AnimationState::Fall);

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

void GAME1_Player::draw(sf::RenderTarget& target) const
{
	const sf::Texture* texture = getCurrentTexture();

	if (texture == nullptr)
		return;

	sf::Sprite sprite(*texture);

	const sf::FloatRect localBounds = sprite.getLocalBounds();

	if (localBounds.size.x <= 0.f || localBounds.size.y <= 0.f)
		return;

	const float scaleX = m_drawWidth / localBounds.size.x;
	const float scaleY = m_drawHeight / localBounds.size.y;

	if (m_facingDirection == FacingDirection::Left)
	{
		sprite.setScale({ -scaleX, scaleY });

		sprite.setPosition({
			m_position.x + m_drawWidth,
			m_position.y
			});
	}
	else
	{
		sprite.setScale({ scaleX, scaleY });
		sprite.setPosition(m_position);
	}

	target.draw(sprite);
}

sf::FloatRect GAME1_Player::getBounds() const
{
	return sf::FloatRect(
		m_position,
		{ m_drawWidth, m_drawHeight }
	);
}

void GAME1_Player::startRespawn()
{
	if (m_respawning)
		return;

	m_respawning = true;
	m_respawnTimer = m_respawnDuration;
	m_velocity = { 0.f, 0.f };
	m_horizontalInputHeld = false;
	m_coyoteTimer = 0.f;
	m_canDoubleJump = true;
	m_doubleJumpAnimationPlaying = false;
	m_variableJumpActive = false;
	m_releasedJumpGravityActive = false;

	setAnimationState(AnimationState::Idle);
}

void GAME1_Player::updateRespawn(float deltaTime)
{
	m_respawnTimer -= deltaTime;

	if (m_respawnTimer <= 0.f)
	{
		m_respawnTimer = 0.f;
		m_respawning = false;
		m_position = m_spawnPosition;
		m_velocity = { 0.f, 0.f };
		m_onGround = false;
		m_horizontalInputHeld = false;
		m_coyoteTimer = 0.f;
		m_canDoubleJump = true;
		m_doubleJumpAnimationPlaying = false;
		m_variableJumpActive = false;
		m_releasedJumpGravityActive = false;

		setAnimationState(AnimationState::Idle);
	}
}

bool GAME1_Player::isRespawning() const
{
	return m_respawning;
}

int GAME1_Player::getRespawnCountdown() const
{
	return static_cast<int>(std::ceil(std::max(0.f, m_respawnTimer)));
}

const GAME1_Player::AnimationSet& GAME1_Player::getCurrentAnimationSet() const
{
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

const std::string& GAME1_Player::getLastError() const
{
	return m_lastError;
}

bool GAME1_Player::isWithinApexGravityWindow() const
{
	if (m_onGround)
		return false;

	const float verticalSpeedWindow =
		m_gravity * std::max(0.f, m_apexGravityTimeWindow);

	return std::abs(m_velocity.y) <= verticalSpeedWindow;
}

float GAME1_Player::moveTowards(float current, float target, float maxDelta)
{
	if (std::abs(target - current) <= maxDelta)
		return target;

	return current + (target > current ? maxDelta : -maxDelta);
}