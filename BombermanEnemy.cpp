#include "BombermanEnemy.h"

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

bool BombermanEnemy::load(const std::string& enemyDirectory,
	BombermanEnemyType type,
	BombermanGridPosition spawnPosition,
	const BombermanLevel& level)
{
	m_lastError.clear();

	m_type = type;
	m_gridPosition = spawnPosition;
	m_targetGridPosition = spawnPosition;

	m_position = level.gridToWorldTopLeft(spawnPosition);
	m_targetPosition = m_position;

	m_isMoving = false;
	m_alive = true;
	m_animationTimer = 0.f;
	m_animationFrameIndex = 0;
	m_facing = FacingDirection::Front;

	m_hitPoints = 1;

	m_bomberPreparationTimer = 0.f;
	m_bomberExplosionTimer = 0.f;
	m_bomberPreparingExplosion = false;
	m_pendingBomberExplosion = false;

	if (m_type == BombermanEnemyType::Lamp)
	{
		m_moveSpeed = 87.5f;
		m_animationFrameDuration = 0.10f;
		m_hitPoints = 1;
		return loadLampAnimation(enemyDirectory);
	}

	if (m_type == BombermanEnemyType::Tree)
	{
		m_moveSpeed = 96.f;
		m_animationFrameDuration = 0.11f;
		m_hitPoints = 1;
		return loadTreeAnimations(enemyDirectory);
	}

	if (m_type == BombermanEnemyType::Bomber)
	{
		m_moveSpeed = 62.f;
		m_animationFrameDuration = 0.16f;
		m_hitPoints = 1;

		m_bomberPreparationTimer = 2.2f;
		m_bomberPreparationCooldown = 5.0f;
		m_bomberExplosionTimer = 0.f;
		m_bomberExplosionDelay = 3.4f;
		m_bomberExplosionRange = 3;
		m_bomberPreparingExplosion = false;
		m_pendingBomberExplosion = false;

		return loadBomberAnimations(enemyDirectory);
	}

	if (m_type == BombermanEnemyType::Chomper)
	{
		m_moveSpeed = 105.f;
		m_animationFrameDuration = 0.10f;
		m_hitPoints = 3;
		return loadChomperAnimations(enemyDirectory);
	}

	m_moveSpeed = 95.f;
	m_animationFrameDuration = 0.14f;
	m_hitPoints = 1;
	return loadCopterAnimations(enemyDirectory);
}

bool BombermanEnemy::loadCopterAnimations(const std::string& enemyDirectory)
{
	return loadDirectionalAnimations(enemyDirectory, "Copter");
}

bool BombermanEnemy::loadTreeAnimations(const std::string& enemyDirectory)
{
	return loadDirectionalAnimations(enemyDirectory, "Tree");
}

bool BombermanEnemy::loadBomberAnimations(const std::string& enemyDirectory)
{
	return loadDirectionalAnimations(enemyDirectory, "Bomber");
}

bool BombermanEnemy::loadChomperAnimations(const std::string& enemyDirectory)
{
	return loadDirectionalAnimations(enemyDirectory, "Chomper");
}

bool BombermanEnemy::loadDirectionalAnimations(const std::string& enemyDirectory,
	const std::string& readableName)
{
	namespace fs = std::filesystem;

	const fs::path base(enemyDirectory);

	if (!loadAnimationFramesFromDirectory(m_frontAnimation, (base / "Front").string(), readableName + " front"))
		return false;

	if (!loadAnimationFramesFromDirectory(m_backAnimation, (base / "Back").string(), readableName + " back"))
		return false;

	if (!loadAnimationFramesFromDirectory(m_sideAnimation, (base / "Side").string(), readableName + " side"))
		return false;

	return true;
}

bool BombermanEnemy::loadLampAnimation(const std::string& enemyDirectory)
{
	if (!loadAnimationFramesFromDirectory(m_lampAnimation, enemyDirectory, "Lamp"))
		return false;

	return true;
}

