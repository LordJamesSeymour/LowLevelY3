#include "GAME1_Enemy.h"

#include "GAME1_Level.h"
#include "GAME1_Player.h"

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
		return path.has_extension() && ToLower(path.extension().string()) == ".png";
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

	int LeftTile(const sf::FloatRect& bounds)
	{
		return static_cast<int>(std::floor(bounds.position.x / static_cast<float>(GAME1_Level::TileSize)));
	}

	int RightTile(const sf::FloatRect& bounds)
	{
		return static_cast<int>(std::floor((bounds.position.x + bounds.size.x - 0.1f) / static_cast<float>(GAME1_Level::TileSize)));
	}

	int TopTile(const sf::FloatRect& bounds)
	{
		return static_cast<int>(std::floor(bounds.position.y / static_cast<float>(GAME1_Level::TileSize)));
	}

	int BottomTile(const sf::FloatRect& bounds)
	{
		return static_cast<int>(std::floor((bounds.position.y + bounds.size.y - 0.1f) / static_cast<float>(GAME1_Level::TileSize)));
	}
}

bool GAME1_Enemy::load(const std::string& enemiesDirectory,
	GAME1_EnemyType type,
	sf::Vector2f spawnPosition)
{
	namespace fs = std::filesystem;

	m_lastError.clear();
	m_type = type;

	const fs::path root(enemiesDirectory);

	if (!loadAnimationFramesFromDirectory(
		m_idleAnimation,
		(root / "EnemyIdle").string(),
		"SurfersQuest enemy idle"))
	{
		return false;
	}

	if (!loadAnimationFramesFromDirectory(
		m_runAnimation,
		(root / "EnemyRun").string(),
		"SurfersQuest enemy run"))
	{
		return false;
	}

	if (!loadAnimationFramesFromDirectory(
		m_hitAnimation,
		(root / "EnemyHit").string(),
		"SurfersQuest enemy hit"))
	{
		return false;
	}

	m_idleAnimation.frameDuration = 0.075f;
	m_runAnimation.frameDuration = 0.055f;
	m_hitAnimation.frameDuration = 0.065f;

	m_spawnPosition = spawnPosition;
	m_position = spawnPosition;
	m_velocity = { 0.f, 0.f };

	m_health = 1;
	m_alive = true;
	m_active = true;
	m_state = BehaviourState::Idle;
	m_facingDirection = FacingDirection::Right;
	m_animationTimer = 0.f;
	m_currentFrameIndex = 0;

	std::random_device rd;
	m_rng.seed(rd());

	chooseIdleBehaviour();
	return true;
}

