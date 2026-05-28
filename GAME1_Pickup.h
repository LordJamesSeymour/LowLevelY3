#pragma once

#include <SFML/Graphics.hpp>

#include <cstddef>
#include <string>
#include <vector>

enum class GAME1_FruitType
{
	Cherry,
	Strawberry,
	Apple,
	Banana,
	Orange,
	Kiwi,
	Melon,
	Pineapple
};

struct GAME1_PickupSpawn
{
	GAME1_FruitType type = GAME1_FruitType::Cherry;
	sf::Vector2i gridPosition{ 0, 0 };
	sf::Vector2f position{ 0.f, 0.f };
};

const std::vector<GAME1_FruitType>& GAME1_GetAllFruitTypes();
const std::string& GAME1_GetFruitName(GAME1_FruitType type);
int GAME1_GetFruitPointValue(GAME1_FruitType type);
char GAME1_GetFruitMapCode(GAME1_FruitType type);
bool GAME1_TryGetFruitTypeForMapCode(char code, GAME1_FruitType& outType);
sf::Color GAME1_GetFruitFallbackColor(GAME1_FruitType type);

class GAME1_PickupAssets
{
public:
	bool load(const std::string& resourcesDirectory);

	const std::vector<sf::Texture>& getIdleFrames(GAME1_FruitType type) const;
	const std::vector<sf::Texture>& getCollectedFrames() const;
	const sf::Texture* getIconTexture(GAME1_FruitType type) const;
	const std::string& getLastWarning() const;

private:
	std::vector<sf::Texture>& getMutableIdleFrames(GAME1_FruitType type);
	void warn(const std::string& message);

private:
	std::vector<std::vector<sf::Texture>> m_idleFrames;
	std::vector<sf::Texture> m_collectedFrames;
	std::vector<sf::Texture> m_emptyFrames;
	std::string m_lastWarning;
};

class GAME1_Pickup
{
public:
	static constexpr float DrawSize = 64.f;

	GAME1_Pickup() = default;
	GAME1_Pickup(GAME1_FruitType type, sf::Vector2i gridPosition, sf::Vector2f position);

	void update(float deltaTime, const GAME1_PickupAssets& assets);
	void draw(sf::RenderTarget& target, const GAME1_PickupAssets& assets) const;

	bool tryCollect(const sf::FloatRect& playerBounds);
	bool isCollectable() const;
	bool isFinished(const GAME1_PickupAssets& assets) const;

	GAME1_FruitType getType() const;
	int getPointValue() const;
	sf::Vector2f getPosition() const;
	sf::Vector2i getGridPosition() const;
	sf::FloatRect getBounds() const;

private:
	void drawTextureFitted(sf::RenderTarget& target,
		const sf::Texture& texture,
		sf::FloatRect bounds) const;

	static bool rectsIntersect(const sf::FloatRect& a, const sf::FloatRect& b);

private:
	GAME1_FruitType m_type = GAME1_FruitType::Cherry;
	sf::Vector2i m_gridPosition{ 0, 0 };
	sf::Vector2f m_position{ 0.f, 0.f };

	bool m_collected = false;

	float m_idleFrameTimer = 0.f;
	float m_collectedFrameTimer = 0.f;
	float m_idleFrameDuration = 0.08f;
	float m_collectedFrameDuration = 0.06f;

	std::size_t m_idleFrameIndex = 0;
	std::size_t m_collectedFrameIndex = 0;
};
