#include "GAME1_SpikeHead.h"

#include "GAME1_Level.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <random>

namespace
{
	std::mt19937& SpikeHeadRng()
	{
		static std::mt19937 engine(std::random_device{}());
		return engine;
	}

	bool TextureIsUsable(const sf::Texture* texture)
	{
		if (texture == nullptr)
			return false;

		const sf::Vector2u size = texture->getSize();
		return size.x > 0 && size.y > 0;
	}

	static_assert(
		GAME1_Level::TileSize == static_cast<int>(GAME1_SpikeHeadTuning::TileSize),
		"SpikeHead trap tile size must match GAME1_Level::TileSize.");

	constexpr float TrapTileSize = static_cast<float>(GAME1_Level::TileSize);
	constexpr float TrapBodySize =
		TrapTileSize * static_cast<float>(GAME1_SpikeHeadTuning::FootprintTiles);

	int FloorDiv(float value)
	{
		return static_cast<int>(std::floor(value / TrapTileSize));
	}
}

bool GAME1_SpikeHeadAssets::load(const std::string& resourcesDirectory)
{
	namespace fs = std::filesystem;

	m_lastWarning.clear();
	m_hasIdleTexture = false;
	m_blinkFrames.clear();
	m_topHitFrames.clear();
	m_bottomHitFrames.clear();
	m_leftHitFrames.clear();
	m_rightHitFrames.clear();

	const fs::path spikeHeadDirectory =
		fs::path(resourcesDirectory) / "Tiles" / "Traps" / "SpikeHead";

	const fs::path idlePath = spikeHeadDirectory / "Idle.png";
	m_hasIdleTexture = m_idleTexture.loadFromFile(idlePath.string());
	if (!m_hasIdleTexture)
		warn("SurfersQuest SpikeHead idle texture failed to load: " + idlePath.string());

	loadSlices(spikeHeadDirectory / "Blink", 0, 3, m_blinkFrames);
	loadSlices(spikeHeadDirectory / "TopHit", 0, 3, m_topHitFrames);
	loadSlices(spikeHeadDirectory / "BottomHit", 0, 3, m_bottomHitFrames);
	loadSlices(spikeHeadDirectory / "LeftHit", 0, 3, m_leftHitFrames);
	loadSlices(spikeHeadDirectory / "RightHit", 0, 3, m_rightHitFrames);

	return true;
}

void GAME1_SpikeHeadAssets::loadSlices(const std::filesystem::path& directory,
	int firstIndex,
	int lastIndex,
	std::vector<sf::Texture>& outFrames)
{
	outFrames.clear();

	for (int i = firstIndex; i <= lastIndex; ++i)
	{
		const std::filesystem::path framePath =
			directory / ("slice_" + std::to_string(i) + ".png");

		sf::Texture texture;
		if (texture.loadFromFile(framePath.string()))
			outFrames.push_back(std::move(texture));
		else
			warn("SurfersQuest SpikeHead frame failed to load: " + framePath.string());
	}
}

void GAME1_SpikeHeadAssets::warn(const std::string& message)
{
	m_lastWarning = message;
	std::cerr << message << '\n';
}

const sf::Texture* GAME1_SpikeHeadAssets::getIdleTexture() const
{
	return m_hasIdleTexture ? &m_idleTexture : nullptr;
}

const std::vector<sf::Texture>& GAME1_SpikeHeadAssets::getBlinkFrames() const
{
	return m_blinkFrames;
}

const std::vector<sf::Texture>& GAME1_SpikeHeadAssets::getTopHitFrames() const
{
	return m_topHitFrames;
}

const std::vector<sf::Texture>& GAME1_SpikeHeadAssets::getBottomHitFrames() const
{
	return m_bottomHitFrames;
}

const std::vector<sf::Texture>& GAME1_SpikeHeadAssets::getLeftHitFrames() const
{
	return m_leftHitFrames;
}

const std::vector<sf::Texture>& GAME1_SpikeHeadAssets::getRightHitFrames() const
{
	return m_rightHitFrames;
}

const std::string& GAME1_SpikeHeadAssets::getLastWarning() const
{
	return m_lastWarning;
}

GAME1_SpikeHead::GAME1_SpikeHead(sf::Vector2i gridPosition)
	: m_gridPosition(gridPosition),
	m_originalGridPosition(gridPosition)
{
	reset();
}