bool BombermanEnemy::loadAnimationFramesFromDirectory(AnimationSet& animation,
	const std::string& directoryPath,
	const std::string& readableName)
{
	namespace fs = std::filesystem;

	animation.frames.clear();

	const fs::path directory(directoryPath);

	if (!fs::exists(directory) || !fs::is_directory(directory))
	{
		m_lastError = "Failed to load Bomberman enemy " + readableName + " animation: folder does not exist: " + directoryPath;
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
		m_lastError = "Failed to load Bomberman enemy " + readableName + " animation: no PNG frames found in: " + directoryPath;
		return false;
	}

	animation.frames.reserve(framePaths.size());

	for (const fs::path& framePath : framePaths)
	{
		sf::Texture texture;

		if (!texture.loadFromFile(framePath.string()))
		{
			m_lastError = "Failed to load Bomberman enemy frame: " + framePath.string();
			return false;
		}

		animation.frames.push_back(std::move(texture));
	}

	return true;
}

void BombermanEnemy::update(float deltaTime,
	BombermanGridPosition targetGridPosition,
	const TileBlockedCallback& isTileBlocked)
{
	if (!m_alive)
		return;

	updateBomberDetonation(deltaTime);

	if (m_bomberPreparingExplosion)
	{
		updateAnimation(deltaTime);
		return;
	}

	if (!m_isMoving)
	{
		chooseNextMove(targetGridPosition, isTileBlocked);
	}

	updateMovement(deltaTime);
	updateAnimation(deltaTime);
}

void BombermanEnemy::updateBomberDetonation(float deltaTime)
{
	if (m_type != BombermanEnemyType::Bomber)
		return;

	if (m_bomberPreparingExplosion)
	{
		m_bomberExplosionTimer -= deltaTime;

		if (m_bomberExplosionTimer <= 0.f)
		{
			m_bomberPreparingExplosion = false;
			m_pendingBomberExplosion = true;
			m_bomberPreparationTimer = m_bomberPreparationCooldown;
			m_bomberExplosionTimer = 0.f;
		}

		return;
	}

	m_bomberPreparationTimer -= deltaTime;

	if (m_bomberPreparationTimer <= 0.f)
	{
		m_isMoving = false;
		m_position = gridToWorldTopLeft(m_gridPosition);
		m_targetGridPosition = m_gridPosition;
		m_targetPosition = m_position;

		m_bomberPreparingExplosion = true;
		m_bomberExplosionTimer = m_bomberExplosionDelay;
	}
}

void BombermanEnemy::chooseNextMove(BombermanGridPosition targetGridPosition,
	const TileBlockedCallback& isTileBlocked)
{
	if (m_type == BombermanEnemyType::Lamp)
	{
		chooseLampMove(targetGridPosition, isTileBlocked);
		return;
	}

	if (m_type == BombermanEnemyType::Tree ||
		m_type == BombermanEnemyType::Bomber)
	{
		chooseStraightLineMove(isTileBlocked);
		return;
	}

	if (m_type == BombermanEnemyType::Chomper)
	{
		chooseChomperMove(targetGridPosition, isTileBlocked);
		return;
	}

	chooseCopterMove(isTileBlocked);
}

void BombermanEnemy::chooseCopterMove(const TileBlockedCallback& isTileBlocked)
{
	std::vector<std::pair<int, int>> directions =
	{
		{ 1, 0 },
		{ -1, 0 },
		{ 0, 1 },
		{ 0, -1 }
	};

	std::shuffle(directions.begin(), directions.end(), m_rng);

	for (const auto& direction : directions)
	{
		if (tryStartMove(direction.first, direction.second, isTileBlocked))
			return;
	}
}

