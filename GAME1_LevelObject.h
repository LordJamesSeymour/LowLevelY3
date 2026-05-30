#pragma once

#include <SFML/System/Vector2.hpp>

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <vector>

enum class GAME1_LevelObjectType
{
	StartTile,
	EndTile
};

struct GAME1_LevelObjectSpawn
{
	GAME1_LevelObjectType type = GAME1_LevelObjectType::StartTile;
	sf::Vector2i gridPosition{ 0, 0 };
};

inline std::string GAME1_LevelObjectToLower(std::string value)
{
	std::transform(value.begin(), value.end(), value.begin(),
		[](unsigned char c)
		{
			return static_cast<char>(std::tolower(c));
		});

	return value;
}

inline const std::vector<GAME1_LevelObjectType>& GAME1_GetAllLevelObjectTypes()
{
	static const std::vector<GAME1_LevelObjectType> types =
	{
		GAME1_LevelObjectType::StartTile,
		GAME1_LevelObjectType::EndTile
	};

	return types;
}

inline std::string GAME1_GetLevelObjectTypeName(GAME1_LevelObjectType type)
{
	switch (type)
	{
	case GAME1_LevelObjectType::EndTile:
		return "EndTile";

	case GAME1_LevelObjectType::StartTile:
	default:
		return "StartTile";
	}
}

inline std::string GAME1_GetLevelObjectEditorLabel(GAME1_LevelObjectType type)
{
	switch (type)
	{
	case GAME1_LevelObjectType::EndTile:
		return "End";

	case GAME1_LevelObjectType::StartTile:
	default:
		return "Start";
	}
}

inline bool GAME1_TryParseLevelObjectType(const std::string& value,
	GAME1_LevelObjectType& outType)
{
	const std::string lowered = GAME1_LevelObjectToLower(value);

	if (lowered == "starttile" || lowered == "start")
	{
		outType = GAME1_LevelObjectType::StartTile;
		return true;
	}

	if (lowered == "endtile" || lowered == "end" || lowered == "goaltile" || lowered == "goal")
	{
		outType = GAME1_LevelObjectType::EndTile;
		return true;
	}

	return false;
}

inline bool GAME1_TryParseLevelObjectLine(const std::string& line,
	GAME1_LevelObjectSpawn& outSpawn)
{
	std::istringstream stream(line);

	std::string marker;
	std::string typeName;
	int col = 0;
	int row = 0;

	if (!(stream >> marker >> typeName >> col >> row))
		return false;

	if (marker != "OBJECT")
		return false;

	GAME1_LevelObjectType type = GAME1_LevelObjectType::StartTile;
	if (!GAME1_TryParseLevelObjectType(typeName, type))
		return false;

	outSpawn.type = type;
	outSpawn.gridPosition = { col, row };
	return true;
}

inline std::string GAME1_BuildLevelObjectLine(const GAME1_LevelObjectSpawn& spawn)
{
	std::ostringstream stream;
	stream << "OBJECT "
		<< GAME1_GetLevelObjectTypeName(spawn.type) << ' '
		<< spawn.gridPosition.x << ' '
		<< spawn.gridPosition.y << ' '
		<< 0;

	return stream.str();
}
