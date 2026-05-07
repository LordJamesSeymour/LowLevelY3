#include "BombermanPlayer.h"

#include <algorithm>
#include <cmath>
#include <filesystem>

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
}

bool BombermanPlayer::load(const std::string& playerDirectory)
{
	namespace fs = std::filesystem;

	m_lastError.clear();

	const fs::path base(playerDirectory);

	const std::string downPath = (base / "player_down.png").string();
	const std::string upPath = (base / "player_up.png").string();
	const std::string leftPath = (base / "player_left.png").string();
	const std::string rightPath = (base / "player_right.png").string();

	if (!m_downTexture.loadFromFile(downPath))
	{
		m_lastError = "Failed to load Bomberman player texture: " + downPath;
		return false;
	}

	if (!loadTextureOrFallback(m_upTexture, upPath, downPath))
		return false;

	if (!loadTextureOrFallback(m_leftTexture, leftPath, downPath))
		return false;

	if (!loadTextureOrFallback(m_rightTexture, rightPath, downPath))
		return false;

	m_sprite.emplace(m_downTexture);

	const sf::FloatRect localBounds = m_sprite->getLocalBounds();

	if (localBounds.size.x <= 0.f || localBounds.size.y <= 0.f)
	{
		m_lastError = "Bomberman player texture has invalid size.";
		return false;
	}

	m_sprite->setScale({
		static_cast<float>(BombermanLevel::TileSize) / localBounds.size.x,
		static_cast<float>(BombermanLevel::TileSize) / localBounds.size.y
		});

	return true;
}

bool BombermanPlayer::loadTextureOrFallback(sf::Texture& texture,
	const std::string& preferredPath,
	const std::string& fallbackPath)
{
	if (texture.loadFromFile(preferredPath))
		return true;

	if (!texture.loadFromFile(fallbackPath))
	{
		m_lastError = "Failed to load Bomberman player texture: " + preferredPath;
		return false;
	}

	return true;
}

void BombermanPlayer::reset(BombermanGridPosition spawnPosition, const BombermanLevel& level)
{
	m_alive = true;
	m_invincibilityTimer = 0.f;

	m_facingDirection = BombermanDirection::Down;
	m_currentMoveInput = { 0.f, 0.f };

	m_upHeldLastFrame = false;
	m_downHeldLastFrame = false;
	m_leftHeldLastFrame = false;
	m_rightHeldLastFrame = false;

	applyTextureForFacingDirection();

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
	applyTextureForFacingDirection();

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
		const bool anyHeld = upHeld || downHeld || leftHeld || rightHeld;

		if (!anyHeld)
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

void BombermanPlayer::applyTextureForFacingDirection()
{
	if (!m_sprite)
		return;

	switch (m_facingDirection)
	{
	case BombermanDirection::Down:
		m_sprite->setTexture(m_downTexture, true);
		break;

	case BombermanDirection::Up:
		m_sprite->setTexture(m_upTexture, true);
		break;

	case BombermanDirection::Left:
		m_sprite->setTexture(m_leftTexture, true);
		break;

	case BombermanDirection::Right:
		m_sprite->setTexture(m_rightTexture, true);
		break;
	}

	const sf::FloatRect localBounds = m_sprite->getLocalBounds();

	if (localBounds.size.x > 0.f && localBounds.size.y > 0.f)
	{
		m_sprite->setScale({
			static_cast<float>(BombermanLevel::TileSize) / localBounds.size.x,
			static_cast<float>(BombermanLevel::TileSize) / localBounds.size.y
			});
	}
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