void GAME1_SpikeHead::setAssets(const GAME1_SpikeHeadAssets* assets)
{
	m_assets = assets;
}

void GAME1_SpikeHead::reset()
{
	// Return to the placed cell: drive the world position from the grid cell
	// (the inverse of snapToGrid, which derives the grid cell from a moved body).
	m_gridPosition = m_originalGridPosition;
	m_position = {
		static_cast<float>(m_gridPosition.x) * TrapTileSize,
		static_cast<float>(m_gridPosition.y) * TrapTileSize
	};

	m_state = State::Idle;
	m_slamDirection = GAME1_SpikeHeadDirection::Down;
	m_hitDirection = GAME1_SpikeHeadDirection::Down;

	m_blinking = false;
	m_blinkDelayTimer = randomFloat(
		GAME1_SpikeHeadTuning::BlinkMinDelay,
		GAME1_SpikeHeadTuning::BlinkMaxDelay);
	m_idleCooldown = 0.f;
	m_attackDelayTimer = 0.f;

	m_animationTimer = 0.f;
	m_frameIndex = 0;
}

void GAME1_SpikeHead::update(float deltaTime,
	const GAME1_Level& level,
	const std::vector<sf::FloatRect>& playerBounds)
{
	switch (m_state)
	{
	case State::Idle:
		updateIdle(deltaTime, playerBounds);
		break;

	case State::Windup:
		updateWindup(deltaTime);
		break;

	case State::Slam:
		updateSlam(deltaTime, level);
		break;

	case State::Hit:
		updateHit(deltaTime);
		break;
	}
}

void GAME1_SpikeHead::updateIdle(float deltaTime,
	const std::vector<sf::FloatRect>& playerBounds)
{
	if (m_idleCooldown > 0.f)
		m_idleCooldown = std::max(0.f, m_idleCooldown - deltaTime);

	// Only acquire a target once the post-impact settle has elapsed so a fresh
	// slam cannot begin on the same frame an impact resolves.
	if (m_idleCooldown <= 0.f)
	{
		GAME1_SpikeHeadDirection direction;
		if (tryAcquireTarget(playerBounds, direction))
		{
			beginWindup(direction);
			return;
		}
	}

	updateBlink(deltaTime);
}

void GAME1_SpikeHead::updateBlink(float deltaTime)
{
	const std::vector<sf::Texture>* blinkFrames =
		m_assets != nullptr ? &m_assets->getBlinkFrames() : nullptr;
	const bool hasBlinkFrames = blinkFrames != nullptr && !blinkFrames->empty();

	if (m_blinking)
	{
		if (!hasBlinkFrames)
		{
			m_blinking = false;
			m_frameIndex = 0;
			return;
		}

		m_animationTimer += deltaTime;

		while (m_animationTimer >= GAME1_SpikeHeadTuning::BlinkFrameDuration)
		{
			m_animationTimer -= GAME1_SpikeHeadTuning::BlinkFrameDuration;
			++m_frameIndex;

			if (m_frameIndex >= blinkFrames->size())
			{
				// Blink finished: back to the static idle sprite and schedule the
				// next blink at a randomised delay so it never looks periodic.
				m_blinking = false;
				m_frameIndex = 0;
				m_blinkDelayTimer = randomFloat(
					GAME1_SpikeHeadTuning::BlinkMinDelay,
					GAME1_SpikeHeadTuning::BlinkMaxDelay);
				break;
			}
		}

		return;
	}

	m_blinkDelayTimer -= deltaTime;

	if (m_blinkDelayTimer <= 0.f && hasBlinkFrames)
	{
		m_blinking = true;
		m_frameIndex = 0;
		m_animationTimer = 0.f;
	}
}

