#include "BombermanPlayer.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <filesystem>
#include <optional>

namespace
{
	sf::FloatRect MakeTileRect(int col, int row)
	{
		return sf::FloatRect(
			{
				static_cast<float>(col * BombermanLevel::TileSize),
				static_cast<float>(row * BombermanLevel::TileSize)
			},
			{
				static_cast<float>(BombermanLevel::TileSize),
				static_cast<float>(BombermanLevel::TileSize)
			}
		);
	}

	bool CircleIntersectsRect(sf::Vector2f circleCenter, float circleRadius, const sf::FloatRect& rect)
	{
		const float closestX = std::clamp(
			circleCenter.x,
			rect.position.x,
			rect.position.x + rect.size.x);

		const float closestY = std::clamp(
			circleCenter.y,
			rect.position.y,
			rect.position.y + rect.size.y);

		const float differenceX = circleCenter.x - closestX;
		const float differenceY = circleCenter.y - closestY;

		return (differenceX * differenceX + differenceY * differenceY) <= circleRadius * circleRadius;
	}

	float SignOrZero(float value)
	{
		if (value > 0.f)
			return 1.f;

		if (value < 0.f)
			return -1.f;

		return 0.f;
	}

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
}

bool BombermanPlayer::load(const std::string& playerDirectory)
{
	namespace fs = std::filesystem;

	m_lastError.clear();

	fs::path blueDirectory(playerDirectory);

	// Current expected call is:
	// assets/Game#0/Bomberman/Resources/Player
	//
	// New expected sprite location is:
	// assets/Game#0/Bomberman/Resources/Player/Blue
	if (blueDirectory.filename().string() != "Blue")
	{
		blueDirectory /= "Blue";
	}

	if (!loadAnimationFolder(m_frontAnimation, (blueDirectory / "Front").string(), "Blue player front animation"))
		return false;

	if (!loadAnimationFolder(m_backAnimation, (blueDirectory / "Back").string(), "Blue player back animation"))
		return false;

	if (!loadAnimationFolder(m_leftAnimation, (blueDirectory / "Left").string(), "Blue player left animation"))
		return false;

	if (!loadAnimationFolder(m_rightAnimation, (blueDirectory / "Right").string(), "Blue player right animation"))
		return false;

	m_facingDirection = BombermanDirection::Down;
	m_previousAnimationDirection = BombermanDirection::Down;

	const sf::Texture* startingTexture = getCurrentAnimationTexture();

	if (startingTexture == nullptr)
	{
		m_lastError = "Blue player has no valid starting animation frame.";
		return false;
	}

	m_sprite.emplace(*startingTexture);

	const sf::FloatRect localBounds = m_sprite->getLocalBounds();

	if (localBounds.size.x <= 0.f || localBounds.size.y <= 0.f)
	{
		m_lastError = "Blue player texture has invalid size.";
		return false;
	}

	m_sprite->setScale({
		static_cast<float>(BombermanLevel::TileSize) / localBounds.size.x,
		static_cast<float>(BombermanLevel::TileSize) / localBounds.size.y
		});

	return true;
}

bool BombermanPlayer::loadAnimationFolder(AnimationSet& animationSet,
	const std::string& directoryPath,
	const std::string& readableName)
{
	namespace fs = std::filesystem;

	animationSet.frames.clear();
	animationSet.movementSequence.clear();
	animationSet.idleFrameIndex = 0;
	animationSet.sequenceIndex = 0;
	animationSet.timer = 0.f;

	const fs::path directory(directoryPath);

	if (!fs::exists(directory) || !fs::is_directory(directory))
	{
		m_lastError = "Failed to load " + readableName + ": folder does not exist: " + directoryPath;
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
		m_lastError = "Failed to load " + readableName + ": no PNG files found in: " + directoryPath;
		return false;
	}

	std::vector<int> trailingNumbers;
	trailingNumbers.reserve(framePaths.size());

	animationSet.frames.reserve(framePaths.size());

	for (const fs::path& framePath : framePaths)
	{
		sf::Texture texture;

		if (!texture.loadFromFile(framePath.string()))
		{
			m_lastError = "Failed to load " + readableName + " frame: " + framePath.string();
			return false;
		}

		const std::optional<int> trailingNumber = ExtractTrailingNumber(framePath);
		trailingNumbers.push_back(trailingNumber.value_or(-1));

		animationSet.frames.push_back(std::move(texture));
	}

	buildMovementSequence(animationSet, trailingNumbers);

	return true;
}