void BombermanEnemy::chooseLampMove(BombermanGridPosition targetGridPosition,
	const TileBlockedCallback& isTileBlocked)
{
	std::vector<std::pair<int, int>> preferredDirections;

	const int horizontalDelta = targetGridPosition.col - m_gridPosition.col;
	const int verticalDelta = targetGridPosition.row - m_gridPosition.row;

	if (std::abs(horizontalDelta) >= std::abs(verticalDelta))
	{
		if (horizontalDelta > 0) preferredDirections.push_back({ 1, 0 });
		if (horizontalDelta < 0) preferredDirections.push_back({ -1, 0 });
		if (verticalDelta > 0) preferredDirections.push_back({ 0, 1 });
		if (verticalDelta < 0) preferredDirections.push_back({ 0, -1 });
	}
	else
	{
		if (verticalDelta > 0) preferredDirections.push_back({ 0, 1 });
		if (verticalDelta < 0) preferredDirections.push_back({ 0, -1 });
		if (horizontalDelta > 0) preferredDirections.push_back({ 1, 0 });
		if (horizontalDelta < 0) preferredDirections.push_back({ -1, 0 });
	}

	preferredDirections.push_back({ 1, 0 });
	preferredDirections.push_back({ -1, 0 });
	preferredDirections.push_back({ 0, 1 });
	preferredDirections.push_back({ 0, -1 });

	for (const auto& direction : preferredDirections)
	{
		if (tryStartMove(direction.first, direction.second, isTileBlocked))
			return;
	}
}

void BombermanEnemy::chooseStraightLineMove(const TileBlockedCallback& isTileBlocked)
{
	const BombermanGridPosition direction = getFacingDirectionDelta();

	if (tryStartMove(direction.col, direction.row, isTileBlocked))
		return;

	reverseDirection();

	const BombermanGridPosition reversedDirection = getFacingDirectionDelta();

	if (tryStartMove(reversedDirection.col, reversedDirection.row, isTileBlocked))
		return;

	std::vector<std::pair<int, int>> fallbackDirections =
	{
		{ 1, 0 },
		{ -1, 0 },
		{ 0, 1 },
		{ 0, -1 }
	};

	std::shuffle(fallbackDirections.begin(), fallbackDirections.end(), m_rng);

	for (const auto& directionPair : fallbackDirections)
	{
		if (tryStartMove(directionPair.first, directionPair.second, isTileBlocked))
			return;
	}
}

void BombermanEnemy::chooseChomperMove(BombermanGridPosition targetGridPosition,
	const TileBlockedCallback& isTileBlocked)
{
	std::vector<std::pair<int, int>> preferredDirections;

	const int horizontalDelta = targetGridPosition.col - m_gridPosition.col;
	const int verticalDelta = targetGridPosition.row - m_gridPosition.row;

	if (targetGridPosition != m_gridPosition)
	{
		if (std::abs(horizontalDelta) >= std::abs(verticalDelta))
		{
			if (horizontalDelta > 0) preferredDirections.push_back({ 1, 0 });
			if (horizontalDelta < 0) preferredDirections.push_back({ -1, 0 });
			if (verticalDelta > 0) preferredDirections.push_back({ 0, 1 });
			if (verticalDelta < 0) preferredDirections.push_back({ 0, -1 });
		}
		else
		{
			if (verticalDelta > 0) preferredDirections.push_back({ 0, 1 });
			if (verticalDelta < 0) preferredDirections.push_back({ 0, -1 });
			if (horizontalDelta > 0) preferredDirections.push_back({ 1, 0 });
			if (horizontalDelta < 0) preferredDirections.push_back({ -1, 0 });
		}
	}

	const BombermanGridPosition currentDirection = getFacingDirectionDelta();

	preferredDirections.push_back({ currentDirection.col, currentDirection.row });
	preferredDirections.push_back({ 1, 0 });
	preferredDirections.push_back({ -1, 0 });
	preferredDirections.push_back({ 0, 1 });
	preferredDirections.push_back({ 0, -1 });

	for (const auto& direction : preferredDirections)
	{
		if (tryStartMove(direction.first, direction.second, isTileBlocked))
			return;
	}

	reverseDirection();
}

