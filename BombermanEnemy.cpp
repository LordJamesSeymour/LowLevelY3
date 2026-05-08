#include "BombermanEnemy.h"

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
}

bool BombermanEnemy::load(const std::string& enemyAssetPath,
	BombermanGridPosition spawnPosition,
	const BombermanLevel& level)
{
	namespace fs = std::filesystem;

	m_lastError.clear();

	fs::path copterDirectory(enemyAssetPath);

	// Compatibility with the old call:
	// GAME1_BombermanWindow may still pass:
	// assets/Game#0/Bomberman/Resources/Enemies/enemy_basic.png
	//
	// If a file path is passed, we move to its parent folder and look for /Copter.
	if (copterDirectory.has_extension())
	{
		copterDirectory = copterDirectory.parent_path() / "Copter";
	}

	// If the caller passes the Enemies folder directly, also find /Copter.
	if (copterDirectory.filename().string() == "Enemies")
	{
		copterDirectory /= "Copter";
	}

	const fs::path frontDirectory = copterDirectory / "Front";
	const fs::path backDirectory = copterDirectory / "Back";
	const fs::path sideDirectory = copterDirectory / "Side";

	if (!loadAnimationFolder(m_frontAnimation, frontDirectory.string(), "Copter front animation"))
		return false;

	if (!loadAnimationFolder(m_backAnimation, backDirectory.string(), "Copter back animation"))
		return false;

	if (!loadAnimationFolder(m_sideAnimation, sideDirectory.string(), "Copter side animation"))
		return false;

	m_alive = true;
	m_isMoving = false;

	m_gridPosition = spawnPosition;
	m_targetGridPosition = spawnPosition;

	m_position = level.gridToWorldTopLeft(spawnPosition);
	m_targetPosition = m_position;

	m_facingDirection = BombermanDirection::Down;
	m_lastMoveDirection = BombermanDirection::Down;

	const sf::Texture* startTexture = getCurrentTexture();

	if (startTexture == nullptr)
	{
		m_lastError = "Copter enemy has no valid starting animation frame.";
		return false;
	}

	m_sprite.emplace(*startTexture);
	snapSpriteToPosition();

	return true;
}

bool BombermanEnemy::loadAnimationFolder(AnimationSet& animationSet,
	const std::string& directoryPath,
	const std::string& readableName)
{
	namespace fs = std::filesystem;

	animationSet.frames.clear();
	animationSet.currentFrame = 0;
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

	animationSet.frames.reserve(framePaths.size());

	for (const fs::path& framePath : framePaths)
	{
		sf::Texture texture;

		if (!texture.loadFromFile(framePath.string()))
		{
			m_lastError = "Failed to load " + readableName + " frame: " + framePath.string();
			return false;
		}

		animationSet.frames.push_back(std::move(texture));
	}

	return true;
}

void BombermanEnemy::update(float deltaTime,
	const std::function<bool(int col, int row)>& isTileBlocked)
{
	if (!m_alive || !m_sprite)
		return;

	updateAnimation(deltaTime);

	if (!m_isMoving)
	{
		chooseNewDirection(isTileBlocked);
		applyCurrentAnimationFrame();
		snapSpriteToPosition();
		return;
	}

	const sf::Vector2f toTarget = m_targetPosition - m_position;
	const float distanceToTarget = std::sqrt(toTarget.x * toTarget.x + toTarget.y * toTarget.y);
	const float movementThisFrame = m_moveSpeed * deltaTime;

	if (distanceToTarget <= movementThisFrame || distanceToTarget <= 0.01f)
	{
		m_position = m_targetPosition;
		m_gridPosition = m_targetGridPosition;
		m_isMoving = false;
	}
	else
	{
		const sf::Vector2f moveDirection{
			toTarget.x / distanceToTarget,
			toTarget.y / distanceToTarget
		};

		m_position += moveDirection * movementThisFrame;
	}

	applyCurrentAnimationFrame();
	snapSpriteToPosition();
}

void BombermanEnemy::updateAnimation(float deltaTime)
{
	AnimationSet& animationSet = getActiveAnimationSet();

	if (animationSet.frames.size() <= 1)
		return;

	animationSet.timer += deltaTime;

	while (animationSet.timer >= m_frameDuration)
	{
		animationSet.timer -= m_frameDuration;
		animationSet.currentFrame = (animationSet.currentFrame + 1) % animationSet.frames.size();
	}
}

void BombermanEnemy::chooseNewDirection(const std::function<bool(int col, int row)>& isTileBlocked)
{
	std::vector<BombermanDirection> possibleDirections;

	const BombermanDirection directions[4]
	{
		BombermanDirection::Up,
		BombermanDirection::Down,
		BombermanDirection::Left,
		BombermanDirection::Right
	};

	const BombermanDirection oppositeDirection = getOppositeDirection(m_lastMoveDirection);

	for (BombermanDirection direction : directions)
	{
		const BombermanGridPosition neighbour = getNeighbourPosition(direction);

		if (isTileBlocked(neighbour.col, neighbour.row))
			continue;

		possibleDirections.push_back(direction);
	}

	if (possibleDirections.empty())
		return;

	// Prefer not immediately reversing unless it is the only available route.
	std::vector<BombermanDirection> nonReverseDirections;

	for (BombermanDirection direction : possibleDirections)
	{
		if (direction != oppositeDirection)
		{
			nonReverseDirections.push_back(direction);
		}
	}

	const std::vector<BombermanDirection>& selectionPool =
		nonReverseDirections.empty()
		? possibleDirections
		: nonReverseDirections;

	std::uniform_int_distribution<int> directionDistribution(
		0,
		static_cast<int>(selectionPool.size()) - 1);

	const BombermanDirection chosenDirection = selectionPool[directionDistribution(m_rng)];
	tryStartMove(chosenDirection, isTileBlocked);
}

