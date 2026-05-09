#include "BombermanPlayer.h"

#include "BombermanLevel.h"

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
			--start;

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
}

bool BombermanPlayer::load(const std::string& playerDirectory)
{
	namespace fs = std::filesystem;

	m_lastError.clear();

	fs::path baseDirectory = fs::path(playerDirectory) / "Blue";

	// Safety fallback in case the caller already passes the Blue folder directly.
	if (!fs::exists(baseDirectory) || !fs::is_directory(baseDirectory))
	{
		baseDirectory = playerDirectory;
	}

	if (!loadAnimationFramesFromDirectory(
		m_frontAnimation,
		(baseDirectory / "Front").string(),
		"player front"))
	{
		return false;
	}

	if (!loadAnimationFramesFromDirectory(
		m_backAnimation,
		(baseDirectory / "Back").string(),
		"player back"))
	{
		return false;
	}

	if (!loadAnimationFramesFromDirectory(
		m_leftAnimation,
		(baseDirectory / "Left").string(),
		"player left"))
	{
		return false;
	}

	if (!loadAnimationFramesFromDirectory(
		m_rightAnimation,
		(baseDirectory / "Right").string(),
		"player right"))
	{
		return false;
	}

	m_facing = Direction::Front;
	m_animationTimer = 0.f;
	m_animationSequenceIndex = 0;

	return true;
}

bool BombermanPlayer::loadAnimationFramesFromDirectory(AnimationSet& animation,
	const std::string& directoryPath,
	const std::string& readableName)
{
	namespace fs = std::filesystem;

	animation.frames.clear();

	const fs::path directory(directoryPath);

	if (!fs::exists(directory) || !fs::is_directory(directory))
	{
		m_lastError = "Failed to load Bomberman " + readableName + " animation: folder does not exist: " + directoryPath;
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
		m_lastError = "Failed to load Bomberman " + readableName + " animation: no PNG files found in: " + directoryPath;
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

void BombermanPlayer::reset(BombermanGridPosition spawnPosition, const BombermanLevel& level)
{
	m_position = level.gridToWorldTopLeft(spawnPosition);

	m_alive = true;
	m_invincibilityTimer = 0.f;

	m_facing = Direction::Front;

	m_isMoving = false;
	m_wasMovingLastFrame = false;

	m_animationTimer = 0.f;
	m_animationSequenceIndex = 0;

	m_previousInputState = {};
}

void BombermanPlayer::update(float deltaTime,
	const BombermanLevel& level,
	const TileBlockedCallback& isTileBlocked)
{
	(void)level;

	if (m_invincibilityTimer > 0.f)
	{
		m_invincibilityTimer = std::max(0.f, m_invincibilityTimer - deltaTime);
	}

	if (!m_alive)
		return;

	const HeldInputState inputState = readInputState();
	const sf::Vector2f moveInput = resolveMovementInput(inputState);

	m_isMoving = moveInput.x != 0.f || moveInput.y != 0.f;

	if (m_isMoving)
	{
		const sf::Vector2f movement = moveInput * m_moveSpeed * deltaTime;
		tryMove(movement, isTileBlocked);
	}

	updateAnimation(deltaTime, m_isMoving);

	m_previousInputState = inputState;
	m_wasMovingLastFrame = m_isMoving;
}

BombermanPlayer::HeldInputState BombermanPlayer::readInputState() const
{
	HeldInputState input;

	input.up =
		sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W) ||
		sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up);

	input.down =
		sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S) ||
		sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Down);

	input.left =
		sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A) ||
		sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left);

	input.right =
		sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D) ||
		sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right);

	return input;
}

sf::Vector2f BombermanPlayer::resolveMovementInput(const HeldInputState& inputState)
{
	const bool upPressed = inputState.up && !m_previousInputState.up;
	const bool downPressed = inputState.down && !m_previousInputState.down;
	const bool leftPressed = inputState.left && !m_previousInputState.left;
	const bool rightPressed = inputState.right && !m_previousInputState.right;

	// New inputs immediately take priority. This fixes the old issue where
	// pressing A/D during vertical movement felt unresponsive.
	if (leftPressed)
	{
		setFacing(Direction::Left);
		return { -1.f, 0.f };
	}

	if (rightPressed)
	{
		setFacing(Direction::Right);
		return { 1.f, 0.f };
	}

	if (upPressed)
	{
		setFacing(Direction::Back);
		return { 0.f, -1.f };
	}

	if (downPressed)
	{
		setFacing(Direction::Front);
		return { 0.f, 1.f };
	}

	// If the current direction is still held, keep moving that way.
	if (isDirectionHeld(m_facing, inputState))
	{
		switch (m_facing)
		{
		case Direction::Left:
			return { -1.f, 0.f };

		case Direction::Right:
			return { 1.f, 0.f };

		case Direction::Back:
			return { 0.f, -1.f };

		case Direction::Front:
		default:
			return { 0.f, 1.f };
		}
	}

	// Fallback when multiple keys are already held before this frame.
	if (inputState.left)
	{
		setFacing(Direction::Left);
		return { -1.f, 0.f };
	}

	if (inputState.right)
	{
		setFacing(Direction::Right);
		return { 1.f, 0.f };
	}

	if (inputState.up)
	{
		setFacing(Direction::Back);
		return { 0.f, -1.f };
	}

	if (inputState.down)
	{
		setFacing(Direction::Front);
		return { 0.f, 1.f };
	}

	return { 0.f, 0.f };
}

