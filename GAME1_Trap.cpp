#include "GAME1_Trap.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <filesystem>
#include <iostream>
#include <sstream>

namespace
{
	struct TrapDefinition
	{
		GAME1_TrapType type;
		const char* name;
		const char* editorLabel;
		sf::Color fallbackColor;
	};

	const std::vector<TrapDefinition>& TrapDefinitions()
	{
		static const std::vector<TrapDefinition> definitions =
		{
			{ GAME1_TrapType::FallingPlatform, "FallingPlatform", "Fall", sf::Color(170, 120, 70) },
			{ GAME1_TrapType::Fan, "Fan", "Fan", sf::Color(95, 180, 230) },
			{ GAME1_TrapType::Fire, "Fire", "Fire", sf::Color(240, 90, 35) },
			{ GAME1_TrapType::Chain, "Chain", "Chain", sf::Color(190, 190, 190) },
			{ GAME1_TrapType::BrownMovingPlatform, "BrownMovingPlatform", "Brown", sf::Color(150, 90, 45) },
			{ GAME1_TrapType::GreyMovingPlatform, "GreyMovingPlatform", "Grey", sf::Color(150, 160, 170) }
		};

		return definitions;
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

	std::size_t TrapIndex(GAME1_TrapType type)
	{
		const std::vector<TrapDefinition>& definitions = TrapDefinitions();

		for (std::size_t i = 0; i < definitions.size(); ++i)
		{
			if (definitions[i].type == type)
				return i;
		}

		return 0;
	}

	const TrapDefinition& GetTrapDefinition(GAME1_TrapType type)
	{
		return TrapDefinitions()[TrapIndex(type)];
	}

	bool TextureIsUsable(const sf::Texture& texture)
	{
		const sf::Vector2u size = texture.getSize();
		return size.x > 0 && size.y > 0;
	}
}

const std::vector<GAME1_TrapType>& GAME1_GetAllTrapTypes()
{
	static const std::vector<GAME1_TrapType> types =
	{
		GAME1_TrapType::FallingPlatform,
		GAME1_TrapType::Fan,
		GAME1_TrapType::Fire,
		GAME1_TrapType::Chain,
		GAME1_TrapType::BrownMovingPlatform,
		GAME1_TrapType::GreyMovingPlatform
	};

	return types;
}

std::string GAME1_GetTrapTypeName(GAME1_TrapType type)
{
	return GetTrapDefinition(type).name;
}

std::string GAME1_GetTrapEditorLabel(GAME1_TrapType type)
{
	return GetTrapDefinition(type).editorLabel;
}

sf::Color GAME1_GetTrapFallbackColor(GAME1_TrapType type)
{
	return GetTrapDefinition(type).fallbackColor;
}

bool GAME1_TryParseTrapType(const std::string& value, GAME1_TrapType& outType)
{
	const std::string lowered = ToLower(value);

	for (const TrapDefinition& definition : TrapDefinitions())
	{
		if (ToLower(definition.name) == lowered)
		{
			outType = definition.type;
			return true;
		}
	}

	return false;
}

int GAME1_GetTrapOrientationValue(GAME1_TrapOrientation orientation)
{
	return static_cast<int>(orientation);
}

GAME1_TrapOrientation GAME1_NormalizeTrapOrientation(int value)
{
	int normalized = value % 4;
	if (normalized < 0)
		normalized += 4;

	return static_cast<GAME1_TrapOrientation>(normalized);
}

GAME1_TrapOrientation GAME1_RotateTrapOrientation(GAME1_TrapOrientation orientation, int quarterTurnsClockwise)
{
	return GAME1_NormalizeTrapOrientation(
		GAME1_GetTrapOrientationValue(orientation) + quarterTurnsClockwise);
}

sf::Vector2f GAME1_GetTrapOrientationDirection(GAME1_TrapOrientation orientation)
{
	switch (orientation)
	{
	case GAME1_TrapOrientation::Right:
		return { 1.f, 0.f };

	case GAME1_TrapOrientation::Down:
		return { 0.f, 1.f };

	case GAME1_TrapOrientation::Left:
		return { -1.f, 0.f };

	case GAME1_TrapOrientation::Up:
	default:
		return { 0.f, -1.f };
	}
}

bool GAME1_IsRotatableTrapType(GAME1_TrapType type)
{
	return type == GAME1_TrapType::Fan || type == GAME1_TrapType::Fire;
}

bool GAME1_IsMovingPlatformTrapType(GAME1_TrapType type)
{
	return type == GAME1_TrapType::BrownMovingPlatform ||
		type == GAME1_TrapType::GreyMovingPlatform;
}

bool GAME1_TryParseTrapObjectLine(const std::string& line, GAME1_TrapSpawn& outSpawn)
{
	std::istringstream stream(line);

	std::string marker;
	std::string typeName;
	int col = 0;
	int row = 0;
	int orientation = 0;

	if (!(stream >> marker >> typeName >> col >> row))
		return false;

	if (marker != "OBJECT")
		return false;

	if (!(stream >> orientation))
		orientation = 0;

	GAME1_TrapType type = GAME1_TrapType::FallingPlatform;
	if (!GAME1_TryParseTrapType(typeName, type))
		return false;

	outSpawn.type = type;
	outSpawn.gridPosition = { col, row };
	outSpawn.orientation = GAME1_NormalizeTrapOrientation(orientation);
	return true;
}

std::string GAME1_BuildTrapObjectLine(const GAME1_TrapSpawn& spawn)
{
	std::ostringstream stream;
	stream << "OBJECT "
		<< GAME1_GetTrapTypeName(spawn.type) << ' '
		<< spawn.gridPosition.x << ' '
		<< spawn.gridPosition.y << ' '
		<< GAME1_GetTrapOrientationValue(spawn.orientation);

	return stream.str();
}

bool GAME1_TrapAssets::load(const std::string& resourcesDirectory)
{
	namespace fs = std::filesystem;

	m_lastWarning.clear();
	m_fallingPlatformFrames.clear();
	m_fanFrames.clear();
	m_fireReloadFrames.clear();
	m_fireOnFrames.clear();
	m_brownPlatformFrames.clear();
	m_greyPlatformFrames.clear();
	m_hasChainTexture = false;

	const fs::path trapsDirectory = fs::path(resourcesDirectory) / "Tiles" / "Traps";

	loadFrames(
		trapsDirectory / "FallingPlatform",
		"FallingPlatOn_",
		0,
		3,
		m_fallingPlatformFrames);

	loadFrames(
		trapsDirectory / "Fan",
		"FanOn_",
		0,
		3,
		m_fanFrames);

	loadFrames(
		trapsDirectory / "Fire",
		"FireReload_",
		0,
		3,
		m_fireReloadFrames);

	loadFrames(
		trapsDirectory / "Fire",
		"FireOn_",
		0,
		2,
		m_fireOnFrames);

	loadFrames(
		trapsDirectory / "MovablePlat",
		"BrownOn_",
		0,
		7,
		m_brownPlatformFrames);

	loadFrames(
		trapsDirectory / "MovablePlat",
		"GreyOn_",
		0,
		7,
		m_greyPlatformFrames);

	loadSingleTexture(
		trapsDirectory / "MovablePlat" / "Chain.png",
		m_chainTexture,
		m_hasChainTexture);

	return true;
}

void GAME1_TrapAssets::loadFrames(const std::filesystem::path& directory,
	const std::string& prefix,
	int firstIndex,
	int lastIndex,
	std::vector<sf::Texture>& outFrames)
{
	outFrames.clear();

	for (int i = firstIndex; i <= lastIndex; ++i)
	{
		const std::filesystem::path framePath =
			directory / (prefix + std::to_string(i) + ".png");

		sf::Texture texture;
		if (texture.loadFromFile(framePath.string()))
		{
			outFrames.push_back(std::move(texture));
		}
		else
		{
			warn("SurfersQuest trap frame failed to load: " + framePath.string());
		}
	}
}

void GAME1_TrapAssets::loadSingleTexture(const std::filesystem::path& path,
	sf::Texture& outTexture,
	bool& outLoaded)
{
	outLoaded = outTexture.loadFromFile(path.string());

	if (!outLoaded)
		warn("SurfersQuest trap texture failed to load: " + path.string());
}

void GAME1_TrapAssets::warn(const std::string& message)
{
	m_lastWarning = message;
	std::cerr << message << '\n';
}

const std::vector<sf::Texture>& GAME1_TrapAssets::getFallingPlatformFrames() const
{
	return m_fallingPlatformFrames;
}

const std::vector<sf::Texture>& GAME1_TrapAssets::getFanFrames() const
{
	return m_fanFrames;
}

const std::vector<sf::Texture>& GAME1_TrapAssets::getFireReloadFrames() const
{
	return m_fireReloadFrames;
}

const std::vector<sf::Texture>& GAME1_TrapAssets::getFireOnFrames() const
{
	return m_fireOnFrames;
}

const std::vector<sf::Texture>& GAME1_TrapAssets::getBrownPlatformFrames() const
{
	return m_brownPlatformFrames;
}

const std::vector<sf::Texture>& GAME1_TrapAssets::getGreyPlatformFrames() const
{
	return m_greyPlatformFrames;
}

const sf::Texture* GAME1_TrapAssets::getChainTexture() const
{
	return m_hasChainTexture ? &m_chainTexture : nullptr;
}

const sf::Texture* GAME1_TrapAssets::getEditorIconTexture(GAME1_TrapType type) const
{
	switch (type)
	{
	case GAME1_TrapType::FallingPlatform:
		return m_fallingPlatformFrames.empty() ? nullptr : &m_fallingPlatformFrames.front();

	case GAME1_TrapType::Fan:
		return m_fanFrames.empty() ? nullptr : &m_fanFrames.front();

	case GAME1_TrapType::Fire:
		return m_fireOnFrames.empty() ? nullptr : &m_fireOnFrames.front();

	case GAME1_TrapType::Chain:
		return getChainTexture();

	case GAME1_TrapType::BrownMovingPlatform:
		return m_brownPlatformFrames.empty() ? nullptr : &m_brownPlatformFrames.front();

	case GAME1_TrapType::GreyMovingPlatform:
		return m_greyPlatformFrames.empty() ? nullptr : &m_greyPlatformFrames.front();

	default:
		return nullptr;
	}
}

const std::string& GAME1_TrapAssets::getLastWarning() const
{
	return m_lastWarning;
}

GAME1_Trap::GAME1_Trap(const GAME1_TrapSpawn& spawn)
	: m_type(spawn.type),
	m_gridPosition(spawn.gridPosition),
	m_orientation(spawn.orientation)
{
	m_originalPosition = {
		static_cast<float>(m_gridPosition.x) * GAME1_TrapTuning::TileSize,
		static_cast<float>(m_gridPosition.y) * GAME1_TrapTuning::TileSize
	};

	resetRuntime();
}

void GAME1_Trap::resetRuntime()
{
	m_position = m_originalPosition;
	m_previousPosition = m_originalPosition;
	m_delta = { 0.f, 0.f };
	m_playerStandingThisFrame = false;
	m_chainPathEnabled = false;
	m_chainPathHorizontal = true;
	m_chainPathStart = m_gridPosition;
	m_chainPathEnd = m_gridPosition;
	m_chainDirection = 1.f;
	m_animationTimer = 0.f;
	m_frameIndex = 0;
	m_firePhase = FirePhase::Reload;
	m_fireLoopsCompleted = 0;
}

void GAME1_Trap::configureChainPath(sf::Vector2i startGrid,
	sf::Vector2i endGrid,
	bool horizontal,
	bool enabled)
{
	m_chainPathStart = startGrid;
	m_chainPathEnd = endGrid;
	m_chainPathHorizontal = horizontal;
	m_chainPathEnabled = enabled;
}

void GAME1_Trap::update(float deltaTime)
{
	const bool playerWasStanding = m_playerStandingThisFrame;
	m_playerStandingThisFrame = false;

	m_previousPosition = m_position;

	switch (m_type)
	{
	case GAME1_TrapType::FallingPlatform:
	{
		updateLoopingAnimation(deltaTime, 4);

		const float targetY = playerWasStanding
			? m_originalPosition.y + GAME1_TrapTuning::FallingPlatformMaxDrop
			: m_originalPosition.y;

		m_position.y = moveTowards(
			m_position.y,
			targetY,
			GAME1_TrapTuning::FallingPlatformSpeed * deltaTime);
		break;
	}

	case GAME1_TrapType::Fan:
		updateLoopingAnimation(deltaTime, 4);
		break;

	case GAME1_TrapType::Fire:
		updateFireAnimation(deltaTime);
		break;

	case GAME1_TrapType::BrownMovingPlatform:
	case GAME1_TrapType::GreyMovingPlatform:
	{
		updateLoopingAnimation(deltaTime, 8);

		if (m_chainPathEnabled)
		{
			const float minCoordinate = static_cast<float>(
				(m_chainPathHorizontal ? m_chainPathStart.x : m_chainPathStart.y)) *
				GAME1_TrapTuning::TileSize;

			const float maxCoordinate = static_cast<float>(
				(m_chainPathHorizontal ? m_chainPathEnd.x : m_chainPathEnd.y)) *
				GAME1_TrapTuning::TileSize;

			float& coordinate = m_chainPathHorizontal ? m_position.x : m_position.y;
			coordinate += m_chainDirection * getMovingPlatformSpeed() * deltaTime;

			if (coordinate > maxCoordinate)
			{
				coordinate = maxCoordinate;
				m_chainDirection = -1.f;
			}
			else if (coordinate < minCoordinate)
			{
				coordinate = minCoordinate;
				m_chainDirection = 1.f;
			}
		}
		break;
	}

	case GAME1_TrapType::Chain:
	default:
		break;
	}

	m_delta = {
		m_position.x - m_previousPosition.x,
		m_position.y - m_previousPosition.y
	};
}

void GAME1_Trap::draw(sf::RenderTarget& target, const GAME1_TrapAssets& assets) const
{
	const sf::Texture* texture = nullptr;
	sf::FloatRect bounds = getTileBounds();
	GAME1_TrapOrientation drawOrientation = m_orientation;

	switch (m_type)
	{
	case GAME1_TrapType::FallingPlatform:
	{
		const std::vector<sf::Texture>& frames = assets.getFallingPlatformFrames();
		texture = frames.empty() ? nullptr : &frames[m_frameIndex % frames.size()];
		drawOrientation = GAME1_TrapOrientation::Up;
		break;
	}

	case GAME1_TrapType::Fan:
	{
		const std::vector<sf::Texture>& frames = assets.getFanFrames();
		texture = frames.empty() ? nullptr : &frames[m_frameIndex % frames.size()];
		break;
	}

	case GAME1_TrapType::Fire:
	{
		if (isActiveFire())
		{
			const std::vector<sf::Texture>& frames = assets.getFireOnFrames();
			texture = frames.empty() ? nullptr : &frames[m_frameIndex % frames.size()];
			bounds = getFireActiveBounds();
		}
		else
		{
			const std::vector<sf::Texture>& frames = assets.getFireReloadFrames();
			texture = frames.empty() ? nullptr : &frames[m_frameIndex % frames.size()];
		}
		break;
	}

	case GAME1_TrapType::Chain:
		texture = assets.getChainTexture();
		drawOrientation = GAME1_TrapOrientation::Up;
		break;

	case GAME1_TrapType::BrownMovingPlatform:
	{
		const std::vector<sf::Texture>& frames = assets.getBrownPlatformFrames();
		texture = frames.empty() ? nullptr : &frames[m_frameIndex % frames.size()];
		drawOrientation = GAME1_TrapOrientation::Up;
		break;
	}

	case GAME1_TrapType::GreyMovingPlatform:
	{
		const std::vector<sf::Texture>& frames = assets.getGreyPlatformFrames();
		texture = frames.empty() ? nullptr : &frames[m_frameIndex % frames.size()];
		drawOrientation = GAME1_TrapOrientation::Up;
		break;
	}

	default:
		break;
	}

	if (texture != nullptr && TextureIsUsable(*texture))
	{
		drawTextureFitted(target, *texture, bounds, drawOrientation);
		return;
	}

	drawFallback(target, bounds, GAME1_GetTrapFallbackColor(m_type));
}

GAME1_TrapType GAME1_Trap::getType() const
{
	return m_type;
}

sf::Vector2i GAME1_Trap::getGridPosition() const
{
	return m_gridPosition;
}

GAME1_TrapOrientation GAME1_Trap::getOrientation() const
{
	return m_orientation;
}

sf::Vector2f GAME1_Trap::getPosition() const
{
	return m_position;
}

sf::Vector2f GAME1_Trap::getDelta() const
{
	return m_delta;
}

bool GAME1_Trap::isChain() const
{
	return m_type == GAME1_TrapType::Chain;
}

bool GAME1_Trap::isPlatform() const
{
	return m_type == GAME1_TrapType::FallingPlatform || isMovingPlatform();
}

bool GAME1_Trap::isMovingPlatform() const
{
	return GAME1_IsMovingPlatformTrapType(m_type);
}

bool GAME1_Trap::isActiveFire() const
{
	return m_type == GAME1_TrapType::Fire && m_firePhase == FirePhase::Active;
}

void GAME1_Trap::markPlayerStanding()
{
	m_playerStandingThisFrame = true;
}

std::optional<GAME1_TrapPlatformContact> GAME1_Trap::getPlatformContact(
	std::size_t trapIndex,
	const sf::FloatRect& playerBounds,
	float previousPlayerBottom) const
{
	if (!isPlatform())
		return std::nullopt;

	const sf::FloatRect platformBounds = getPlatformBounds();
	const float playerBottom = playerBounds.position.y + playerBounds.size.y;
	const float platformTop = platformBounds.position.y;
	const float previousPlatformTop = m_previousPosition.y;

	const bool horizontalOverlap =
		playerBounds.position.x < platformBounds.position.x + platformBounds.size.x &&
		playerBounds.position.x + playerBounds.size.x > platformBounds.position.x;

	if (!horizontalOverlap)
		return std::nullopt;

	const bool crossedTop =
		previousPlayerBottom <= platformTop + 6.f &&
		playerBottom >= platformTop - 1.f;

	const bool wasAlreadyStanding =
		previousPlayerBottom >= previousPlatformTop - 6.f &&
		previousPlayerBottom <= previousPlatformTop + 6.f;

	if (!crossedTop && !wasAlreadyStanding)
		return std::nullopt;

	GAME1_TrapPlatformContact contact;
	contact.trapIndex = trapIndex;
	contact.bounds = platformBounds;
	contact.delta = m_delta;
	return contact;
}

std::optional<sf::FloatRect> GAME1_Trap::getFanForceBounds() const
{
	if (m_type != GAME1_TrapType::Fan)
		return std::nullopt;

	const float tile = GAME1_TrapTuning::TileSize;
	const sf::Vector2f base = m_originalPosition;

	switch (m_orientation)
	{
	case GAME1_TrapOrientation::Right:
		return sf::FloatRect({ base.x + tile, base.y }, { tile * 2.f, tile });

	case GAME1_TrapOrientation::Down:
		return sf::FloatRect({ base.x, base.y + tile }, { tile, tile * 2.f });

	case GAME1_TrapOrientation::Left:
		return sf::FloatRect({ base.x - tile * 2.f, base.y }, { tile * 2.f, tile });

	case GAME1_TrapOrientation::Up:
	default:
		return sf::FloatRect({ base.x, base.y - tile * 2.f }, { tile, tile * 2.f });
	}
}

std::optional<sf::FloatRect> GAME1_Trap::getFireDamageBounds() const
{
	if (!isActiveFire())
		return std::nullopt;

	return getFireActiveBounds();
}

sf::Vector2f GAME1_Trap::getFanDirection() const
{
	return GAME1_GetTrapOrientationDirection(m_orientation);
}

void GAME1_Trap::updateLoopingAnimation(float deltaTime, std::size_t frameCount)
{
	if (frameCount <= 1)
		return;

	m_animationTimer += deltaTime;

	while (m_animationTimer >= GAME1_TrapTuning::TrapAnimationFrameDuration)
	{
		m_animationTimer -= GAME1_TrapTuning::TrapAnimationFrameDuration;
		m_frameIndex = (m_frameIndex + 1) % frameCount;
	}
}

void GAME1_Trap::updateFireAnimation(float deltaTime)
{
	const std::size_t frameCount =
		m_firePhase == FirePhase::Reload
		? 4u
		: 3u;

	if (frameCount <= 1)
		return;

	m_animationTimer += deltaTime;

	while (m_animationTimer >= GAME1_TrapTuning::FireFrameDuration)
	{
		m_animationTimer -= GAME1_TrapTuning::FireFrameDuration;
		++m_frameIndex;

		if (m_frameIndex < frameCount)
			continue;

		m_frameIndex = 0;
		++m_fireLoopsCompleted;

		if (m_firePhase == FirePhase::Reload &&
			m_fireLoopsCompleted >= GAME1_TrapTuning::FireReloadLoopCount)
		{
			m_firePhase = FirePhase::Active;
			m_fireLoopsCompleted = 0;
		}
		else if (m_firePhase == FirePhase::Active &&
			m_fireLoopsCompleted >= GAME1_TrapTuning::FireActiveLoopCount)
		{
			m_firePhase = FirePhase::Reload;
			m_fireLoopsCompleted = 0;
		}
	}
}

float GAME1_Trap::getMovingPlatformSpeed() const
{
	return m_type == GAME1_TrapType::GreyMovingPlatform
		? GAME1_TrapTuning::GreyPlatformSpeed
		: GAME1_TrapTuning::BrownPlatformSpeed;
}

sf::FloatRect GAME1_Trap::getTileBounds() const
{
	return sf::FloatRect(
		m_position,
		{ GAME1_TrapTuning::TileSize, GAME1_TrapTuning::TileSize });
}

sf::FloatRect GAME1_Trap::getPlatformBounds() const
{
	return getTileBounds();
}

sf::FloatRect GAME1_Trap::getFireActiveBounds() const
{
	const float tile = GAME1_TrapTuning::TileSize;
	const sf::Vector2f base = m_originalPosition;

	switch (m_orientation)
	{
	case GAME1_TrapOrientation::Right:
		return sf::FloatRect({ base.x, base.y }, { tile * 2.f, tile });

	case GAME1_TrapOrientation::Down:
		return sf::FloatRect({ base.x, base.y }, { tile, tile * 2.f });

	case GAME1_TrapOrientation::Left:
		return sf::FloatRect({ base.x - tile, base.y }, { tile * 2.f, tile });

	case GAME1_TrapOrientation::Up:
	default:
		return sf::FloatRect({ base.x, base.y - tile }, { tile, tile * 2.f });
	}
}

void GAME1_Trap::drawTextureFitted(sf::RenderTarget& target,
	const sf::Texture& texture,
	const sf::FloatRect& bounds,
	GAME1_TrapOrientation orientation) const
{
	sf::Sprite sprite(texture);
	const sf::FloatRect localBounds = sprite.getLocalBounds();

	if (localBounds.size.x <= 0.f || localBounds.size.y <= 0.f)
		return;

	const bool sideways =
		orientation == GAME1_TrapOrientation::Right ||
		orientation == GAME1_TrapOrientation::Left;

	const float orientedWidth = sideways ? localBounds.size.y : localBounds.size.x;
	const float orientedHeight = sideways ? localBounds.size.x : localBounds.size.y;

	const float scale = std::min(
		bounds.size.x / orientedWidth,
		bounds.size.y / orientedHeight);

	sprite.setOrigin({
		localBounds.position.x + localBounds.size.x * 0.5f,
		localBounds.position.y + localBounds.size.y * 0.5f
		});

	sprite.setPosition({
		bounds.position.x + bounds.size.x * 0.5f,
		bounds.position.y + bounds.size.y * 0.5f
		});

	sprite.setRotation(sf::degrees(static_cast<float>(GAME1_GetTrapOrientationValue(orientation)) * 90.f));
	sprite.setScale({ scale, scale });

	target.draw(sprite);
}

void GAME1_Trap::drawFallback(sf::RenderTarget& target,
	const sf::FloatRect& bounds,
	sf::Color color) const
{
	sf::RectangleShape fallback;
	fallback.setPosition(bounds.position);
	fallback.setSize(bounds.size);
	fallback.setFillColor(color);
	fallback.setOutlineColor(sf::Color::White);
	fallback.setOutlineThickness(1.f);
	target.draw(fallback);
}

bool GAME1_Trap::rectsIntersect(const sf::FloatRect& a, const sf::FloatRect& b)
{
	return a.position.x < b.position.x + b.size.x &&
		a.position.x + a.size.x > b.position.x &&
		a.position.y < b.position.y + b.size.y &&
		a.position.y + a.size.y > b.position.y;
}

float GAME1_Trap::moveTowards(float current, float target, float maxDelta)
{
	if (std::abs(target - current) <= maxDelta)
		return target;

	return target > current ? current + maxDelta : current - maxDelta;
}