bool GAME1_Enemy::loadAnimationFramesFromDirectory(AnimationSet& animation,
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
		if (entry.is_regular_file() && IsPngFile(entry.path()))
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

void GAME1_Enemy::update(float deltaTime, const GAME1_Level& level, const GAME1_Player& player)
{
	if (!m_active)
		return;

	if (m_state == BehaviourState::Hit)
	{
		updateHit(deltaTime);
		return;
	}

	m_velocity.y += m_gravity * deltaTime;
	m_position.y += m_velocity.y * deltaTime;
	snapToGround(level);

	const bool playerVisible = canSeePlayer(level, player);

	if (playerVisible && m_state != BehaviourState::Chase)
	{
		m_state = BehaviourState::Chase;
		m_animationTimer = 0.f;
		m_currentFrameIndex = 0;
	}
	else if (!playerVisible && m_state == BehaviourState::Chase)
	{
		chooseNextBehaviour();
	}

	if (m_state == BehaviourState::Idle)
		updateIdle(deltaTime);
	else if (m_state == BehaviourState::Patrol)
		updatePatrol(deltaTime, level);
	else if (m_state == BehaviourState::Chase)
		updateChase(deltaTime, level, player);

	updateAnimation(deltaTime);
}

void GAME1_Enemy::chooseNextBehaviour()
{
	if (randomInt(0, 99) < 45)
		chooseIdleBehaviour();
	else
		choosePatrolBehaviour();
}

void GAME1_Enemy::chooseIdleBehaviour()
{
	m_state = BehaviourState::Idle;
	m_velocity.x = 0.f;
	m_idleTimer = randomFloat(0.8f, 2.4f);
	m_turnTimer = randomFloat(0.25f, 0.9f);
	m_animationTimer = 0.f;
	m_currentFrameIndex = 0;
}

void GAME1_Enemy::choosePatrolBehaviour()
{
	m_state = BehaviourState::Patrol;
	m_patrolPointsRemaining = randomInt(1, 3);
	chooseNewPatrolPoint();
	m_animationTimer = 0.f;
	m_currentFrameIndex = 0;
}

void GAME1_Enemy::chooseNewPatrolPoint()
{
	const float tileSize = static_cast<float>(GAME1_Level::TileSize);
	const float distance = randomFloat(m_minPatrolDistanceTiles, m_maxPatrolDistanceTiles) * tileSize;
	const float direction = randomInt(0, 1) == 0 ? -1.f : 1.f;
	m_targetX = m_position.x + direction * distance;
	m_facingDirection = direction >= 0.f ? FacingDirection::Right : FacingDirection::Left;
}

void GAME1_Enemy::updateIdle(float deltaTime)
{
	m_idleTimer -= deltaTime;
	m_turnTimer -= deltaTime;

	if (m_turnTimer <= 0.f)
	{
		if (randomInt(0, 99) < 45)
		{
			m_facingDirection =
				m_facingDirection == FacingDirection::Right
				? FacingDirection::Left
				: FacingDirection::Right;
		}

		m_turnTimer = randomFloat(0.45f, 1.2f);
	}

	if (m_idleTimer <= 0.f)
		chooseNextBehaviour();
}

void GAME1_Enemy::updatePatrol(float deltaTime, const GAME1_Level& level)
{
	const float direction = m_targetX >= m_position.x ? 1.f : -1.f;
	m_facingDirection = direction >= 0.f ? FacingDirection::Right : FacingDirection::Left;

	const float step = direction * m_moveSpeed * deltaTime;
	const float nextX = m_position.x + step;

	const bool reachedTarget =
		(direction > 0.f && nextX >= m_targetX) ||
		(direction < 0.f && nextX <= m_targetX);

	if (!canMoveToX(nextX, level) || !hasGroundAhead(nextX, level))
	{
		m_patrolPointsRemaining = 0;
		chooseIdleBehaviour();
		return;
	}

	m_position.x = reachedTarget ? m_targetX : nextX;

	if (reachedTarget)
	{
		--m_patrolPointsRemaining;

		if (m_patrolPointsRemaining > 0)
			chooseNewPatrolPoint();
		else
			chooseNextBehaviour();
	}
}

void GAME1_Enemy::updateChase(float deltaTime, const GAME1_Level& level, const GAME1_Player& player)
{
	const sf::FloatRect playerBounds = player.getBounds();
	const sf::FloatRect enemyBounds = getBounds();

	const float playerCenterX = playerBounds.position.x + playerBounds.size.x * 0.5f;
	const float enemyCenterX = enemyBounds.position.x + enemyBounds.size.x * 0.5f;

	const float distanceToPlayer = playerCenterX - enemyCenterX;

	if (std::abs(distanceToPlayer) <= 3.f)
	{
		m_velocity.x = 0.f;
		return;
	}

	const float direction = distanceToPlayer >= 0.f ? 1.f : -1.f;
	m_facingDirection = direction >= 0.f ? FacingDirection::Right : FacingDirection::Left;

	const float chaseSpeed = m_moveSpeed * m_chaseSpeedMultiplier;
	const float nextX = m_position.x + direction * chaseSpeed * deltaTime;

	if (!canMoveToX(nextX, level) || !hasGroundAhead(nextX, level))
	{
		m_velocity.x = 0.f;
		return;
	}

	m_position.x = nextX;
}

void GAME1_Enemy::updateHit(float deltaTime)
{
	const AnimationSet& animation = getCurrentAnimationSet();

	if (animation.frames.empty())
	{
		m_active = false;
		return;
	}

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
			m_active = false;
			m_alive = false;
			return;
		}
	}
}

void GAME1_Enemy::updateAnimation(float deltaTime)
{
	const AnimationSet& animation = getCurrentAnimationSet();

	if (animation.frames.size() <= 1)
		return;

	m_animationTimer += deltaTime;

	while (m_animationTimer >= animation.frameDuration)
	{
		m_animationTimer -= animation.frameDuration;
		m_currentFrameIndex = (m_currentFrameIndex + 1) % animation.frames.size();
	}
}