bool BombermanPlayer::isDirectionHeld(Direction direction, const HeldInputState& inputState) const
{
	switch (direction)
	{
	case Direction::Front:
		return inputState.down;

	case Direction::Back:
		return inputState.up;

	case Direction::Left:
		return inputState.left;

	case Direction::Right:
		return inputState.right;
	}

	return false;
}

void BombermanPlayer::setFacing(Direction direction)
{
	if (m_facing == direction)
		return;

	m_facing = direction;
	m_animationTimer = 0.f;
	m_animationSequenceIndex = 0;
}

void BombermanPlayer::tryMove(sf::Vector2f movement, const TileBlockedCallback& isTileBlocked)
{
	if (tryMoveDirect(movement, isTileBlocked))
		return;

	tryMoveWithEdgeCorrection(movement, isTileBlocked);
}

bool BombermanPlayer::tryMoveDirect(sf::Vector2f movement, const TileBlockedCallback& isTileBlocked)
{
	const sf::Vector2f candidatePosition = m_position + movement;

	if (wouldCollideAt(candidatePosition, isTileBlocked))
		return false;

	m_position = candidatePosition;
	return true;
}

bool BombermanPlayer::tryMoveWithEdgeCorrection(sf::Vector2f movement, const TileBlockedCallback& isTileBlocked)
{
	if (movement.x == 0.f && movement.y == 0.f)
		return false;

	const bool movingHorizontally = std::abs(movement.x) > std::abs(movement.y);

	for (float amount = 1.f; amount <= m_edgeCorrectionDistance; amount += 1.f)
	{
		for (float sign : { -1.f, 1.f })
		{
			const sf::Vector2f correction = movingHorizontally
				? sf::Vector2f{ 0.f, sign * amount }
			: sf::Vector2f{ sign * amount, 0.f };

			const sf::Vector2f correctedStart = m_position + correction;
			const sf::Vector2f correctedEnd = correctedStart + movement;

			// Important:
			// Do not pull the player sideways unless the correction fully clears
			// both the starting position and the final movement.
			if (wouldCollideAt(correctedStart, isTileBlocked))
				continue;

			if (wouldCollideAt(correctedEnd, isTileBlocked))
				continue;

			m_position = correctedEnd;
			return true;
		}
	}

	return false;
}

bool BombermanPlayer::wouldCollideAt(sf::Vector2f position, const TileBlockedCallback& isTileBlocked) const
{
	const sf::FloatRect bounds = getCollisionBoundsAt(position);

	const int leftTile = static_cast<int>(std::floor(bounds.position.x / static_cast<float>(BombermanLevel::TileSize)));
	const int rightTile = static_cast<int>(std::floor((bounds.position.x + bounds.size.x - 0.1f) / static_cast<float>(BombermanLevel::TileSize)));
	const int topTile = static_cast<int>(std::floor(bounds.position.y / static_cast<float>(BombermanLevel::TileSize)));
	const int bottomTile = static_cast<int>(std::floor((bounds.position.y + bounds.size.y - 0.1f) / static_cast<float>(BombermanLevel::TileSize)));

	for (int row = topTile; row <= bottomTile; ++row)
	{
		for (int col = leftTile; col <= rightTile; ++col)
		{
			if (!isTileBlocked(col, row))
				continue;

			const sf::FloatRect tileBounds(
				{
					static_cast<float>(col * BombermanLevel::TileSize),
					static_cast<float>(row * BombermanLevel::TileSize)
				},
				{
					static_cast<float>(BombermanLevel::TileSize),
					static_cast<float>(BombermanLevel::TileSize)
				}
			);

			if (rectsIntersect(bounds, tileBounds))
				return true;
		}
	}

	return false;
}

void BombermanPlayer::updateAnimation(float deltaTime, bool isMoving)
{
	if (!isMoving)
	{
		m_animationTimer = 0.f;
		m_animationSequenceIndex = 0;
		return;
	}

	const AnimationSet& animation = getCurrentAnimation();

	if (animation.frames.size() <= 1)
		return;

	m_animationTimer += deltaTime;

	while (m_animationTimer >= m_animationFrameDuration)
	{
		m_animationTimer -= m_animationFrameDuration;
		++m_animationSequenceIndex;
	}
}

