#include "GAME1_Saw.h"

#include "GAME1_Level.h"

#include <algorithm>
#include <cassert>
#include <iostream>

namespace
{
	bool TextureIsUsable(const sf::Texture* texture)
	{
		if (texture == nullptr)
			return false;

		const sf::Vector2u size = texture->getSize();
		return size.x > 0 && size.y > 0;
	}

	// Half the overhang of the 2x2 body past the single anchor tile, i.e. the
	// offset that centres the 128 px body on the 64 px anchor tile (32 px).
	static_assert(
		GAME1_Level::TileSize == static_cast<int>(GAME1_SawTuning::TileSize),
		"Saw trap tile size must match GAME1_Level::TileSize.");

	constexpr float TrapTileSize = static_cast<float>(GAME1_Level::TileSize);
	constexpr float TrapBodySize =
		TrapTileSize * static_cast<float>(GAME1_SawTuning::FootprintTiles);
	constexpr float CentreOffset = (TrapBodySize - TrapTileSize) * 0.5f;
}

bool GAME1_SawAssets::load(const std::string& resourcesDirectory)
{
	namespace fs = std::filesystem;

	m_lastWarning.clear();
	m_frames.clear();

	const fs::path sawDirectory =
		fs::path(resourcesDirectory) / "Tiles" / "Traps" / "Saw";

	for (int i = 0; i <= 7; ++i)
	{
		const fs::path framePath = sawDirectory / ("On_" + std::to_string(i) + ".png");

		sf::Texture texture;
		if (texture.loadFromFile(framePath.string()))
			m_frames.push_back(std::move(texture));
		else
			warn("SurfersQuest Saw frame failed to load: " + framePath.string());
	}

	return true;
}

void GAME1_SawAssets::warn(const std::string& message)
{
	m_lastWarning = message;
	std::cerr << message << '\n';
}

const std::vector<sf::Texture>& GAME1_SawAssets::getFrames() const
{
	return m_frames;
}

const sf::Texture* GAME1_SawAssets::getEditorIcon() const
{
	return m_frames.empty() ? nullptr : &m_frames.front();
}

const std::string& GAME1_SawAssets::getLastWarning() const
{
	return m_lastWarning;
}

GAME1_Saw::GAME1_Saw(sf::Vector2i anchorGridPosition)
	: m_anchorGrid(anchorGridPosition)
{
	reset();
}

void GAME1_Saw::setAssets(const GAME1_SawAssets* assets)
{
	m_assets = assets;
}

void GAME1_Saw::reset()
{
	// Centre the 2x2 body on the anchor tile so the chain runs through its
	// middle. Until configureTrack runs the saw has no travel range.
	m_position = {
		static_cast<float>(m_anchorGrid.x) * TrapTileSize - CentreOffset,
		static_cast<float>(m_anchorGrid.y) * TrapTileSize - CentreOffset
	};

	m_minX = m_position.x;
	m_maxX = m_position.x;
	m_minY = m_position.y;
	m_maxY = m_position.y;
	m_trackHorizontal = true;
	m_direction = 1.f;

	m_animationTimer = 0.f;
	m_frameIndex = 0;
}

void GAME1_Saw::configureTrack(sf::Vector2i startGrid,
	sf::Vector2i endGrid,
	bool horizontal,
	int levelWidthTiles,
	int levelHeightTiles)
{
	// If the level has not been sized yet, keep the saw stationary at its anchor
	// instead of clamping its travel range to the origin.
	if (levelWidthTiles <= 0 || levelHeightTiles <= 0)
	{
		m_minX = m_position.x;
		m_maxX = m_position.x;
		m_minY = m_position.y;
		m_maxY = m_position.y;
		return;
	}

	// Keep the 2x2 body inside the level even if a chain runs to the edge.
	const float worldRight = static_cast<float>(levelWidthTiles) * TrapTileSize - TrapBodySize;
	const float worldBottom = static_cast<float>(levelHeightTiles) * TrapTileSize - TrapBodySize;

	m_position.x = clampValue(m_position.x, 0.f, std::max(0.f, worldRight));
	m_position.y = clampValue(m_position.y, 0.f, std::max(0.f, worldBottom));
	m_trackHorizontal = horizontal;

	if (m_trackHorizontal)
	{
		// Body top-left positions whose centre rides each end tile's centre.
		float minX = static_cast<float>(startGrid.x) * TrapTileSize - CentreOffset;
		float maxX = static_cast<float>(endGrid.x) * TrapTileSize - CentreOffset;

		minX = clampValue(minX, 0.f, std::max(0.f, worldRight));
		maxX = clampValue(maxX, 0.f, std::max(0.f, worldRight));

		if (maxX < minX)
			std::swap(minX, maxX);

		m_minX = minX;
		m_maxX = maxX;
		m_minY = m_position.y;
		m_maxY = m_position.y;
		m_position.x = clampValue(m_position.x, m_minX, m_maxX);
		return;
	}

	float minY = static_cast<float>(startGrid.y) * TrapTileSize - CentreOffset;
	float maxY = static_cast<float>(endGrid.y) * TrapTileSize - CentreOffset;

	minY = clampValue(minY, 0.f, std::max(0.f, worldBottom));
	maxY = clampValue(maxY, 0.f, std::max(0.f, worldBottom));

	if (maxY < minY)
		std::swap(minY, maxY);

	m_minX = m_position.x;
	m_maxX = m_position.x;
	m_minY = minY;
	m_maxY = maxY;
	m_position.y = clampValue(m_position.y, m_minY, m_maxY);
}