bool BombermanEnemy::tryStartMove(BombermanDirection direction,
	const std::function<bool(int col, int row)>& isTileBlocked)
{
	const BombermanGridPosition neighbour = getNeighbourPosition(direction);

	if (isTileBlocked(neighbour.col, neighbour.row))
		return false;

	m_facingDirection = direction;
	m_lastMoveDirection = direction;

	m_targetGridPosition = neighbour;

	m_targetPosition = {
		static_cast<float>(m_targetGridPosition.col * BombermanLevel::TileSize),
		static_cast<float>(m_targetGridPosition.row * BombermanLevel::TileSize)
	};

	m_isMoving = true;

	return true;
}

BombermanGridPosition BombermanEnemy::getNeighbourPosition(BombermanDirection direction) const
{
	switch (direction)
	{
	case BombermanDirection::Up:
		return { m_gridPosition.col, m_gridPosition.row - 1 };

	case BombermanDirection::Down:
		return { m_gridPosition.col, m_gridPosition.row + 1 };

	case BombermanDirection::Left:
		return { m_gridPosition.col - 1, m_gridPosition.row };

	case BombermanDirection::Right:
		return { m_gridPosition.col + 1, m_gridPosition.row };
	}

	return m_gridPosition;
}

sf::Vector2f BombermanEnemy::directionToVector(BombermanDirection direction) const
{
	switch (direction)
	{
	case BombermanDirection::Up:
		return { 0.f, -1.f };

	case BombermanDirection::Down:
		return { 0.f, 1.f };

	case BombermanDirection::Left:
		return { -1.f, 0.f };

	case BombermanDirection::Right:
		return { 1.f, 0.f };
	}

	return { 0.f, 0.f };
}

BombermanDirection BombermanEnemy::getOppositeDirection(BombermanDirection direction) const
{
	switch (direction)
	{
	case BombermanDirection::Up:
		return BombermanDirection::Down;

	case BombermanDirection::Down:
		return BombermanDirection::Up;

	case BombermanDirection::Left:
		return BombermanDirection::Right;

	case BombermanDirection::Right:
		return BombermanDirection::Left;
	}

	return BombermanDirection::Down;
}

BombermanEnemy::AnimationSet& BombermanEnemy::getActiveAnimationSet()
{
	switch (m_facingDirection)
	{
	case BombermanDirection::Up:
		return m_backAnimation;

	case BombermanDirection::Down:
		return m_frontAnimation;

	case BombermanDirection::Left:
	case BombermanDirection::Right:
	default:
		return m_sideAnimation;
	}
}

const BombermanEnemy::AnimationSet& BombermanEnemy::getActiveAnimationSet() const
{
	switch (m_facingDirection)
	{
	case BombermanDirection::Up:
		return m_backAnimation;

	case BombermanDirection::Down:
		return m_frontAnimation;

	case BombermanDirection::Left:
	case BombermanDirection::Right:
	default:
		return m_sideAnimation;
	}
}

const sf::Texture* BombermanEnemy::getCurrentTexture() const
{
	const AnimationSet& animationSet = getActiveAnimationSet();

	if (animationSet.frames.empty())
		return nullptr;

	return &animationSet.frames[animationSet.currentFrame % animationSet.frames.size()];
}

void BombermanEnemy::applyCurrentAnimationFrame()
{
	if (!m_sprite)
		return;

	const sf::Texture* currentTexture = getCurrentTexture();

	if (currentTexture == nullptr)
		return;

	m_sprite->setTexture(*currentTexture, true);
}

void BombermanEnemy::snapSpriteToPosition()
{
	if (!m_sprite)
		return;

	const sf::FloatRect localBounds = m_sprite->getLocalBounds();

	if (localBounds.size.x <= 0.f || localBounds.size.y <= 0.f)
		return;

	const float scaleX = static_cast<float>(BombermanLevel::TileSize) / localBounds.size.x;
	const float scaleY = static_cast<float>(BombermanLevel::TileSize) / localBounds.size.y;

	// Side sprites face LEFT by default.
	// Moving left = normal.
	// Moving right = mirrored horizontally.
	if (m_facingDirection == BombermanDirection::Right)
	{
		m_sprite->setScale({ -scaleX, scaleY });
		m_sprite->setPosition({
			m_position.x + static_cast<float>(BombermanLevel::TileSize),
			m_position.y
			});
	}
	else
	{
		m_sprite->setScale({ scaleX, scaleY });
		m_sprite->setPosition(m_position);
	}
}

void BombermanEnemy::draw(sf::RenderTarget& target) const
{
	if (!m_alive || !m_sprite)
		return;

	target.draw(*m_sprite);
}

bool BombermanEnemy::isAlive() const
{
	return m_alive;
}

void BombermanEnemy::kill()
{
	m_alive = false;
	m_isMoving = false;
}

BombermanGridPosition BombermanEnemy::getGridPosition(const BombermanLevel& level) const
{
	const sf::Vector2f center{
		m_position.x + static_cast<float>(BombermanLevel::TileSize) * 0.5f,
		m_position.y + static_cast<float>(BombermanLevel::TileSize) * 0.5f
	};

	return level.worldToGrid(center);
}

sf::FloatRect BombermanEnemy::getBounds() const
{
	const float inset = 8.f;

	return sf::FloatRect(
		{
			m_position.x + inset,
			m_position.y + inset
		},
		{
			static_cast<float>(BombermanLevel::TileSize) - inset * 2.f,
			static_cast<float>(BombermanLevel::TileSize) - inset * 2.f
		}
	);
}

const std::string& BombermanEnemy::getLastError() const
{
	return m_lastError;
}