void BombermanPlayer::buildMovementSequence(AnimationSet& animationSet,
	const std::vector<int>& trailingNumbers)
{
	auto findFrameByNumber = [&trailingNumbers](int wantedNumber) -> std::optional<std::size_t>
		{
			for (std::size_t i = 0; i < trailingNumbers.size(); ++i)
			{
				if (trailingNumbers[i] == wantedNumber)
					return i;
			}

			return std::nullopt;
		};

	const std::optional<std::size_t> frame0 = findFrameByNumber(0);
	const std::optional<std::size_t> frame1 = findFrameByNumber(1);
	const std::optional<std::size_t> frame2 = findFrameByNumber(2);

	if (frame1.has_value())
	{
		animationSet.idleFrameIndex = frame1.value();
	}
	else
	{
		animationSet.idleFrameIndex = 0;
	}

	if (frame0.has_value() && frame1.has_value() && frame2.has_value())
	{
		// Required movement cycle:
		// _1 -> _2 -> _1 -> _0 -> _1 -> repeat
		animationSet.movementSequence =
		{
			frame1.value(),
			frame2.value(),
			frame1.value(),
			frame0.value(),
			frame1.value()
		};
	}
	else
	{
		// Fallback for future animation folders with different frame names.
		// If the expected 0/1/2 naming is missing, cycle through all loaded frames.
		for (std::size_t i = 0; i < animationSet.frames.size(); ++i)
		{
			animationSet.movementSequence.push_back(i);
		}
	}

	if (animationSet.movementSequence.empty())
	{
		animationSet.movementSequence.push_back(animationSet.idleFrameIndex);
	}
}

void BombermanPlayer::reset(BombermanGridPosition spawnPosition, const BombermanLevel& level)
{
	m_alive = true;
	m_invincibilityTimer = 0.f;

	m_facingDirection = BombermanDirection::Down;
	m_previousAnimationDirection = BombermanDirection::Down;

	m_currentMoveInput = { 0.f, 0.f };
	m_movementKeyHeld = false;
	m_wasMovementKeyHeld = false;

	m_upHeldLastFrame = false;
	m_downHeldLastFrame = false;
	m_leftHeldLastFrame = false;
	m_rightHeldLastFrame = false;

	resetActiveAnimationToIdle();
	applyCurrentAnimationFrame();

	m_position = level.gridToWorldTopLeft(spawnPosition);

	if (m_sprite)
	{
		m_sprite->setPosition(m_position);
	}
}

void BombermanPlayer::update(float deltaTime,
	const BombermanLevel& level,
	const std::function<bool(int col, int row)>& isTileBlocked)
{
	(void)level;

	if (m_invincibilityTimer > 0.f)
	{
		m_invincibilityTimer = std::max(0.f, m_invincibilityTimer - deltaTime);
	}

	if (!m_sprite || !m_alive)
		return;

	refreshMovementInput();
	updateAnimation(deltaTime);

	const sf::Vector2f movement = m_currentMoveInput * m_moveSpeed * deltaTime;
	const sf::Vector2f nextPosition = m_position + movement;

	if (canFitAt(nextPosition, isTileBlocked))
	{
		m_position = nextPosition;
	}
	else
	{
		tryMoveWithEdgeCorrection(movement, isTileBlocked);
	}

	applyCurrentAnimationFrame();
	m_sprite->setPosition(m_position);
}