void GAME1_Saw::update(float deltaTime)
{
	const std::vector<sf::Texture>* frames =
		m_assets != nullptr ? &m_assets->getFrames() : nullptr;

	if (frames != nullptr && frames->size() > 1)
	{
		m_animationTimer += deltaTime;

		while (m_animationTimer >= GAME1_SawTuning::FrameDuration)
		{
			m_animationTimer -= GAME1_SawTuning::FrameDuration;
			m_frameIndex = (m_frameIndex + 1) % frames->size();
		}
	}

	if (m_trackHorizontal && m_maxX > m_minX)
	{
		m_position.x += m_direction * GAME1_SawTuning::MoveSpeed * deltaTime;

		if (m_position.x >= m_maxX)
		{
			m_position.x = m_maxX;
			m_direction = -1.f;
		}
		else if (m_position.x <= m_minX)
		{
			m_position.x = m_minX;
			m_direction = 1.f;
		}
	}
	else if (!m_trackHorizontal && m_maxY > m_minY)
	{
		m_position.y += m_direction * GAME1_SawTuning::MoveSpeed * deltaTime;

		if (m_position.y >= m_maxY)
		{
			m_position.y = m_maxY;
			m_direction = -1.f;
		}
		else if (m_position.y <= m_minY)
		{
			m_position.y = m_minY;
			m_direction = 1.f;
		}
	}
}

void GAME1_Saw::draw(sf::RenderTarget& target) const
{
	const sf::FloatRect box = getBodyBounds();
	const sf::Texture* texture = currentTexture();

	if (TextureIsUsable(texture))
	{
		sf::Sprite sprite(*texture);
		const sf::FloatRect localBounds = sprite.getLocalBounds();

		if (localBounds.size.x > 0.f && localBounds.size.y > 0.f)
		{
			// Uniform fit so the blade keeps its aspect ratio (no uneven
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
	fallback.setFillColor(sf::Color(190, 190, 200));
	fallback.setOutlineColor(sf::Color::White);
	fallback.setOutlineThickness(1.f);
	target.draw(fallback);
}

const sf::Texture* GAME1_Saw::currentTexture() const
{
	if (m_assets == nullptr)
		return nullptr;

	const std::vector<sf::Texture>& frames = m_assets->getFrames();
	if (frames.empty())
		return nullptr;

	return &frames[m_frameIndex % frames.size()];
}

sf::Vector2i GAME1_Saw::getAnchorGridPosition() const
{
	return m_anchorGrid;
}

sf::FloatRect GAME1_Saw::getBodyBounds() const
{
	return sf::FloatRect(
		m_position,
		{ TrapBodySize, TrapBodySize });
}

sf::FloatRect GAME1_Saw::getHazardBounds() const
{
	const float inset = GAME1_SawTuning::HazardInset;
	const sf::FloatRect body = getBodyBounds();

	// Inset both sides, clamped so the rect can never collapse to a negative
	// size even if HazardInset is mis-tuned in the future.
	const float width = std::max(1.f, body.size.x - inset * 2.f);
	const float height = std::max(1.f, body.size.y - inset * 2.f);

	const sf::FloatRect hazard(
		{ body.position.x + inset, body.position.y + inset },
		{ width, height });

	// Debug-only sanity: the hazard must stay a small box around the saw body
	// (never negative, never larger than the 2x2 footprint).
	assert(hazard.size.x > 0.f && hazard.size.x <= TrapBodySize);
	assert(hazard.size.y > 0.f && hazard.size.y <= TrapBodySize);

	return hazard;
}

float GAME1_Saw::clampValue(float value, float low, float high)
{
	return std::max(low, std::min(value, high));
}