bool GAME1_SpikeHead::tryAcquireTarget(const std::vector<sf::FloatRect>& playerBounds,
	GAME1_SpikeHeadDirection& outDirection) const
{
	const sf::FloatRect body = getBodyBounds();
	const sf::Vector2f bodyCenter(
		body.position.x + body.size.x * 0.5f,
		body.position.y + body.size.y * 0.5f);

	const GAME1_SpikeHeadDirection directions[4] =
	{
		GAME1_SpikeHeadDirection::Up,
		GAME1_SpikeHeadDirection::Down,
		GAME1_SpikeHeadDirection::Left,
		GAME1_SpikeHeadDirection::Right
	};

	bool found = false;
	float bestDistanceSquared = 0.f;

	for (const sf::FloatRect& bounds : playerBounds)
	{
		const sf::Vector2f playerCenter(
			bounds.position.x + bounds.size.x * 0.5f,
			bounds.position.y + bounds.size.y * 0.5f);

		const float dx = playerCenter.x - bodyCenter.x;
		const float dy = playerCenter.y - bodyCenter.y;
		const float distanceSquared = dx * dx + dy * dy;

		for (GAME1_SpikeHeadDirection direction : directions)
		{
			if (!rectsIntersect(bounds, armBounds(direction)))
				continue;

			if (!found || distanceSquared < bestDistanceSquared)
			{
				found = true;
				bestDistanceSquared = distanceSquared;
				outDirection = direction;
			}
		}
	}

	return found;
}

sf::FloatRect GAME1_SpikeHead::armBounds(GAME1_SpikeHeadDirection direction) const
{
	const float tile = TrapTileSize;
	const float body = TrapBodySize;
	const float reach = tile * static_cast<float>(GAME1_SpikeHeadTuning::DetectionRangeTiles);
	const sf::Vector2f origin = m_position;

	switch (direction)
	{
	case GAME1_SpikeHeadDirection::Up:
		return sf::FloatRect({ origin.x, origin.y - reach }, { body, reach });

	case GAME1_SpikeHeadDirection::Down:
		return sf::FloatRect({ origin.x, origin.y + body }, { body, reach });

	case GAME1_SpikeHeadDirection::Left:
		return sf::FloatRect({ origin.x - reach, origin.y }, { reach, body });

	case GAME1_SpikeHeadDirection::Right:
	default:
		return sf::FloatRect({ origin.x + body, origin.y }, { reach, body });
	}
}

void GAME1_SpikeHead::beginWindup(GAME1_SpikeHeadDirection direction)
{
	m_state = State::Windup;
	m_slamDirection = direction;
	m_attackDelayTimer = GAME1_SpikeHeadTuning::AttackDelaySeconds;
	m_blinking = false;
	m_frameIndex = 0;
	m_animationTimer = 0.f;
}

void GAME1_SpikeHead::updateWindup(float deltaTime)
{
	m_attackDelayTimer = std::max(0.f, m_attackDelayTimer - deltaTime);

	if (m_attackDelayTimer <= 0.f)
		beginSlam(m_slamDirection);
}

void GAME1_SpikeHead::beginSlam(GAME1_SpikeHeadDirection direction)
{
	m_state = State::Slam;
	m_slamDirection = direction;
	m_blinking = false;
	m_frameIndex = 0;
	m_animationTimer = 0.f;
}