void BombermanPlayer::refreshMovementInput()
{
	const bool upHeld =
		sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W) ||
		sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up);

	const bool downHeld =
		sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S) ||
		sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Down);

	const bool leftHeld =
		sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A) ||
		sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left);

	const bool rightHeld =
		sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D) ||
		sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right);

	const bool upNew = upHeld && !m_upHeldLastFrame;
	const bool downNew = downHeld && !m_downHeldLastFrame;
	const bool leftNew = leftHeld && !m_leftHeldLastFrame;
	const bool rightNew = rightHeld && !m_rightHeldLastFrame;

	m_movementKeyHeld = upHeld || downHeld || leftHeld || rightHeld;

	bool changedDirectionThisFrame = false;

	auto chooseDirection = [this, &changedDirectionThisFrame](BombermanDirection direction)
		{
			m_facingDirection = direction;
			changedDirectionThisFrame = true;

			switch (direction)
			{
			case BombermanDirection::Up:
				m_currentMoveInput = { 0.f, -1.f };
				break;

			case BombermanDirection::Down:
				m_currentMoveInput = { 0.f, 1.f };
				break;

			case BombermanDirection::Left:
				m_currentMoveInput = { -1.f, 0.f };
				break;

			case BombermanDirection::Right:
				m_currentMoveInput = { 1.f, 0.f };
				break;
			}
		};

	if (upNew) chooseDirection(BombermanDirection::Up);
	if (downNew) chooseDirection(BombermanDirection::Down);
	if (leftNew) chooseDirection(BombermanDirection::Left);
	if (rightNew) chooseDirection(BombermanDirection::Right);

	if (!changedDirectionThisFrame)
	{
		if (!m_movementKeyHeld)
		{
			m_currentMoveInput = { 0.f, 0.f };
		}
		else
		{
			bool currentDirectionStillHeld = false;

			if (m_currentMoveInput.y < 0.f && upHeld)
				currentDirectionStillHeld = true;
			else if (m_currentMoveInput.y > 0.f && downHeld)
				currentDirectionStillHeld = true;
			else if (m_currentMoveInput.x < 0.f && leftHeld)
				currentDirectionStillHeld = true;
			else if (m_currentMoveInput.x > 0.f && rightHeld)
				currentDirectionStillHeld = true;

			if (!currentDirectionStillHeld)
			{
				if (rightHeld)
					chooseDirection(BombermanDirection::Right);
				else if (leftHeld)
					chooseDirection(BombermanDirection::Left);
				else if (downHeld)
					chooseDirection(BombermanDirection::Down);
				else if (upHeld)
					chooseDirection(BombermanDirection::Up);
			}
		}
	}

	m_upHeldLastFrame = upHeld;
	m_downHeldLastFrame = downHeld;
	m_leftHeldLastFrame = leftHeld;
	m_rightHeldLastFrame = rightHeld;
}

void BombermanPlayer::updateAnimation(float deltaTime)
{
	AnimationSet& animationSet = getActiveAnimationSet();

	const bool directionChanged = m_facingDirection != m_previousAnimationDirection;
	const bool movementJustStarted = m_movementKeyHeld && !m_wasMovementKeyHeld;
	const bool movementJustStopped = !m_movementKeyHeld && m_wasMovementKeyHeld;

	if (directionChanged || movementJustStarted || movementJustStopped)
	{
		animationSet.sequenceIndex = 0;
		animationSet.timer = 0.f;
	}

	if (m_movementKeyHeld)
	{
		if (animationSet.movementSequence.size() > 1)
		{
			animationSet.timer += deltaTime;

			while (animationSet.timer >= m_animationFrameDuration)
			{
				animationSet.timer -= m_animationFrameDuration;
				animationSet.sequenceIndex =
					(animationSet.sequenceIndex + 1) % animationSet.movementSequence.size();
			}
		}
	}
	else
	{
		animationSet.sequenceIndex = 0;
		animationSet.timer = 0.f;
	}

	m_previousAnimationDirection = m_facingDirection;
	m_wasMovementKeyHeld = m_movementKeyHeld;
}

void BombermanPlayer::applyCurrentAnimationFrame()
{
	if (!m_sprite)
		return;

	const sf::Texture* currentTexture = getCurrentAnimationTexture();

	if (currentTexture == nullptr)
		return;

	m_sprite->setTexture(*currentTexture, true);

	const sf::FloatRect localBounds = m_sprite->getLocalBounds();

	if (localBounds.size.x > 0.f && localBounds.size.y > 0.f)
	{
		m_sprite->setScale({
			static_cast<float>(BombermanLevel::TileSize) / localBounds.size.x,
			static_cast<float>(BombermanLevel::TileSize) / localBounds.size.y
			});
	}
}