bool GAME1_Enemy::canSeePlayer(const GAME1_Level& level, const GAME1_Player& player) const
{
	if (!player.isRespawning() && !player.isGameOver())
	{
		const sf::FloatRect playerBounds = player.getBounds();
		const sf::FloatRect enemyBounds = getBounds();

		const float playerCenterX = playerBounds.position.x + playerBounds.size.x * 0.5f;
		const float playerCenterY = playerBounds.position.y + playerBounds.size.y * 0.5f;
		const float enemyCenterX = enemyBounds.position.x + enemyBounds.size.x * 0.5f;
		const float enemyCenterY = enemyBounds.position.y + enemyBounds.size.y * 0.5f;

		const float horizontalDelta = playerCenterX - enemyCenterX;
		const float verticalDelta = std::abs(playerCenterY - enemyCenterY);
		const float visionRange = m_visionRangeTiles * static_cast<float>(GAME1_Level::TileSize);
		const float verticalVisionTolerance = static_cast<float>(GAME1_Level::TileSize) * 1.25f;

		const bool playerInFacingDirection =
			(m_facingDirection == FacingDirection::Right && horizontalDelta >= 0.f) ||
			(m_facingDirection == FacingDirection::Left && horizontalDelta <= 0.f);

		if (playerInFacingDirection &&
			std::abs(horizontalDelta) <= visionRange &&
			verticalDelta <= verticalVisionTolerance)
		{
			return hasClearSightToPlayer(level, player);
		}
	}

	return false;
}

bool GAME1_Enemy::hasClearSightToPlayer(const GAME1_Level& level, const GAME1_Player& player) const
{
	const sf::FloatRect playerBounds = player.getBounds();
	const sf::FloatRect enemyBounds = getBounds();

	const float playerCenterX = playerBounds.position.x + playerBounds.size.x * 0.5f;
	const float enemyCenterX = enemyBounds.position.x + enemyBounds.size.x * 0.5f;
	const float enemyCenterY = enemyBounds.position.y + enemyBounds.size.y * 0.5f;

	const int row = static_cast<int>(std::floor(enemyCenterY / static_cast<float>(GAME1_Level::TileSize)));
	const int enemyCol = static_cast<int>(std::floor(enemyCenterX / static_cast<float>(GAME1_Level::TileSize)));
	const int playerCol = static_cast<int>(std::floor(playerCenterX / static_cast<float>(GAME1_Level::TileSize)));

	if (enemyCol == playerCol)
		return true;

	const int step = playerCol > enemyCol ? 1 : -1;

	for (int col = enemyCol + step; col != playerCol; col += step)
	{
		if (level.isSolidTile(col, row))
			return false;
	}

	return true;
}

bool GAME1_Enemy::canMoveToX(float newX, const GAME1_Level& level) const
{
	sf::FloatRect futureBounds = getBounds();
	futureBounds.position.x = newX;

	const int topTile = TopTile(futureBounds);
	const int bottomTile = BottomTile(futureBounds);

	const int probeTile =
		newX > m_position.x
		? RightTile(futureBounds)
		: LeftTile(futureBounds);

	for (int row = topTile; row <= bottomTile; ++row)
	{
		if (level.isSolidTile(probeTile, row))
			return false;
	}

	return true;
}

bool GAME1_Enemy::hasGroundAhead(float newX, const GAME1_Level& level) const
{
	sf::FloatRect futureBounds = getBounds();
	futureBounds.position.x = newX;

	const float footY = futureBounds.position.y + futureBounds.size.y + 4.f;
	const float probeX =
		newX > m_position.x
		? futureBounds.position.x + futureBounds.size.x + 3.f
		: futureBounds.position.x - 3.f;

	const int col = static_cast<int>(std::floor(probeX / static_cast<float>(GAME1_Level::TileSize)));
	const int row = static_cast<int>(std::floor(footY / static_cast<float>(GAME1_Level::TileSize)));

	return level.isSolidTile(col, row) || level.isOneWayPlatformTile(col, row);
}

