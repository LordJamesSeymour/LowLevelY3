#include "GAME1_Pickup.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iostream>
#include <optional>
#include <utility>

namespace
{
	struct FruitDefinition
	{
		GAME1_FruitType type;
		const char* name;
		char mapCode;
		int points;
		sf::Color fallbackColor;
	};

	const std::vector<FruitDefinition>& FruitDefinitions()
	{
		static const std::vector<FruitDefinition> definitions =
		{
			{ GAME1_FruitType::Cherry, "Cherry", 'c', 50, sf::Color(210, 35, 65) },
			{ GAME1_FruitType::Strawberry, "Strawberry", 's', 100, sf::Color(230, 55, 80) },
			{ GAME1_FruitType::Apple, "Apple", 'a', 150, sf::Color(205, 45, 35) },
			{ GAME1_FruitType::Banana, "Banana", 'b', 200, sf::Color(245, 220, 70) },
			{ GAME1_FruitType::Orange, "Orange", 'o', 250, sf::Color(245, 145, 45) },
			{ GAME1_FruitType::Kiwi, "Kiwi", 'k', 300, sf::Color(115, 190, 70) },
			{ GAME1_FruitType::Melon, "Melon", 'm', 450, sf::Color(90, 210, 130) },
			{ GAME1_FruitType::Pineapple, "Pineapple", 'p', 600, sf::Color(235, 185, 45) }
		};

		return definitions;
	}

	std::size_t FruitIndex(GAME1_FruitType type)
	{
		const std::vector<FruitDefinition>& definitions = FruitDefinitions();

		for (std::size_t i = 0; i < definitions.size(); ++i)
		{
			if (definitions[i].type == type)
				return i;
		}

		return 0;
	}