BombermanPlayer::AnimationSet& BombermanPlayer::getActiveAnimationSet()
{
	switch (m_facingDirection)
	{
	case BombermanDirection::Up:
		return m_backAnimation;

	case BombermanDirection::Down:
		return m_frontAnimation;

	case BombermanDirection::Left:
		return m_leftAnimation;

	case BombermanDirection::Right:
	default:
		return m_rightAnimation;
	}
}

const BombermanPlayer::AnimationSet& BombermanPlayer::getActiveAnimationSet() const
{
	switch (m_facingDirection)
	{
	case BombermanDirection::Up:
		return m_backAnimation;

	case BombermanDirection::Down:
		return m_frontAnimation;

	case BombermanDirection::Left:
		return m_leftAnimation;

	case BombermanDirection::Right:
	default:
		return m_rightAnimation;
	}
}

const sf::Texture* BombermanPlayer::getCurrentAnimationTexture() const
{
	const AnimationSet& animationSet = getActiveAnimationSet();

	if (animationSet.frames.empty())
		return nullptr;

	if (!m_movementKeyHeld)
	{
		return &animationSet.frames[animationSet.idleFrameIndex % animationSet.frames.size()];
	}

	if (animationSet.movementSequence.empty())
	{
		return &animationSet.frames[animationSet.idleFrameIndex % animationSet.frames.size()];
	}

	const std::size_t frameIndex =
		animationSet.movementSequence[animationSet.sequenceIndex % animationSet.movementSequence.size()];

	return &animationSet.frames[frameIndex % animationSet.frames.size()];
}

void BombermanPlayer::resetActiveAnimationToIdle()
{
	AnimationSet& animationSet = getActiveAnimationSet();
	animationSet.sequenceIndex = 0;
	animationSet.timer = 0.f;
}

bool BombermanPlayer::canFitAt(sf::Vector2f topLeftPosition,
	const std::function<bool(int col, int row)>& isTileBlocked) const
{
	const sf::Vector2f circleCenter = getCollisionCenterAt(topLeftPosition);
	const float radius = m_collisionRadius;

	const int leftCol = static_cast<int>(std::floor((circleCenter.x - radius) / static_cast<float>(BombermanLevel::TileSize)));
	const int rightCol = static_cast<int>(std::floor((circleCenter.x + radius - 0.1f) / static_cast<float>(BombermanLevel::TileSize)));
	const int topRow = static_cast<int>(std::floor((circleCenter.y - radius) / static_cast<float>(BombermanLevel::TileSize)));
	const int bottomRow = static_cast<int>(std::floor((circleCenter.y + radius - 0.1f) / static_cast<float>(BombermanLevel::TileSize)));

	for (int row = topRow; row <= bottomRow; ++row)
	{
		for (int col = leftCol; col <= rightCol; ++col)
		{
			if (!isTileBlocked(col, row))
				continue;

			const sf::FloatRect tileRect = MakeTileRect(col, row);

			if (CircleIntersectsRect(circleCenter, radius, tileRect))
				return false;
		}
	}

	return true;
}