void GAME1_Enemy::snapToGround(const GAME1_Level& level)
{
	sf::FloatRect bounds = getBounds();

	const int leftTile = LeftTile(bounds);
	const int rightTile = RightTile(bounds);
	const int bottomTile = BottomTile(bounds);

	for (int col = leftTile; col <= rightTile; ++col)
	{
		if (level.isSolidTile(col, bottomTile) || level.isOneWayPlatformTile(col, bottomTile))
		{
			m_position.y =
				static_cast<float>(bottomTile * GAME1_Level::TileSize) -
				bounds.size.y;

			m_velocity.y = 0.f;
			return;
		}
	}
}

void GAME1_Enemy::takeStompDamage()
{
	if (!m_alive || m_state == BehaviourState::Hit)
		return;

	--m_health;

	if (m_health <= 0)
	{
		m_alive = false;
		m_state = BehaviourState::Hit;
		m_velocity = { 0.f, 0.f };
		m_animationTimer = 0.f;
		m_currentFrameIndex = 0;
	}
}

bool GAME1_Enemy::handlePlayerCollision(GAME1_Player& player)
{
	if (!m_active || !m_alive)
		return false;

	const sf::FloatRect playerBounds = player.getBounds();
	const sf::FloatRect enemyBounds = getBounds();

	if (!rectsIntersect(playerBounds, enemyBounds))
		return false;

	const sf::Vector2f playerVelocity = player.getVelocity();
	const float playerBottom = playerBounds.position.y + playerBounds.size.y;
	const float enemyTop = enemyBounds.position.y;

	const bool stompedFromAbove =
		playerVelocity.y > 40.f &&
		playerBottom <= enemyTop + enemyBounds.size.y * 0.42f;

	if (stompedFromAbove)
	{
		takeStompDamage();
		player.bounceAfterEnemyStomp();
		return true;
	}

	player.takeEnemyDamage(enemyBounds, 25);
	return true;
}

void GAME1_Enemy::draw(sf::RenderTarget& target) const
{
	if (!m_active)
		return;

	const sf::Texture* texture = getCurrentTexture();

	if (texture == nullptr)
		return;

	sf::Sprite sprite(*texture);
	const sf::FloatRect localBounds = sprite.getLocalBounds();

	if (localBounds.size.x <= 0.f || localBounds.size.y <= 0.f)
		return;

	const float scaleX = m_drawWidth / localBounds.size.x;
	const float scaleY = m_drawHeight / localBounds.size.y;

	if (m_facingDirection == FacingDirection::Right)
	{
		sprite.setScale({ scaleX, scaleY });
		sprite.setPosition(m_position);
	}
	else
	{
		sprite.setScale({ -scaleX, scaleY });
		sprite.setPosition({ m_position.x + m_drawWidth, m_position.y });
	}

	target.draw(sprite);
}

bool GAME1_Enemy::isActive() const
{
	return m_active;
}

bool GAME1_Enemy::isAlive() const
{
	return m_alive;
}

sf::FloatRect GAME1_Enemy::getBounds() const
{
	return sf::FloatRect(m_position, { m_drawWidth, m_drawHeight });
}

const std::string& GAME1_Enemy::getLastError() const
{
	return m_lastError;
}

const GAME1_Enemy::AnimationSet& GAME1_Enemy::getCurrentAnimationSet() const
{
	if (m_state == BehaviourState::Hit && !m_hitAnimation.frames.empty())
		return m_hitAnimation;

	if ((m_state == BehaviourState::Patrol || m_state == BehaviourState::Chase) && !m_runAnimation.frames.empty())
		return m_runAnimation;

	return m_idleAnimation;
}

const sf::Texture* GAME1_Enemy::getCurrentTexture() const
{
	const AnimationSet& animation = getCurrentAnimationSet();

	if (animation.frames.empty())
		return nullptr;

	return &animation.frames[m_currentFrameIndex % animation.frames.size()];
}

float GAME1_Enemy::randomFloat(float minValue, float maxValue)
{
	std::uniform_real_distribution<float> distribution(minValue, maxValue);
	return distribution(m_rng);
}

int GAME1_Enemy::randomInt(int minValue, int maxValue)
{
	std::uniform_int_distribution<int> distribution(minValue, maxValue);
	return distribution(m_rng);
}

bool GAME1_Enemy::rectsIntersect(const sf::FloatRect& a, const sf::FloatRect& b)
{
	return a.position.x < b.position.x + b.size.x &&
		a.position.x + a.size.x > b.position.x &&
		a.position.y < b.position.y + b.size.y &&
		a.position.y + a.size.y > b.position.y;
}