void GAME1_SpikeHead::updateSlam(float deltaTime, const GAME1_Level& level)
{
	const float body = TrapBodySize;
	const float tile = TrapTileSize;
	const float step = GAME1_SpikeHeadTuning::SlamSpeed * deltaTime;

	// The two perpendicular cells the leading edge spans stay fixed during a
	// cardinal slam because the body is grid-aligned on that axis.
	const int colA = FloorDiv(m_position.x + 1.f);
	const int colB = FloorDiv(m_position.x + body - 1.f);
	const int rowA = FloorDiv(m_position.y + 1.f);
	const int rowB = FloorDiv(m_position.y + body - 1.f);

	bool blocked = false;

	switch (m_slamDirection)
	{
	case GAME1_SpikeHeadDirection::Down:
	{
		float desiredTop = m_position.y + step;
		const float desiredBottom = desiredTop + body;

		const int startRow = FloorDiv(m_position.y + body);
		const int endRow = FloorDiv(desiredBottom - 0.01f);

		for (int row = startRow; row <= endRow; ++row)
		{
			if (isSlamBlocked(level, colA, row) || isSlamBlocked(level, colB, row))
			{
				desiredTop = static_cast<float>(row) * tile - body;
				blocked = true;
				break;
			}
		}

		m_position.y = desiredTop;
		break;
	}

	case GAME1_SpikeHeadDirection::Up:
	{
		float desiredTop = m_position.y - step;

		const int startRow = FloorDiv(m_position.y - 1.f);
		const int endRow = FloorDiv(desiredTop);

		for (int row = startRow; row >= endRow; --row)
		{
			if (isSlamBlocked(level, colA, row) || isSlamBlocked(level, colB, row))
			{
				desiredTop = static_cast<float>(row + 1) * tile;
				blocked = true;
				break;
			}
		}

		m_position.y = desiredTop;
		break;
	}

	case GAME1_SpikeHeadDirection::Left:
	{
		float desiredLeft = m_position.x - step;

		const int startCol = FloorDiv(m_position.x - 1.f);
		const int endCol = FloorDiv(desiredLeft);

		for (int col = startCol; col >= endCol; --col)
		{
			if (isSlamBlocked(level, col, rowA) || isSlamBlocked(level, col, rowB))
			{
				desiredLeft = static_cast<float>(col + 1) * tile;
				blocked = true;
				break;
			}
		}

		m_position.x = desiredLeft;
		break;
	}

	case GAME1_SpikeHeadDirection::Right:
	{
		float desiredLeft = m_position.x + step;
		const float desiredRight = desiredLeft + body;

		const int startCol = FloorDiv(m_position.x + body);
		const int endCol = FloorDiv(desiredRight - 0.01f);

		for (int col = startCol; col <= endCol; ++col)
		{
			if (isSlamBlocked(level, col, rowA) || isSlamBlocked(level, col, rowB))
			{
				desiredLeft = static_cast<float>(col) * tile - body;
				blocked = true;
				break;
			}
		}

		m_position.x = desiredLeft;
		break;
	}
	}

	if (blocked)
	{
		m_hitDirection = m_slamDirection;
		snapToGrid();
		beginHit();
	}
}

void GAME1_SpikeHead::beginHit()
{
	m_state = State::Hit;
	m_frameIndex = 0;
	m_animationTimer = 0.f;
}

void GAME1_SpikeHead::updateHit(float deltaTime)
{
	const std::vector<sf::Texture>& frames = currentHitFrames();

	if (frames.empty())
	{
		// No impact art available: settle immediately at the new position.
		m_state = State::Idle;
		m_frameIndex = 0;
		m_idleCooldown = GAME1_SpikeHeadTuning::PostHitIdleCooldown;
		m_blinkDelayTimer = randomFloat(
			GAME1_SpikeHeadTuning::BlinkMinDelay,
			GAME1_SpikeHeadTuning::BlinkMaxDelay);
		return;
	}

	m_animationTimer += deltaTime;

	while (m_animationTimer >= GAME1_SpikeHeadTuning::HitFrameDuration)
	{
		m_animationTimer -= GAME1_SpikeHeadTuning::HitFrameDuration;
		++m_frameIndex;

		if (m_frameIndex >= frames.size())
		{
			// Impact finished: this stopping point is the new idle position.
			m_state = State::Idle;
			m_frameIndex = 0;
			m_blinking = false;
			m_idleCooldown = GAME1_SpikeHeadTuning::PostHitIdleCooldown;
			m_blinkDelayTimer = randomFloat(
				GAME1_SpikeHeadTuning::BlinkMinDelay,
				GAME1_SpikeHeadTuning::BlinkMaxDelay);
			return;
		}
	}
}

void GAME1_SpikeHead::snapToGrid()
{
	const float tile = TrapTileSize;

	m_gridPosition.x = static_cast<int>(std::lround(m_position.x / tile));
	m_gridPosition.y = static_cast<int>(std::lround(m_position.y / tile));

	m_position = {
		static_cast<float>(m_gridPosition.x) * tile,
		static_cast<float>(m_gridPosition.y) * tile
	};
}

bool GAME1_SpikeHead::isSlamBlocked(const GAME1_Level& level, int col, int row) const
{
	// The level boundary blocks a slam just like a solid wall would, so the body
	// never flies off into empty space outside the map.
	if (col < 0 || row < 0 ||
		col >= level.getWidthInTiles() || row >= level.getHeightInTiles())
	{
		return true;
	}

	return level.isSolidTile(col, row);
}