const BombermanPlayer::AnimationSet& BombermanPlayer::getCurrentAnimation() const
{
	switch (m_facing)
	{
	case Direction::Back:
		return m_backAnimation;

	case Direction::Left:
		return m_leftAnimation;

	case Direction::Right:
		return m_rightAnimation;

	case Direction::Front:
	default:
		return m_frontAnimation;
	}
}

std::size_t BombermanPlayer::getCurrentFrameIndex() const
{
	const AnimationSet& animation = getCurrentAnimation();

	if (animation.frames.empty())
		return 0;

	if (animation.frames.size() == 1)
		return 0;

	// Idle frame should be _1 when available.
	if (!m_isMoving)
	{
		return animation.frames.size() > 1 ? 1 : 0;
	}

	// Desired movement loop:
	// _1 > _2 > _1 > _0 > _1, repeat.
	if (animation.frames.size() >= 3)
	{
		static constexpr std::size_t sequence[] = { 1, 2, 1, 0, 1 };
		const std::size_t sequenceIndex = m_animationSequenceIndex % 5;
		return sequence[sequenceIndex];
	}

	// Fallback for 2-frame animations.
	static constexpr std::size_t twoFrameSequence[] = { 0, 1, 0 };
	return twoFrameSequence[m_animationSequenceIndex % 3];
}

void BombermanPlayer::draw(sf::RenderTarget& target) const
{
	if (!m_alive)
		return;

	if (m_invincibilityTimer > 0.f)
	{
		const int blinkPhase = static_cast<int>(m_invincibilityTimer * 14.f);

		if (blinkPhase % 2 != 0)
			return;
	}

	const AnimationSet& animation = getCurrentAnimation();

	if (animation.frames.empty())
		return;

	const sf::Texture& texture = animation.frames[getCurrentFrameIndex()];

	sf::Sprite sprite(texture);

	const sf::FloatRect localBounds = sprite.getLocalBounds();

	if (localBounds.size.x <= 0.f || localBounds.size.y <= 0.f)
		return;

	const float scaleX = static_cast<float>(BombermanLevel::TileSize) / localBounds.size.x;
	const float scaleY = static_cast<float>(BombermanLevel::TileSize) / localBounds.size.y;

	sprite.setScale({ scaleX, scaleY });
	sprite.setPosition(m_position);

	target.draw(sprite);
}

bool BombermanPlayer::isAlive() const
{
	return m_alive;
}

void BombermanPlayer::kill()
{
	m_alive = false;
}

void BombermanPlayer::beginInvincibility(float duration)
{
	m_invincibilityTimer = std::max(0.f, duration);
}

bool BombermanPlayer::isInvincible() const
{
	return m_invincibilityTimer > 0.f;
}

void BombermanPlayer::setMoveSpeed(float moveSpeed)
{
	m_moveSpeed = std::max(0.f, moveSpeed);
}

float BombermanPlayer::getMoveSpeed() const
{
	return m_moveSpeed;
}

BombermanGridPosition BombermanPlayer::getGridPosition(const BombermanLevel& level) const
{
	const sf::FloatRect bounds = getCollisionBounds();

	const sf::Vector2f center{
		bounds.position.x + bounds.size.x * 0.5f,
		bounds.position.y + bounds.size.y * 0.5f
	};

	return level.worldToGrid(center);
}

sf::FloatRect BombermanPlayer::getBounds() const
{
	return sf::FloatRect(
		m_position,
		{
			static_cast<float>(BombermanLevel::TileSize),
			static_cast<float>(BombermanLevel::TileSize)
		}
	);
}

sf::FloatRect BombermanPlayer::getCollisionBounds() const
{
	return getCollisionBoundsAt(m_position);
}

sf::FloatRect BombermanPlayer::getCollisionBoundsAt(sf::Vector2f position) const
{
	const float offset = (static_cast<float>(BombermanLevel::TileSize) - m_collisionSize) * 0.5f;

	return sf::FloatRect(
		{
			position.x + offset,
			position.y + offset
		},
		{
			m_collisionSize,
			m_collisionSize
		}
	);
}

bool BombermanPlayer::rectsIntersect(const sf::FloatRect& a, const sf::FloatRect& b) const
{
	return a.position.x < b.position.x + b.size.x &&
		a.position.x + a.size.x > b.position.x &&
		a.position.y < b.position.y + b.size.y &&
		a.position.y + a.size.y > b.position.y;
}

sf::Vector2f BombermanPlayer::gridToWorldTopLeft(BombermanGridPosition gridPosition) const
{
	return {
		static_cast<float>(gridPosition.col * BombermanLevel::TileSize),
		static_cast<float>(gridPosition.row * BombermanLevel::TileSize)
	};
}

const std::string& BombermanPlayer::getLastError() const
{
	return m_lastError;
}