bool BombermanPlayer::tryMoveWithEdgeCorrection(sf::Vector2f movement,
	const std::function<bool(int col, int row)>& isTileBlocked)
{
	if (movement.x == 0.f && movement.y == 0.f)
		return false;

	const sf::Vector2f center = getCollisionCenter();

	if (movement.x != 0.f)
	{
		const float nearestLaneCenterY = getNearestLaneCenter(center.y);
		const float laneOffsetY = nearestLaneCenterY - center.y;
		const float absLaneOffsetY = std::abs(laneOffsetY);

		if (absLaneOffsetY <= m_edgeCorrectionDeadZone)
			return false;

		if (absLaneOffsetY > m_edgeCorrectionMaxDistance)
			return false;

		const float directionTowardLane = SignOrZero(laneOffsetY);
		const float maxCorrectionThisFrame = std::min(absLaneOffsetY, m_edgeCorrectionMaxDistance);

		for (float amount = m_edgeCorrectionStep;
			amount <= maxCorrectionThisFrame;
			amount += m_edgeCorrectionStep)
		{
			const sf::Vector2f offset{ 0.f, directionTowardLane * amount };

			if (tryForwardMoveWithPerpendicularOffset(movement, offset, isTileBlocked))
				return true;
		}
	}

	if (movement.y != 0.f)
	{
		const float nearestLaneCenterX = getNearestLaneCenter(center.x);
		const float laneOffsetX = nearestLaneCenterX - center.x;
		const float absLaneOffsetX = std::abs(laneOffsetX);

		if (absLaneOffsetX <= m_edgeCorrectionDeadZone)
			return false;

		if (absLaneOffsetX > m_edgeCorrectionMaxDistance)
			return false;

		const float directionTowardLane = SignOrZero(laneOffsetX);
		const float maxCorrectionThisFrame = std::min(absLaneOffsetX, m_edgeCorrectionMaxDistance);

		for (float amount = m_edgeCorrectionStep;
			amount <= maxCorrectionThisFrame;
			amount += m_edgeCorrectionStep)
		{
			const sf::Vector2f offset{ directionTowardLane * amount, 0.f };

			if (tryForwardMoveWithPerpendicularOffset(movement, offset, isTileBlocked))
				return true;
		}
	}

	return false;
}

bool BombermanPlayer::tryForwardMoveWithPerpendicularOffset(sf::Vector2f movement,
	sf::Vector2f perpendicularOffset,
	const std::function<bool(int col, int row)>& isTileBlocked)
{
	const sf::Vector2f candidatePosition = m_position + perpendicularOffset + movement;

	if (!canFitAt(candidatePosition, isTileBlocked))
		return false;

	m_position = candidatePosition;
	return true;
}

sf::Vector2f BombermanPlayer::getCollisionCenterAt(sf::Vector2f topLeftPosition) const
{
	const float halfTile = static_cast<float>(BombermanLevel::TileSize) * 0.5f;

	return {
		topLeftPosition.x + halfTile,
		topLeftPosition.y + halfTile
	};
}

sf::Vector2f BombermanPlayer::getCollisionCenter() const
{
	return getCollisionCenterAt(m_position);
}

float BombermanPlayer::getNearestLaneCenter(float positionOnAxis) const
{
	const float tileSize = static_cast<float>(BombermanLevel::TileSize);
	const float laneIndex = std::round((positionOnAxis - tileSize * 0.5f) / tileSize);

	return laneIndex * tileSize + tileSize * 0.5f;
}

void BombermanPlayer::draw(sf::RenderTarget& target) const
{
	if (!m_sprite || !m_alive)
		return;

	if (m_invincibilityTimer > 0.f)
	{
		const int flashPhase = static_cast<int>(m_invincibilityTimer * m_flashRate);

		if (flashPhase % 2 != 0)
			return;
	}

	target.draw(*m_sprite);
}

void BombermanPlayer::kill()
{
	m_alive = false;
	m_invincibilityTimer = 0.f;
}

bool BombermanPlayer::isAlive() const
{
	return m_alive;
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
	m_moveSpeed = std::max(80.f, moveSpeed);
}

float BombermanPlayer::getMoveSpeed() const
{
	return m_moveSpeed;
}

BombermanGridPosition BombermanPlayer::getGridPosition(const BombermanLevel& level) const
{
	return level.worldToGrid(getCollisionCenter());
}

BombermanDirection BombermanPlayer::getFacingDirection() const
{
	return m_facingDirection;
}

sf::FloatRect BombermanPlayer::getBounds() const
{
	return getCollisionBounds();
}

sf::FloatRect BombermanPlayer::getCollisionBounds() const
{
	const sf::Vector2f center = getCollisionCenter();

	return sf::FloatRect(
		{
			center.x - m_collisionRadius,
			center.y - m_collisionRadius
		},
		{
			m_collisionRadius * 2.f,
			m_collisionRadius * 2.f
		}
	);
}

const std::string& BombermanPlayer::getLastError() const
{
	return m_lastError;
}