bool BombermanEnemy::tryStartMove(int colDelta,
	int rowDelta,
	const TileBlockedCallback& isTileBlocked)
{
	if (colDelta == 0 && rowDelta == 0)
		return false;

	const int targetCol = m_gridPosition.col + colDelta;
	const int targetRow = m_gridPosition.row + rowDelta;

	if (isTileBlocked(targetCol, targetRow))
		return false;

	m_targetGridPosition = { targetCol, targetRow };
	m_targetPosition = gridToWorldTopLeft(m_targetGridPosition);

	m_isMoving = true;

	if (colDelta < 0)
		m_facing = FacingDirection::Left;
	else if (colDelta > 0)
		m_facing = FacingDirection::Right;
	else if (rowDelta < 0)
		m_facing = FacingDirection::Back;
	else if (rowDelta > 0)
		m_facing = FacingDirection::Front;

	return true;
}

void BombermanEnemy::reverseDirection()
{
	switch (m_facing)
	{
	case FacingDirection::Left:
		m_facing = FacingDirection::Right;
		break;

	case FacingDirection::Right:
		m_facing = FacingDirection::Left;
		break;

	case FacingDirection::Back:
		m_facing = FacingDirection::Front;
		break;

	case FacingDirection::Front:
	default:
		m_facing = FacingDirection::Back;
		break;
	}
}

void BombermanEnemy::updateMovement(float deltaTime)
{
	if (!m_isMoving)
		return;

	const sf::Vector2f toTarget = m_targetPosition - m_position;
	const float distance = std::sqrt(toTarget.x * toTarget.x + toTarget.y * toTarget.y);

	if (distance <= 0.001f)
	{
		m_position = m_targetPosition;
		m_gridPosition = m_targetGridPosition;
		m_isMoving = false;
		return;
	}

	const float step = m_moveSpeed * deltaTime;

	if (step >= distance)
	{
		m_position = m_targetPosition;
		m_gridPosition = m_targetGridPosition;
		m_isMoving = false;
		return;
	}

	const sf::Vector2f direction = toTarget / distance;
	m_position += direction * step;
}

void BombermanEnemy::updateAnimation(float deltaTime)
{
	const AnimationSet& animation = getCurrentAnimation();

	if (animation.frames.empty())
		return;

	m_animationTimer += deltaTime;

	while (m_animationTimer >= m_animationFrameDuration)
	{
		m_animationTimer -= m_animationFrameDuration;
		++m_animationFrameIndex;
	}
}

const BombermanEnemy::AnimationSet& BombermanEnemy::getCurrentAnimation() const
{
	if (m_type == BombermanEnemyType::Lamp)
		return m_lampAnimation;

	switch (m_facing)
	{
	case FacingDirection::Back:
		return m_backAnimation;

	case FacingDirection::Left:
	case FacingDirection::Right:
		return m_sideAnimation;

	case FacingDirection::Front:
	default:
		return m_frontAnimation;
	}
}

bool BombermanEnemy::shouldUsePingPongAnimation() const
{
	return m_type == BombermanEnemyType::Lamp ||
		m_type == BombermanEnemyType::Tree ||
		m_type == BombermanEnemyType::Bomber ||
		m_type == BombermanEnemyType::Chomper;
}

std::size_t BombermanEnemy::getCurrentFrameIndex() const
{
	const AnimationSet& animation = getCurrentAnimation();

	if (animation.frames.empty())
		return 0;

	if (shouldUsePingPongAnimation() && animation.frames.size() > 1)
	{
		const std::size_t frameCount = animation.frames.size();
		const std::size_t cycleLength = frameCount * 2 - 2;
		const std::size_t cycleIndex = m_animationFrameIndex % cycleLength;

		if (cycleIndex < frameCount)
			return cycleIndex;

		return cycleLength - cycleIndex;
	}

	return m_animationFrameIndex % animation.frames.size();
}