	const FruitDefinition& GetFruitDefinition(GAME1_FruitType type)
	{
		const std::vector<FruitDefinition>& definitions = FruitDefinitions();
		return definitions[FruitIndex(type)];
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

	std::vector<std::filesystem::path> GetSortedPngFiles(const std::filesystem::path& directory)
	{
		namespace fs = std::filesystem;

		std::vector<fs::path> paths;

		if (!fs::exists(directory) || !fs::is_directory(directory))
			return paths;

		for (const auto& entry : fs::directory_iterator(directory))
		{
			if (entry.is_regular_file() && IsPngFile(entry.path()))
				paths.push_back(entry.path());
		}

		std::sort(paths.begin(), paths.end(), NaturalFrameSort);
		return paths;
	}

	std::vector<std::filesystem::path> GetSortedCollectedFrames(const std::filesystem::path& pickupsDirectory)
	{
		namespace fs = std::filesystem;

		std::vector<fs::path> paths;

		if (!fs::exists(pickupsDirectory) || !fs::is_directory(pickupsDirectory))
			return paths;

		for (const auto& entry : fs::directory_iterator(pickupsDirectory))
		{
			if (!entry.is_regular_file() || !IsPngFile(entry.path()))
				continue;

			const std::string stem = ToLower(entry.path().stem().string());
			if (stem.rfind("collected_", 0) == 0)
				paths.push_back(entry.path());
		}

		std::sort(paths.begin(), paths.end(), NaturalFrameSort);
		return paths;
	}
}

const std::vector<GAME1_FruitType>& GAME1_GetAllFruitTypes()
{
	static const std::vector<GAME1_FruitType> types =
	{
		GAME1_FruitType::Cherry,
		GAME1_FruitType::Strawberry,
		GAME1_FruitType::Apple,
		GAME1_FruitType::Banana,
		GAME1_FruitType::Orange,
		GAME1_FruitType::Kiwi,
		GAME1_FruitType::Melon,
		GAME1_FruitType::Pineapple
	};

	return types;
}

const std::string& GAME1_GetFruitName(GAME1_FruitType type)
{
	static const std::vector<std::string> names =
	{
		"Cherry",
		"Strawberry",
		"Apple",
		"Banana",
		"Orange",
		"Kiwi",
		"Melon",
		"Pineapple"
	};

	return names[FruitIndex(type)];
}

int GAME1_GetFruitPointValue(GAME1_FruitType type)
{
	return GetFruitDefinition(type).points;
}

char GAME1_GetFruitMapCode(GAME1_FruitType type)
{
	return GetFruitDefinition(type).mapCode;
}

bool GAME1_TryGetFruitTypeForMapCode(char code, GAME1_FruitType& outType)
{
	for (const FruitDefinition& definition : FruitDefinitions())
	{
		if (definition.mapCode == code)
		{
			outType = definition.type;
			return true;
		}
	}

	return false;
}

sf::Color GAME1_GetFruitFallbackColor(GAME1_FruitType type)
{
	return GetFruitDefinition(type).fallbackColor;
}

bool GAME1_PickupAssets::load(const std::string& resourcesDirectory)
{
	namespace fs = std::filesystem;

	m_lastWarning.clear();
	m_idleFrames.clear();
	m_idleFrames.resize(FruitDefinitions().size());
	m_collectedFrames.clear();

	if (resourcesDirectory.empty())
		return true;

	const fs::path pickupsDirectory = fs::path(resourcesDirectory) / "Pickups";

	if (!fs::exists(pickupsDirectory) || !fs::is_directory(pickupsDirectory))
	{
		warn("SurfersQuest pickup assets folder missing: " + pickupsDirectory.string());
		return true;
	}

	for (const FruitDefinition& definition : FruitDefinitions())
	{
		std::vector<sf::Texture>& frames = getMutableIdleFrames(definition.type);
		const fs::path fruitDirectory = pickupsDirectory / definition.name;
		const std::vector<fs::path> framePaths = GetSortedPngFiles(fruitDirectory);

		if (framePaths.empty())
		{
			warn("SurfersQuest pickup frames missing for " + std::string(definition.name) + ": " + fruitDirectory.string());
			continue;
		}

		for (const fs::path& framePath : framePaths)
		{
			sf::Texture texture;

			if (texture.loadFromFile(framePath.string()))
			{
				frames.push_back(std::move(texture));
			}
			else
			{
				warn("SurfersQuest pickup frame failed to load: " + framePath.string());
			}
		}
	}

	const std::vector<fs::path> collectedPaths = GetSortedCollectedFrames(pickupsDirectory);

	if (collectedPaths.empty())
	{
		warn("SurfersQuest collected pickup animation missing in: " + pickupsDirectory.string());
	}
	else
	{
		for (const fs::path& framePath : collectedPaths)
		{
			sf::Texture texture;

			if (texture.loadFromFile(framePath.string()))
			{
				m_collectedFrames.push_back(std::move(texture));
			}
			else
			{
				warn("SurfersQuest collected pickup frame failed to load: " + framePath.string());
			}
		}
	}

	return true;
}

const std::vector<sf::Texture>& GAME1_PickupAssets::getIdleFrames(GAME1_FruitType type) const
{
	const std::size_t index = FruitIndex(type);

	if (index >= m_idleFrames.size())
		return m_emptyFrames;

	return m_idleFrames[index];
}

const std::vector<sf::Texture>& GAME1_PickupAssets::getCollectedFrames() const
{
	return m_collectedFrames;
}

const sf::Texture* GAME1_PickupAssets::getIconTexture(GAME1_FruitType type) const
{
	const std::vector<sf::Texture>& frames = getIdleFrames(type);

	if (frames.empty())
		return nullptr;

	return &frames.front();
}

const std::string& GAME1_PickupAssets::getLastWarning() const
{
	return m_lastWarning;
}

std::vector<sf::Texture>& GAME1_PickupAssets::getMutableIdleFrames(GAME1_FruitType type)
{
	const std::size_t index = FruitIndex(type);

	if (index >= m_idleFrames.size())
		m_idleFrames.resize(index + 1);

	return m_idleFrames[index];
}

void GAME1_PickupAssets::warn(const std::string& message)
{
	m_lastWarning = message;
	std::cerr << message << '\n';
}

GAME1_Pickup::GAME1_Pickup(GAME1_FruitType type, sf::Vector2i gridPosition, sf::Vector2f position)
	: m_type(type),
	m_gridPosition(gridPosition),
	m_position(position)
{
}

void GAME1_Pickup::update(float deltaTime, const GAME1_PickupAssets& assets)
{
	if (!m_collected)
	{
		const std::vector<sf::Texture>& idleFrames = assets.getIdleFrames(m_type);

		if (idleFrames.size() <= 1)
			return;

		m_idleFrameTimer += deltaTime;

		while (m_idleFrameTimer >= m_idleFrameDuration)
		{
			m_idleFrameTimer -= m_idleFrameDuration;
			m_idleFrameIndex = (m_idleFrameIndex + 1) % idleFrames.size();
		}

		return;
	}

	const std::vector<sf::Texture>& collectedFrames = assets.getCollectedFrames();

	if (collectedFrames.empty() || m_collectedFrameIndex >= collectedFrames.size())
		return;

	m_collectedFrameTimer += deltaTime;

	while (m_collectedFrameTimer >= m_collectedFrameDuration)
	{
		m_collectedFrameTimer -= m_collectedFrameDuration;

		if (m_collectedFrameIndex + 1 < collectedFrames.size())
		{
			++m_collectedFrameIndex;
		}
		else
		{
			m_collectedFrameIndex = collectedFrames.size();
			break;
		}
	}
}

void GAME1_Pickup::draw(sf::RenderTarget& target, const GAME1_PickupAssets& assets) const
{
	const sf::FloatRect drawBounds(m_position, { DrawSize, DrawSize });

	if (m_collected)
	{
		const std::vector<sf::Texture>& collectedFrames = assets.getCollectedFrames();

		if (m_collectedFrameIndex >= collectedFrames.size())
			return;

		drawTextureFitted(target, collectedFrames[m_collectedFrameIndex], drawBounds);
		return;
	}

	const std::vector<sf::Texture>& idleFrames = assets.getIdleFrames(m_type);

	if (!idleFrames.empty())
	{
		drawTextureFitted(target, idleFrames[m_idleFrameIndex % idleFrames.size()], drawBounds);
		return;
	}

	sf::RectangleShape fallback;
	fallback.setPosition(drawBounds.position);
	fallback.setSize(drawBounds.size);
	fallback.setFillColor(GAME1_GetFruitFallbackColor(m_type));
	fallback.setOutlineColor(sf::Color::White);
	fallback.setOutlineThickness(1.f);
	target.draw(fallback);
}

bool GAME1_Pickup::tryCollect(const sf::FloatRect& playerBounds)
{
	if (!isCollectable())
		return false;

	if (!rectsIntersect(playerBounds, getBounds()))
		return false;

	m_collected = true;
	m_collectedFrameTimer = 0.f;
	m_collectedFrameIndex = 0;
	return true;
}

bool GAME1_Pickup::isCollectable() const
{
	return !m_collected;
}

bool GAME1_Pickup::isFinished(const GAME1_PickupAssets& assets) const
{
	if (!m_collected)
		return false;

	const std::vector<sf::Texture>& collectedFrames = assets.getCollectedFrames();
	return collectedFrames.empty() || m_collectedFrameIndex >= collectedFrames.size();
}

GAME1_FruitType GAME1_Pickup::getType() const
{
	return m_type;
}

int GAME1_Pickup::getPointValue() const
{
	return GAME1_GetFruitPointValue(m_type);
}

sf::Vector2f GAME1_Pickup::getPosition() const
{
	return m_position;
}

sf::Vector2i GAME1_Pickup::getGridPosition() const
{
	return m_gridPosition;
}

sf::FloatRect GAME1_Pickup::getBounds() const
{
	return sf::FloatRect(m_position, { DrawSize, DrawSize });
}

void GAME1_Pickup::drawTextureFitted(sf::RenderTarget& target,
	const sf::Texture& texture,
	sf::FloatRect bounds) const
{
	sf::Sprite sprite(texture);

	const sf::FloatRect localBounds = sprite.getLocalBounds();

	if (localBounds.size.x <= 0.f || localBounds.size.y <= 0.f)
		return;

	sprite.setScale({
		bounds.size.x / localBounds.size.x,
		bounds.size.y / localBounds.size.y
		});

	sprite.setPosition(bounds.position);
	target.draw(sprite);
}

bool GAME1_Pickup::rectsIntersect(const sf::FloatRect& a, const sf::FloatRect& b)
{
	return a.position.x < b.position.x + b.size.x &&
		a.position.x + a.size.x > b.position.x &&
		a.position.y < b.position.y + b.size.y &&
		a.position.y + a.size.y > b.position.y;
}