void GAME1_SpikeHead::draw(sf::RenderTarget& target) const
{
	const sf::FloatRect box = getBodyBounds();
	const sf::Texture* texture = currentTexture();

	if (TextureIsUsable(texture))
	{
		sf::Sprite sprite(*texture);
		const sf::FloatRect localBounds = sprite.getLocalBounds();

		if (localBounds.size.x > 0.f && localBounds.size.y > 0.f)
		{
			// Uniform fit so the sprite keeps its aspect ratio (no uneven
			// squash/stretch), centred inside the 2x2 body box.
			const float scale = std::min(
				box.size.x / localBounds.size.x,
				box.size.y / localBounds.size.y);

			sprite.setOrigin({
				localBounds.position.x + localBounds.size.x * 0.5f,
				localBounds.position.y + localBounds.size.y * 0.5f
				});

			sprite.setPosition({
				box.position.x + box.size.x * 0.5f,
				box.position.y + box.size.y * 0.5f
				});

			sprite.setScale({ scale, scale });
			target.draw(sprite);
			return;
		}
	}

	sf::RectangleShape fallback;
	fallback.setPosition(box.position);
	fallback.setSize(box.size);
	fallback.setFillColor(sf::Color(150, 150, 160));
	fallback.setOutlineColor(sf::Color::White);
	fallback.setOutlineThickness(1.f);
	target.draw(fallback);
}

const std::vector<sf::Texture>& GAME1_SpikeHead::currentHitFrames() const
{
	static const std::vector<sf::Texture> empty;

	if (m_assets == nullptr)
		return empty;

	switch (m_hitDirection)
	{
	case GAME1_SpikeHeadDirection::Up:
		return m_assets->getTopHitFrames();

	case GAME1_SpikeHeadDirection::Down:
		return m_assets->getBottomHitFrames();

	case GAME1_SpikeHeadDirection::Left:
		return m_assets->getLeftHitFrames();

	case GAME1_SpikeHeadDirection::Right:
	default:
		return m_assets->getRightHitFrames();
	}
}

const sf::Texture* GAME1_SpikeHead::currentTexture() const
{
	if (m_assets == nullptr)
		return nullptr;

	if (m_state == State::Hit)
	{
		const std::vector<sf::Texture>& frames = currentHitFrames();
		if (frames.empty())
			return m_assets->getIdleTexture();

		return &frames[std::min(m_frameIndex, frames.size() - 1)];
	}

	if (m_state == State::Idle && m_blinking)
	{
		const std::vector<sf::Texture>& frames = m_assets->getBlinkFrames();
		if (!frames.empty())
			return &frames[std::min(m_frameIndex, frames.size() - 1)];
	}

	// Idle (static) and Slam both show the resting spike-ball sprite.
	return m_assets->getIdleTexture();
}

sf::Vector2i GAME1_SpikeHead::getGridPosition() const
{
	return m_gridPosition;
}

sf::FloatRect GAME1_SpikeHead::getBodyBounds() const
{
	return sf::FloatRect(
		m_position,
		{ TrapBodySize, TrapBodySize });
}

sf::FloatRect GAME1_SpikeHead::getHazardBounds() const
{
	const float inset = GAME1_SpikeHeadTuning::HazardInset;
	const sf::FloatRect body = getBodyBounds();

	// Inset both sides, clamped so the rect can never collapse to a negative
	// size. This is the BODY, not the detection cross (armBounds).
	const float width = std::max(1.f, body.size.x - inset * 2.f);
	const float height = std::max(1.f, body.size.y - inset * 2.f);

	const sf::FloatRect hazard(
		{ body.position.x + inset, body.position.y + inset },
		{ width, height });

	// Debug-only sanity: the hazard must stay a small box around the 2x2 body
	// (never negative, never larger than the 2x2 footprint / detection range).
	assert(hazard.size.x > 0.f && hazard.size.x <= TrapBodySize);
	assert(hazard.size.y > 0.f && hazard.size.y <= TrapBodySize);

	return hazard;
}

bool GAME1_SpikeHead::rectsIntersect(const sf::FloatRect& a, const sf::FloatRect& b)
{
	return a.position.x < b.position.x + b.size.x &&
		a.position.x + a.size.x > b.position.x &&
		a.position.y < b.position.y + b.size.y &&
		a.position.y + a.size.y > b.position.y;
}

float GAME1_SpikeHead::randomFloat(float minValue, float maxValue)
{
	std::uniform_real_distribution<float> distribution(minValue, maxValue);
	return distribution(SpikeHeadRng());
}