void BombermanEnemy::draw(sf::RenderTarget& target) const
{
	if (!m_alive)
		return;

	if (m_bomberPreparingExplosion)
		return;

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

	const bool flipX =
		m_facing == FacingDirection::Right &&
		(m_type == BombermanEnemyType::Copter ||
			m_type == BombermanEnemyType::Tree ||
			m_type == BombermanEnemyType::Bomber ||
			m_type == BombermanEnemyType::Chomper);

	sprite.setScale({
		flipX ? -scaleX : scaleX,
		scaleY
		});

	float drawX = m_position.x;
	float drawY = m_position.y;

	if (flipX)
		drawX += static_cast<float>(BombermanLevel::TileSize);

	sprite.setPosition({ drawX, drawY });

	target.draw(sprite);
}

bool BombermanEnemy::isAlive() const
{
	return m_alive;
}

void BombermanEnemy::kill()
{
	m_alive = false;
}

void BombermanEnemy::takeHit()
{
	if (!m_alive)
		return;

	if (!canBeKilledByExplosion())
		return;

	--m_hitPoints;

	if (m_hitPoints <= 0)
		m_alive = false;
}

int BombermanEnemy::getHitPoints() const
{
	return m_hitPoints;
}

bool BombermanEnemy::canPassThroughBreakableBlocks() const
{
	return m_type == BombermanEnemyType::Lamp;
}

bool BombermanEnemy::canBeKilledByExplosion() const
{
	if (m_type == BombermanEnemyType::Bomber && m_bomberPreparingExplosion)
		return false;

	return true;
}

bool BombermanEnemy::wantsToEatBombs() const
{
	return m_type == BombermanEnemyType::Chomper;
}

BombermanEnemyType BombermanEnemy::getType() const
{
	return m_type;
}

BombermanGridPosition BombermanEnemy::getGridPosition(const BombermanLevel& level) const
{
	const sf::FloatRect bounds = getBounds();

	const sf::Vector2f center{
		bounds.position.x + bounds.size.x * 0.5f,
		bounds.position.y + bounds.size.y * 0.5f
	};

	return level.worldToGrid(center);
}

BombermanGridPosition BombermanEnemy::getFacingDirectionDelta() const
{
	switch (m_facing)
	{
	case FacingDirection::Left:
		return { -1, 0 };

	case FacingDirection::Right:
		return { 1, 0 };

	case FacingDirection::Back:
		return { 0, -1 };

	case FacingDirection::Front:
	default:
		return { 0, 1 };
	}
}

sf::Vector2f BombermanEnemy::getDrawPosition() const
{
	return m_position;
}

sf::FloatRect BombermanEnemy::getBounds() const
{
	return sf::FloatRect(
		{
			m_position.x + 6.f,
			m_position.y + 6.f
		},
		{
			static_cast<float>(BombermanLevel::TileSize) - 12.f,
			static_cast<float>(BombermanLevel::TileSize) - 12.f
		}
	);
}

bool BombermanEnemy::shouldDrawAsBomb() const
{
	return m_type == BombermanEnemyType::Bomber && m_bomberPreparingExplosion;
}

bool BombermanEnemy::isPreparingBombExplosion() const
{
	return m_type == BombermanEnemyType::Bomber && m_bomberPreparingExplosion;
}

int BombermanEnemy::getBomberExplosionRange() const
{
	return m_bomberExplosionRange;
}

bool BombermanEnemy::consumePendingBomberExplosion()
{
	if (!m_pendingBomberExplosion)
		return false;

	m_pendingBomberExplosion = false;
	return true;
}

sf::Vector2f BombermanEnemy::gridToWorldTopLeft(BombermanGridPosition gridPosition) const
{
	return {
		static_cast<float>(gridPosition.col * BombermanLevel::TileSize),
		static_cast<float>(gridPosition.row * BombermanLevel::TileSize)
	};
}

sf::Vector2f BombermanEnemy::gridToWorldCenter(BombermanGridPosition gridPosition) const
{
	return {
		static_cast<float>(gridPosition.col * BombermanLevel::TileSize) + static_cast<float>(BombermanLevel::TileSize) * 0.5f,
		static_cast<float>(gridPosition.row * BombermanLevel::TileSize) + static_cast<float>(BombermanLevel::TileSize) * 0.5f
	};
}

const std::string& BombermanEnemy::getLastError() const
{
	return m_lastError;
}