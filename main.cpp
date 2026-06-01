#include <SFML/Graphics.hpp>
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include "ArcadeHub.h"
#include "ArcadeHubOptions.h"
#include "ArcadeInput.h"
#include "ArcadeSettings.h"
#include "ArcadeUISounds.h"
#include "GamePauseMenu.h"
#include "BombermanAudio.h"

#include "GAME1_BombermanWindow.h"
#include "GAME1_BombermanMenu.h"
#include "GAME1_BombermanLevelSelect.h"
#include "GAME1_BombermanLevelEditor.h"

#include "GAME1_Level.h"
#include "GAME1_Player.h"
#include "GAME1_Menu.h"
#include "GAME1_LevelEditor.h"
#include "GAME1_LevelSelect.h"
#include "GAME1_Enemy.h"
#include "GAME1_Pickup.h"
#include "GAME1_SurfersQuestAudio.h"

#include "GAME2_Menu.h"
#include "GAME2_Game.h"

namespace
{
	const std::filesystem::path kGlobalFontPath = "assets/menu.ttf";
	const std::filesystem::path kHubShaderPath = "assets/Shaders/ArcadeHubCRT.frag";
	const std::filesystem::path kLockedImagePath = "assets/LockedImage.png";
	const std::filesystem::path kSettingsIconPath = "assets/settings.png";

	const std::filesystem::path kBombermanRootDirectory = "assets/Game#0/Bomberman";
	const std::filesystem::path kBombermanMapsDirectory = "assets/Game#0/Bomberman/Maps";
	const std::filesystem::path kBombermanSplashStillImagePath = "assets/Game#0/SplashScreen/BombermanSplashScreen.png";
	const std::filesystem::path kBombermanSplashFramesDirectory = "assets/Game#0/SplashScreen/GIFs";

	const std::filesystem::path kGame1RootDirectory = "assets/Game#1/SurfersQuest";
	const std::filesystem::path kGame1ResourcesDirectory = kGame1RootDirectory / "Resources";
	const std::filesystem::path kGame1MapsDirectory = kGame1RootDirectory / "Maps";
	const std::filesystem::path kGame1SplashStillImagePath = "assets/Game#1/SplashScreen/SidescrollerSplashScreen.png";
	const std::filesystem::path kGame1SplashFramesDirectory = "assets/Game#1/SplashScreen/GIFs";

	// Surfers Quest local co-op tuning.
	constexpr float kGame1CoopJoinSpawnOffsetX = 60.f;
	constexpr float kGame1CoopOffScreenKillDistance = 220.f;
	constexpr float kGame1CoopRespawnOffsetX = 50.f;

	const std::filesystem::path kGame2ResourcesDirectory = "assets/Game#2/Resources";
	const std::filesystem::path kGame2SplashStillImagePath = "assets/Game#2/SplashScreen/Game2SplashScreen.png";

	struct GAME1_PlayerScoreState
	{
		int score = 0;
		std::vector<GAME1_FruitType> collectedFruits;
		// Per-player completion time (multiplayer). Recorded when this player
		// reaches the End Tile, or set to the level end time if the multiplayer
		// countdown expires before they get there.
		float completionTimeSeconds = 0.f;
		bool reachedEnd = false;
		bool forceStatsNA = false;

		void reset()
		{
			score = 0;
			collectedFruits.clear();
			completionTimeSeconds = 0.f;
			reachedEnd = false;
			forceStatsNA = false;
		}
	};

	struct GAME1_FloatingScorePopup
	{
		sf::Vector2f position{ 0.f, 0.f };
		int points = 0;
		float age = 0.f;
		float lifetime = 0.75f;
	};

	std::string FormatGame1RunTime(float seconds)
	{
		const int totalSeconds = std::max(0, static_cast<int>(std::floor(seconds)));
		const int minutes = totalSeconds / 60;
		const int remainingSeconds = totalSeconds % 60;

		std::ostringstream stream;
		stream << std::setw(2) << std::setfill('0') << minutes
			<< ':'
			<< std::setw(2) << std::setfill('0') << remainingSeconds;

		return stream.str();
	}

	sf::Color GetGame1RainbowColor(float timeSeconds)
	{
		auto channel = [timeSeconds](float phase)
			{
				const float value = 0.5f + 0.5f * std::sin(timeSeconds * 3.2f + phase);
				return static_cast<std::uint8_t>(80 + static_cast<int>(value * 175.f));
			};

		return sf::Color(
			channel(0.f),
			channel(2.0943951f),
			channel(4.1887902f));
	}

	bool RectsOverlap(const sf::FloatRect& a, const sf::FloatRect& b)
	{
		return a.position.x < b.position.x + b.size.x &&
			a.position.x + a.size.x > b.position.x &&
			a.position.y < b.position.y + b.size.y &&
			a.position.y + a.size.y > b.position.y;
	}

	bool PointInRect(const sf::FloatRect& rect, sf::Vector2f point)
	{
		return point.x >= rect.position.x &&
			point.x <= rect.position.x + rect.size.x &&
			point.y >= rect.position.y &&
			point.y <= rect.position.y + rect.size.y;
	}

	enum class AppState
	{
		Hub,

		GAME1_BombermanMenu,
		GAME1_BombermanLevelSelect,
		GAME1_BombermanEditor,
		GAME1_Bomberman,

		GAME1_Menu,
		GAME1_LevelSelect,
		GAME1_Game,
		GAME1_Editor,

		GAME2_Menu,
		GAME2_Game
	};

	enum class UserMessageType
	{
		Error,
		Info
	};

	void ShowUserMessage(const std::string& title, const std::string& message, UserMessageType type)
	{
#if defined(_WIN32)
		const UINT icon = type == UserMessageType::Error ? MB_ICONERROR : MB_ICONINFORMATION;
		MessageBoxA(nullptr, message.c_str(), title.c_str(), MB_OK | icon);
#else
		std::ostream& stream = type == UserMessageType::Error ? std::cerr : std::cout;
		stream << title << ":\n" << message << std::endl;
#endif
	}

	void ShowError(const std::string& message)
	{
		ShowUserMessage("Project Error", message, UserMessageType::Error);
	}

	void ShowInfo(const std::string& message)
	{
		ShowUserMessage("Info", message, UserMessageType::Info);
	}

	void ApplyWindowView(sf::RenderWindow& window)
	{
		const sf::Vector2u size = window.getSize();

		if (size.x == 0 || size.y == 0)
			return;

		const sf::FloatRect visibleArea(
			{ 0.f, 0.f },
			{ static_cast<float>(size.x), static_cast<float>(size.y) }
		);

		window.setView(sf::View(visibleArea));
	}

	// Windowed fallback resolution. Matches the Raspberry Pi / Elecrow panel
	// (1024x600) and is a safe size on desktop too.
	const sf::Vector2u kWindowedResolution(1024u, 600u);

	// Creates (or recreates) the main window in the mode currently stored in
	// ArcadeSettings, then reapplies the framerate limit, 1:1 view and cursor
	// visibility. Used both at startup and whenever the fullscreen setting is
	// toggled at runtime.
	void ApplyWindowMode(sf::RenderWindow& window)
	{
		if (ArcadeSettings::isFullscreenEnabled())
		{
			window.create(sf::VideoMode::getDesktopMode(), "Arcade Collection", sf::State::Fullscreen);
		}
		else
		{
			window.create(sf::VideoMode(kWindowedResolution), "Arcade Collection", sf::State::Windowed);
		}

		window.setFramerateLimit(60);
		ApplyWindowView(window);
		window.setMouseCursorVisible(!ArcadeSettings::isFullscreenEnabled());
	}

	bool ResizeTexture(sf::Texture& texture, sf::Vector2u newSize)
	{
		if (newSize.x == 0 || newSize.y == 0)
			return false;

		if (texture.getSize() == newSize)
			return true;

		return texture.resize(newSize);
	}

	std::string TryFindSolutionInFolder(const std::filesystem::path& folder)
	{
		namespace fs = std::filesystem;

		if (!fs::exists(folder) || !fs::is_directory(folder))
			return {};

		for (const auto& entry : fs::directory_iterator(folder))
		{
			if (entry.is_regular_file() && entry.path().extension() == ".sln")
			{
				return entry.path().stem().string();
			}
		}

		return {};
	}

	std::string GetProjectNameFromSolution()
	{
		namespace fs = std::filesystem;

		fs::path folder = fs::current_path();

		for (int depth = 0; depth < 6; ++depth)
		{
			const std::string found = TryFindSolutionInFolder(folder);
			if (!found.empty())
				return found;

			if (!folder.has_parent_path())
				break;

			folder = folder.parent_path();
		}

		return "Project Name";
	}

	bool IsValidLevelFile(const std::filesystem::path& path)
	{
		if (!path.has_filename() || path.extension() != ".txt")
			return false;

		const std::string stem = path.stem().string();

		if (stem.rfind("level", 0) != 0)
			return false;

		if (stem.size() <= 5)
			return false;

		for (std::size_t i = 5; i < stem.size(); ++i)
		{
			if (!std::isdigit(static_cast<unsigned char>(stem[i])))
				return false;
		}

		return true;
	}

	int ExtractLevelNumber(const std::filesystem::path& path)
	{
		const std::string stem = path.stem().string();
		return std::stoi(stem.substr(5));
	}

	std::vector<std::string> GetAllGame1LevelPaths()
	{
		namespace fs = std::filesystem;

		std::vector<fs::path> paths;

		if (!fs::exists(kGame1MapsDirectory))
			return {};

		for (const auto& entry : fs::directory_iterator(kGame1MapsDirectory))
		{
			if (entry.is_regular_file() && IsValidLevelFile(entry.path()))
			{
				paths.push_back(entry.path());
			}
		}

		std::sort(paths.begin(), paths.end(),
			[](const fs::path& a, const fs::path& b)
			{
				return ExtractLevelNumber(a) < ExtractLevelNumber(b);
			});

		std::vector<std::string> result;
		result.reserve(paths.size());

		for (const fs::path& path : paths)
		{
			result.push_back(path.string());
		}

		return result;
	}

	bool LoadGame1(GAME1_Level& level,
		GAME1_Player& player,
		std::vector<GAME1_Enemy>& enemies,
		std::vector<GAME1_Pickup>& pickups,
		const std::string& mapPath)
	{
		const std::string playerIdleDirectory =
			(kGame1ResourcesDirectory / "Player" / "PlayerIdle").string();

		enemies.clear();
		pickups.clear();

		if (!level.loadFromFile(mapPath, kGame1ResourcesDirectory.string()))
		{
			std::string msg =
				"Game 1 level failed to load.\n\n" +
				level.getLastError() +
				"\n\nCurrent working directory:\n" +
				std::filesystem::current_path().string();

			ShowError(msg);
			return false;
		}

		if (!player.load(playerIdleDirectory, level.getPlayerSpawnPosition()))
		{
			std::string msg =
				"Game 1 player failed to load.\n\n" +
				player.getLastError() +
				"\n\nCurrent working directory:\n" +
				std::filesystem::current_path().string();

			ShowError(msg);
			return false;
		}

		for (const GAME1_LevelEnemySpawn& spawn : level.getEnemySpawns())
		{
			GAME1_Enemy enemy;

			if (!enemy.load((kGame1ResourcesDirectory / "Enemies").string(), GAME1_EnemyType::Basic, spawn.position))
			{
				std::string msg =
					"Game 1 enemy failed to load.\n\n" +
					enemy.getLastError() +
					"\n\nCurrent working directory:\n" +
					std::filesystem::current_path().string();

				ShowError(msg);
				return false;
			}

			enemies.push_back(std::move(enemy));
		}

		for (const GAME1_PickupSpawn& spawn : level.getPickupSpawns())
		{
			pickups.emplace_back(spawn.type, spawn.gridPosition, spawn.position);
		}

		return true;
	}
}

int main()
{
	ArcadeSettings::initialize();

	sf::RenderWindow window;
	ApplyWindowMode(window);

	sf::Texture crtFrameTexture;
	if (!crtFrameTexture.resize(window.getSize()))
	{
		ShowError("Failed to create the CRT frame texture.");
		return -1;
	}

	crtFrameTexture.setSmooth(false);

	sf::Shader crtShader;
	bool crtShaderLoaded = false;

	if (sf::Shader::isAvailable())
	{
		if (crtShader.loadFromFile(kHubShaderPath.string(), sf::Shader::Type::Fragment))
		{
			crtShaderLoaded = true;
		}
	}

	float totalAppTime = 0.f;

	ArcadeHub hub;

	std::vector<ArcadeHubGameEntry> hubGames;
	hubGames.push_back({
		"GAME #1",
		"Bomberman",
		kBombermanSplashFramesDirectory.string(),
		kBombermanSplashStillImagePath.string(),
		false
		});

	hubGames.push_back({
		"GAME #2",
		"Surfers Quest",
		kGame1SplashFramesDirectory.string(),
		kGame1SplashStillImagePath.string(),
		false
		});

	hubGames.push_back({
		"GAME #3",
		"Space Shooter",
		"",
		kGame2SplashStillImagePath.string(),
		true
		});

	if (!hub.load(
		kGlobalFontPath.string(),
		GetProjectNameFromSolution(),
		hubGames,
		kLockedImagePath.string()))
	{
		std::string msg =
			"Arcade hub failed to load.\n\n" +
			hub.getLastError() +
			"\n\nCurrent working directory:\n" +
			std::filesystem::current_path().string();

		ShowError(msg);
		return -1;
	}

	ArcadeHubOptions hubOptions;
	if (!hubOptions.load(kGlobalFontPath.string(), kSettingsIconPath.string()))
	{
		std::string msg =
			"Hub options failed to load.\n\n" +
			hubOptions.getLastError() +
			"\n\nCurrent working directory:\n" +
			std::filesystem::current_path().string();

		ShowError(msg);
		return -1;
	}

	GamePauseMenu pauseMenu;
	if (!pauseMenu.load(kGlobalFontPath.string(), kSettingsIconPath.string()))
	{
		std::string msg =
			"Pause menu failed to load.\n\n" +
			pauseMenu.getLastError() +
			"\n\nCurrent working directory:\n" +
			std::filesystem::current_path().string();

		ShowError(msg);
		return -1;
	}

	GAME1_BombermanMenu bombermanMenu;
	if (!bombermanMenu.load(kGlobalFontPath.string(), kBombermanRootDirectory.string()))
	{
		std::string msg =
			"Bomberman menu failed to load.\n\n" +
			bombermanMenu.getLastError() +
			"\n\nCurrent working directory:\n" +
			std::filesystem::current_path().string();

		ShowError(msg);
		return -1;
	}

	GAME1_BombermanLevelSelect bombermanLevelSelect;
	if (!bombermanLevelSelect.load(kGlobalFontPath.string(), kBombermanMapsDirectory.string()))
	{
		std::string msg =
			"Bomberman level select failed to load.\n\n" +
			bombermanLevelSelect.getLastError() +
			"\n\nCurrent working directory:\n" +
			std::filesystem::current_path().string();

		ShowError(msg);
		return -1;
	}

	GAME1_BombermanLevelEditor bombermanEditor;
	if (!bombermanEditor.load(kGlobalFontPath.string(), kBombermanRootDirectory.string()))
	{
		std::string msg =
			"Bomberman level editor failed to load.\n\n" +
			bombermanEditor.getLastError() +
			"\n\nCurrent working directory:\n" +
			std::filesystem::current_path().string();

		ShowError(msg);
		return -1;
	}

	GAME1_BombermanWindow bombermanWindow;
	if (!bombermanWindow.load(kGlobalFontPath.string(), kBombermanRootDirectory.string()))
	{
		std::string msg =
			"Bomberman failed to load.\n\n" +
			bombermanWindow.getLastError() +
			"\n\nCurrent working directory:\n" +
			std::filesystem::current_path().string();

		ShowError(msg);
		return -1;
	}

	sf::Font game1UiFont;
	if (!game1UiFont.openFromFile(kGlobalFontPath.string()))
	{
		std::string msg =
			"Game 1 UI font failed to load.\n\n" +
			kGlobalFontPath.string() +
			"\n\nCurrent working directory:\n" +
			std::filesystem::current_path().string();

		ShowError(msg);
		return -1;
	}

	sf::Text respawnText(game1UiFont);
	respawnText.setCharacterSize(34);
	respawnText.setFillColor(sf::Color::White);
	respawnText.setOutlineColor(sf::Color::Black);
	respawnText.setOutlineThickness(2.f);

	GAME1_Menu game1Menu;
	if (!game1Menu.load((kGame1ResourcesDirectory / "xbutton.png").string(),
		(kGame1ResourcesDirectory / "logo.png").string(),
		(kGame1ResourcesDirectory / "play.png").string(),
		(kGame1ResourcesDirectory / "leveleditor.png").string()))
	{
		std::string msg =
			"Game 1 menu failed to load.\n\n" +
			game1Menu.getLastError() +
			"\n\nCurrent working directory:\n" +
			std::filesystem::current_path().string();

		ShowError(msg);
		return -1;
	}

	GAME1_LevelSelect game1LevelSelect;
	if (!game1LevelSelect.load(kGlobalFontPath.string()))
	{
		std::string msg =
			"Game 1 level select failed to load.\n\n" +
			game1LevelSelect.getLastError() +
			"\n\nCurrent working directory:\n" +
			std::filesystem::current_path().string();

		ShowError(msg);
		return -1;
	}

	GAME1_LevelEditor game1Editor;
	if (!game1Editor.load(kGlobalFontPath.string(), kGame1RootDirectory.string()))
	{
		std::string msg =
			"Game 1 level editor failed to load.\n\n" +
			game1Editor.getLastError() +
			"\n\nCurrent working directory:\n" +
			std::filesystem::current_path().string();

		ShowError(msg);
		return -1;
	}

	GAME1_Level game1Level;
	GAME1_Player game1Player;
	GAME1_Player game1Player2;
	std::vector<GAME1_Enemy> game1Enemies;
	GAME1_PickupAssets game1PickupAssets;
	game1PickupAssets.load(kGame1ResourcesDirectory.string());
	std::vector<GAME1_Pickup> game1Pickups;
	std::array<GAME1_PlayerScoreState, 2> game1Scores;
	std::vector<GAME1_FloatingScorePopup> game1ScorePopups;
	std::string game1CurrentLevelPath;
	int game1RunScore = 0;
	std::vector<GAME1_FruitType> game1RunCollectedFruits;
	float game1RunTimerSeconds = 0.f;
	float game1FinalRunTimerSeconds = 0.f;
	bool game1RunTimerStarted = false;
	bool game1RunFinished = false;
	bool game1VictoryPopupOpen = false;
	int game1TriggeredEndTileIndex = -1;
	int game1VictorySelectedButton = 0;
	sf::FloatRect game1VictoryRetryBounds;
	sf::FloatRect game1VictoryNextBounds;

	// Surfers Quest multiplayer end-tile countdown. Only used in co-op:
	// when the first player reaches the End Tile, the other player gets a
	// fixed window to reach it too before the level finishes.
	const float kGame1EndCountdownDuration = 10.f;
	bool game1EndCountdownActive = false;
	float game1EndCountdownSeconds = 0.f;
	int game1EndCountdownFirstFinisher = -1;

	// Vertical camera state for Surfers Quest: persistent across frames,
	// snapped on level load, smoothly follows player with margin-pinning.
	float game1CameraCenterY = 0.f;
	bool game1CameraNeedsSnap = true;

	int game1LastCheckpointOrder = -1;

	// Surfers Quest local co-op state.
	bool game1Player2Joined = false;
	bool game1TeamGameOver = false;
	// Tracks which device P1 first used (keyboard or a specific joystick).
	// Remains singlePlayer=true until P1 makes their first movement/action input.
	// Used to assign P1's binding correctly when P2 joins.
	GAME1_PlayerBinding game1Player1DetectedSource{};

	GAME2_Menu game2Menu;
	if (!game2Menu.load(
		(kGame2ResourcesDirectory / "logo.png").string(),
		(kGame2ResourcesDirectory / "play.png").string()))
	{
		std::string msg =
			"Game 2 menu failed to load.\n\n" +
			game2Menu.getLastError() +
			"\n\nCurrent working directory:\n" +
			std::filesystem::current_path().string();

		ShowError(msg);
		return -1;
	}

	GAME2_Game game2Game;
	if (!game2Game.load(kGame2ResourcesDirectory.string(), window.getSize()))
	{
		std::string msg =
			"Game 2 failed to load.\n\n" +
			game2Game.getLastError() +
			"\n\nCurrent working directory:\n" +
			std::filesystem::current_path().string();

		ShowError(msg);
		return -1;
	}

	BombermanAudio::initialise(kBombermanRootDirectory.string());
	GAME1_SurfersQuestAudio::initialise(kGame1ResourcesDirectory.string());
	ArcadeUISounds::loadUIClick();

	ArcadeSettings::registerAudioRefreshCallback(
		[&bombermanMenu, &game1Menu, &bombermanWindow]()
		{
			bombermanMenu.refreshAudioVolumes();
			game1Menu.refreshAudioVolumes();
			bombermanWindow.refreshAudioVolumes();
			GAME1_SurfersQuestAudio::refreshVolumes();
			BombermanAudio::refreshVolumes();
		});

	// Recreate the window immediately when the fullscreen setting is toggled,
	// then resize the CRT frame texture to match the new framebuffer.
	ArcadeSettings::registerWindowApplyCallback(
		[&window, &crtFrameTexture, &crtShaderLoaded]()
		{
			ApplyWindowMode(window);

			if (!ResizeTexture(crtFrameTexture, window.getSize()))
			{
				crtShaderLoaded = false;
			}
			else
			{
				crtFrameTexture.setSmooth(false);
			}
		});

	AppState appState = AppState::Hub;

	auto IsSurfersQuestState = [](AppState state)
		{
			return state == AppState::GAME1_Menu ||
				state == AppState::GAME1_LevelSelect ||
				state == AppState::GAME1_Game ||
				state == AppState::GAME1_Editor;
		};

	auto IsBombermanState = [](AppState state)
		{
			return state == AppState::GAME1_BombermanMenu ||
				state == AppState::GAME1_BombermanLevelSelect ||
				state == AppState::GAME1_BombermanEditor ||
				state == AppState::GAME1_Bomberman;
		};

	auto SetAppState = [&](AppState newState)
		{
			const AppState previousState = appState;

			if (pauseMenu.isOpen())
			{
				pauseMenu.close();
			}
			hubOptions.setGearVisible(true);

			if (previousState == AppState::GAME1_BombermanMenu &&
				newState != AppState::GAME1_BombermanMenu)
			{
				bombermanMenu.stopMusic();
			}

			if (IsSurfersQuestState(previousState) &&
				!IsSurfersQuestState(newState))
			{
				game1Menu.stopMusic();
				GAME1_SurfersQuestAudio::stopAll();
			}

			if (IsBombermanState(previousState) && !IsBombermanState(newState))
			{
				BombermanAudio::stopAllMusic();
			}

			appState = newState;
			if (appState != AppState::GAME1_Game)
			{
				game1VictoryPopupOpen = false;
				game1RunFinished = false;
				game1TriggeredEndTileIndex = -1;
			}

			window.setMouseCursorVisible(
				appState != AppState::GAME1_Bomberman &&
				appState != AppState::GAME1_Game);

			if (appState == AppState::Hub)
			{
				hub.notifyUserActivity();
			}

			if (appState == AppState::GAME1_BombermanMenu)
			{
				bombermanMenu.resetSelection();
				bombermanMenu.startMusic();
				BombermanAudio::playMenuMusic();
			}
			else if (appState == AppState::GAME1_Bomberman)
			{
				BombermanAudio::playGameplayMusic();
			}
			else if (appState == AppState::GAME1_BombermanLevelSelect)
			{
				BombermanAudio::playMenuMusic();
			}
			else if (appState == AppState::GAME1_BombermanEditor)
			{
				BombermanAudio::stopAllMusic();
			}

			if (appState == AppState::GAME1_Menu)
			{
				game1Menu.resetSelection();
				game1Menu.startMusic();
				GAME1_SurfersQuestAudio::playMenu();
			}

			if (appState == AppState::GAME1_LevelSelect)
			{
				game1Menu.startMusic();
				GAME1_SurfersQuestAudio::playMenu();
			}

			if (appState == AppState::GAME1_Editor)
			{
				game1Menu.stopMusic();
				GAME1_SurfersQuestAudio::stopAll();
			}

			if (appState == AppState::GAME1_Game)
			{
				game1CameraNeedsSnap = true;
				game1LastCheckpointOrder = -1;

				game1Player2Joined = false;
				game1TeamGameOver = false;

				game1Player.setBinding(GAME1_PlayerBinding{});
				game1Player.setCoopMode(false);
				game1Player.setDrawHud(true);
				game1Player.clearLevelFinishState();
				game1Player2.clearLevelFinishState();
				game1Player1DetectedSource = GAME1_PlayerBinding{};
			}
		};

	auto ResetGame1Scores = [&]()
		{
			for (GAME1_PlayerScoreState& scoreState : game1Scores)
			{
				scoreState.reset();
			}

			game1RunScore = 0;
			game1RunCollectedFruits.clear();
			game1ScorePopups.clear();
		};

	auto ResetGame1RunState = [&]()
		{
			game1RunTimerSeconds = 0.f;
			game1FinalRunTimerSeconds = 0.f;
			game1RunTimerStarted = false;
			game1RunFinished = false;
			game1VictoryPopupOpen = false;
			game1TriggeredEndTileIndex = -1;
			game1VictorySelectedButton = 0;
			game1EndCountdownActive = false;
			game1EndCountdownSeconds = 0.f;
			game1EndCountdownFirstFinisher = -1;
			for (GAME1_PlayerScoreState& scoreState : game1Scores)
			{
				scoreState.completionTimeSeconds = 0.f;
				scoreState.reachedEnd = false;
				scoreState.forceStatsNA = false;
			}
			game1Player.clearLevelFinishState();
			if (game1Player2Joined)
				game1Player2.clearLevelFinishState();
			game1Level.resetGoalTiles();
		};

	auto ResetGame1PickupsFromLevel = [&]()
		{
			game1Pickups.clear();

			for (const GAME1_PickupSpawn& spawn : game1Level.getPickupSpawns())
			{
				game1Pickups.emplace_back(spawn.type, spawn.gridPosition, spawn.position);
			}
		};

	auto ResetGame1CollectiblesAndScores = [&]()
		{
			ResetGame1Scores();
			ResetGame1PickupsFromLevel();
		};

	auto ResetGame1CheckpointProgress = [&]()
		{
			game1LastCheckpointOrder = -1;
			game1Level.resetCheckpoints();

			const sf::Vector2f startPosition = game1Level.getPlayerSpawnPosition();
			game1Player.setSpawnPosition(startPosition);
			if (game1Player2Joined)
				game1Player2.setSpawnPosition(startPosition);
		};

	auto ResetGame1FullAttemptState = [&]()
		{
			game1TeamGameOver = false;
			game1CameraNeedsSnap = true;
			ResetGame1CheckpointProgress();
			ResetGame1CollectiblesAndScores();
			game1Level.resetTraps();
			ResetGame1RunState();
		};

	auto AddGame1ScorePopup = [&](sf::Vector2f position, int points)
		{
			GAME1_FloatingScorePopup popup;
			popup.position = position;
			popup.points = points;
			game1ScorePopups.push_back(popup);
		};

	auto AwardGame1Points = [&](int playerIndex,
		int points,
		sf::Vector2f worldPosition,
		std::optional<GAME1_FruitType> collectedFruit)
		{
			if (playerIndex < 0 || playerIndex >= static_cast<int>(game1Scores.size()))
				return;

			GAME1_PlayerScoreState& scoreState =
				game1Scores[static_cast<std::size_t>(playerIndex)];

			scoreState.score += points;
			game1RunScore += points;

			if (collectedFruit.has_value())
			{
				scoreState.collectedFruits.push_back(collectedFruit.value());
				game1RunCollectedFruits.push_back(collectedFruit.value());
			}

			AddGame1ScorePopup(worldPosition, points);
		};

	auto TryLoadGame1Level = [&](const std::string& mapPath) -> bool
		{
			if (!LoadGame1(game1Level, game1Player, game1Enemies, game1Pickups, mapPath))
				return false;

			game1CurrentLevelPath = mapPath;
			ResetGame1FullAttemptState();
			return true;
		};

	auto GetGame1CurrentLevelIndex = [&]() -> int
		{
			const std::vector<std::string> levelPaths = GetAllGame1LevelPaths();

			for (int i = 0; i < static_cast<int>(levelPaths.size()); ++i)
			{
				if (levelPaths[static_cast<std::size_t>(i)] == game1CurrentLevelPath)
					return i;
			}

			return -1;
		};

	auto Game1HasNextLevel = [&]() -> bool
		{
			const std::vector<std::string> levelPaths = GetAllGame1LevelPaths();
			const int currentIndex = GetGame1CurrentLevelIndex();
			return !levelPaths.empty() && currentIndex >= 0;
		};

	auto TryRestartGame1CurrentLevel = [&]() -> bool
		{
			if (game1CurrentLevelPath.empty())
				return false;

			const bool wasCoop = game1Player2Joined;
			const GAME1_PlayerBinding p1Binding = game1Player.getBinding();
			const GAME1_PlayerBinding p2Binding = game1Player2.getBinding();

			if (!TryLoadGame1Level(game1CurrentLevelPath))
				return false;

			SetAppState(AppState::GAME1_Game);

			if (wasCoop)
			{
				const std::string player2IdleDirectory =
					(kGame1ResourcesDirectory / "Player2" / "PlayerIdle").string();
				const sf::Vector2f startPosition = game1Level.getPlayerSpawnPosition();
				const float playerWidth = game1Player.getBounds().size.x;
				const float maxX = std::max(0.f, game1Level.getPixelWidth() - playerWidth);
				sf::Vector2f p2StartPosition = startPosition;
				p2StartPosition.x -= kGame1CoopJoinSpawnOffsetX;
				if (p2StartPosition.x < 0.f || p2StartPosition.x > maxX)
					p2StartPosition.x = startPosition.x + kGame1CoopJoinSpawnOffsetX;
				p2StartPosition.x = std::clamp(p2StartPosition.x, 0.f, maxX);

				if (!game1Player2.load(player2IdleDirectory, p2StartPosition))
				{
					const std::string msg =
						"Surfers Quest Player 2 failed to load.\n\n" +
						game1Player2.getLastError() +
						"\n\nCurrent working directory:\n" +
						std::filesystem::current_path().string();

					ShowError(msg);
					return false;
				}

				game1Player.setBinding(p1Binding);
				game1Player.setCoopMode(true);
				game1Player.setDrawHud(false);

				game1Player2.setSpawnPosition(startPosition);
				game1Player2.setBinding(p2Binding);
				game1Player2.setCoopMode(true);
				game1Player2.setDrawHud(false);
				game1Player2Joined = true;
				game1Player1DetectedSource = p1Binding;
			}

			game1TeamGameOver = false;
			game1CameraNeedsSnap = true;
			GAME1_SurfersQuestAudio::playGameplay();
			ArcadeInput::consumePressedState();
			return true;
		};

	auto TryStartGame1NextLevel = [&]() -> bool
		{
			const std::vector<std::string> levelPaths = GetAllGame1LevelPaths();
			const int currentIndex = GetGame1CurrentLevelIndex();

			if (levelPaths.empty() || currentIndex < 0)
			{
				return false;
			}

			const std::size_t nextIndex =
				(static_cast<std::size_t>(currentIndex) + 1U) % levelPaths.size();

			if (!TryLoadGame1Level(levelPaths[nextIndex]))
				return false;

			SetAppState(AppState::GAME1_Game);
			ArcadeInput::consumePressedState();
			return true;
		};

	auto LayoutGame1VictoryPopup = [&]() -> sf::FloatRect
		{
			const float windowWidth = static_cast<float>(window.getSize().x);
			const float windowHeight = static_cast<float>(window.getSize().y);
			const bool multiplayer = game1Player2Joined;

			auto FruitRows = [](const GAME1_PlayerScoreState& scoreState) -> int
				{
					if (scoreState.forceStatsNA || scoreState.collectedFruits.empty())
						return 1;

					return static_cast<int>((scoreState.collectedFruits.size() + 3) / 4);
				};

			int collectedRows;
			if (multiplayer)
			{
				collectedRows = std::max(
					FruitRows(game1Scores[0]),
					FruitRows(game1Scores[1]));
			}
			else
			{
				collectedRows = game1RunCollectedFruits.empty()
					? 1
					: static_cast<int>((game1RunCollectedFruits.size() + 3) / 4);
			}

			const float panelWidth = multiplayer
				? std::min(940.f, std::max(720.f, windowWidth - 120.f))
				: std::min(760.f, std::max(520.f, windowWidth - 180.f));
			const float desiredHeight = (multiplayer ? 380.f : 350.f) +
				static_cast<float>(collectedRows) * (multiplayer ? 46.f : 58.f);
			const float panelHeight = std::min(
				std::max(multiplayer ? 420.f : 390.f, desiredHeight),
				std::max(360.f, windowHeight - 80.f));

			const sf::FloatRect panelBounds(
				{ (windowWidth - panelWidth) * 0.5f, (windowHeight - panelHeight) * 0.5f },
				{ panelWidth, panelHeight });

			const float buttonWidth = std::min(190.f, (panelWidth - 120.f) * 0.5f);
			const float buttonHeight = 54.f;
			const float buttonGap = 28.f;
			const float buttonsY = panelBounds.position.y + panelBounds.size.y - buttonHeight - 34.f;
			const float buttonsX = panelBounds.position.x + (panelWidth - (buttonWidth * 2.f + buttonGap)) * 0.5f;

			game1VictoryRetryBounds = sf::FloatRect(
				{ buttonsX, buttonsY },
				{ buttonWidth, buttonHeight });

			game1VictoryNextBounds = sf::FloatRect(
				{ buttonsX + buttonWidth + buttonGap, buttonsY },
				{ buttonWidth, buttonHeight });

			if (!Game1HasNextLevel() && game1VictorySelectedButton == 1)
				game1VictorySelectedButton = 0;

			return panelBounds;
		};

	auto ActivateGame1VictoryButton = [&](int buttonIndex) -> bool
		{
			if (buttonIndex == 0)
			{
				ArcadeUISounds::playUIClick();
				return TryRestartGame1CurrentLevel();
			}

			if (buttonIndex == 1 && Game1HasNextLevel())
			{
				ArcadeUISounds::playUIClick();
				return TryStartGame1NextLevel();
			}

			return true;
		};

	auto SelectGame1VictoryButton = [&](int buttonIndex)
		{
			game1VictorySelectedButton = Game1HasNextLevel()
				? std::clamp(buttonIndex, 0, 1)
				: 0;
		};

	auto HandleGame1VictoryMenuInput = [&]() -> bool
		{
			if (!game1VictoryPopupOpen)
				return true;

			LayoutGame1VictoryPopup();

			if (ArcadeInput::isMoveLeftPressed() ||
				ArcadeInput::isMoveUpPressed())
			{
				SelectGame1VictoryButton(0);
				ArcadeInput::consumePressedState();
			}
			else if (ArcadeInput::isMoveRightPressed() ||
				ArcadeInput::isMoveDownPressed())
			{
				SelectGame1VictoryButton(1);
				ArcadeInput::consumePressedState();
			}
			else if (ArcadeInput::isConfirmPressed() ||
				ArcadeInput::isPrimaryPressed())
			{
				if (!ActivateGame1VictoryButton(game1VictorySelectedButton))
					return false;

				ArcadeInput::consumePressedState();
			}

			return true;
		};

	auto TryLaunchSelectedHubGame = [&]()
		{
			if (hub.isSelectedGameLocked())
				return;

			ArcadeUISounds::playUIClick();

			if (hub.getSelectedIndex() == 0)
			{
				SetAppState(AppState::GAME1_BombermanMenu);
			}
			else if (hub.getSelectedIndex() == 1)
			{
				SetAppState(AppState::GAME1_Menu);
			}
			else if (hub.getSelectedIndex() == 2)
			{
				SetAppState(AppState::GAME2_Menu);
			}
		};

	auto HandleBombermanMenuAction = [&](GAME1_BombermanMenuAction action)
		{
			switch (action)
			{
			case GAME1_BombermanMenuAction::PlayLevels:
				bombermanLevelSelect.refreshLevelList();
				SetAppState(AppState::GAME1_BombermanLevelSelect);
				break;

			case GAME1_BombermanMenuAction::LevelEditor:
				bombermanEditor.reset();

				if (!bombermanEditor.getLastError().empty())
				{
					ShowError(bombermanEditor.getLastError());
				}
				else
				{
					SetAppState(AppState::GAME1_BombermanEditor);
				}

				break;

			case GAME1_BombermanMenuAction::BackToHub:
				SetAppState(AppState::Hub);
				break;

			case GAME1_BombermanMenuAction::None:
			default:
				break;
			}
		};

	auto HandleGame1MenuAction = [&](GAME1_MenuAction action)
		{
			switch (action)
			{
			case GAME1_MenuAction::Quit:
				SetAppState(AppState::Hub);
				break;

			case GAME1_MenuAction::Play:
				game1LevelSelect.setLevels(GetAllGame1LevelPaths());
				SetAppState(AppState::GAME1_LevelSelect);
				break;

			case GAME1_MenuAction::LevelEditor:
				game1Editor.resetEmpty();
				SetAppState(AppState::GAME1_Editor);
				break;

			case GAME1_MenuAction::None:
			default:
				break;
		}
	};

	auto IsPausableGameplayState = [](AppState state)
		{
			return state == AppState::GAME1_Bomberman ||
				state == AppState::GAME1_Game;
		};

	auto UpdateMouseCursorVisibility = [&]()
		{
			const bool gameplayCursorHidden = IsPausableGameplayState(appState);
			const bool menuCursorVisible = pauseMenu.isOpen() || hubOptions.isOpen();

			window.setMouseCursorVisible(!gameplayCursorHidden || menuCursorVisible);
		};

	auto OpenPauseMenu = [&]()
		{
			if (!IsPausableGameplayState(appState))
				return;

			if (pauseMenu.isOpen() || hubOptions.isOpen())
				return;

			ArcadeUISounds::playUIClick();
			hubOptions.setGearVisible(false);
			pauseMenu.layout(window);
			pauseMenu.open();
			window.setMouseCursorVisible(true);
		};

	auto HandlePauseAction = [&](GamePauseMenu::Action action)
		{
			switch (action)
			{
			case GamePauseMenu::Action::Resume:
				pauseMenu.close();
				hubOptions.setGearVisible(true);
				UpdateMouseCursorVisibility();
				if (appState == AppState::GAME1_Bomberman)
				{
					bombermanWindow.suppressActionInputUntilReleased();
				}
				ArcadeInput::consumePressedState();
				break;

			case GamePauseMenu::Action::OpenSettings:
				hubOptions.open();
				ArcadeInput::consumePressedState();
				break;

			case GamePauseMenu::Action::Quit:
			{
				const AppState target = (appState == AppState::GAME1_Bomberman)
					? AppState::GAME1_BombermanMenu
					: AppState::GAME1_Menu;
				SetAppState(target);
				break;
			}

			case GamePauseMenu::Action::None:
			default:
				break;
			}
		};

	auto ClampGame1CoopPosition = [&game1Level, &game1Player](sf::Vector2f position)
		{
			const float playerWidth = game1Player.getBounds().size.x;
			const float maxX = std::max(0.f, game1Level.getPixelWidth() - playerWidth);

			position.x = std::clamp(position.x, 0.f, maxX);
			return position;
		};

	auto GetGame1CoopOffsetPosition = [&ClampGame1CoopPosition, &game1Level, &game1Player](
		sf::Vector2f anchorPosition,
		float xOffset)
		{
			const float playerWidth = game1Player.getBounds().size.x;
			const float maxX = std::max(0.f, game1Level.getPixelWidth() - playerWidth);

			sf::Vector2f position = anchorPosition;
			position.x += xOffset;

			if (position.x < 0.f || position.x > maxX)
			{
				position.x = anchorPosition.x - xOffset;
			}

			return ClampGame1CoopPosition(position);
		};

	auto IsGame1SafeRespawnPosition = [&game1Level](
		sf::Vector2f position,
		sf::Vector2f playerSize) -> bool
		{
			if (playerSize.x <= 0.f || playerSize.y <= 0.f)
				return false;

			if (position.x < 0.f || position.y < 0.f)
				return false;

			if (position.x + playerSize.x > game1Level.getPixelWidth() ||
				position.y + playerSize.y > game1Level.getPixelHeight())
			{
				return false;
			}

			const float tileSize = static_cast<float>(GAME1_Level::TileSize);
			const sf::FloatRect playerBounds(position, playerSize);
			const int leftTile = static_cast<int>(std::floor(playerBounds.position.x / tileSize));
			const int rightTile = static_cast<int>(std::floor(
				(playerBounds.position.x + playerBounds.size.x - 0.1f) / tileSize));
			const int topTile = static_cast<int>(std::floor(playerBounds.position.y / tileSize));
			const int bottomTile = static_cast<int>(std::floor(
				(playerBounds.position.y + playerBounds.size.y - 0.1f) / tileSize));

			for (int row = topTile; row <= bottomTile; ++row)
			{
				for (int col = leftTile; col <= rightTile; ++col)
				{
					if (game1Level.isSolidTile(col, row) ||
						game1Level.isSpikeTrapTile(col, row) ||
						game1Level.isOneWayPlatformTile(col, row))
					{
						return false;
					}
				}
			}

			if (game1Level.getActiveFireDamageBounds(playerBounds).has_value() ||
				game1Level.getSpikeHeadDamageBounds(playerBounds).has_value() ||
				game1Level.getSawDamageBounds(playerBounds).has_value())
			{
				return false;
			}

			return true;
		};

	auto GetGame1SafeRespawnPositionAround = [&IsGame1SafeRespawnPosition](
		const GAME1_Player& respawningPlayer,
		sf::Vector2f preferredPosition) -> std::optional<sf::Vector2f>
		{
			const float tileSize = static_cast<float>(GAME1_Level::TileSize);
			const sf::Vector2f playerSize = respawningPlayer.getBounds().size;
			const int preferredCol = static_cast<int>(std::floor(preferredPosition.x / tileSize));
			const int preferredRow = static_cast<int>(std::floor(preferredPosition.y / tileSize));

			for (int radius = 0; radius <= 5; ++radius)
			{
				std::optional<sf::Vector2f> bestPosition;
				int bestScore = 0;

				for (int row = preferredRow - radius; row <= preferredRow + radius; ++row)
				{
					for (int col = preferredCol - radius; col <= preferredCol + radius; ++col)
					{
						if (std::max(std::abs(col - preferredCol), std::abs(row - preferredRow)) != radius)
							continue;

						const sf::Vector2f candidate(
							static_cast<float>(col * GAME1_Level::TileSize),
							static_cast<float>(row * GAME1_Level::TileSize));

						if (!IsGame1SafeRespawnPosition(candidate, playerSize))
							continue;

						const int score =
							(col - preferredCol) * (col - preferredCol) +
							(row - preferredRow) * (row - preferredRow);

						if (!bestPosition.has_value() || score < bestScore)
						{
							bestPosition = candidate;
							bestScore = score;
						}
					}
				}

				if (bestPosition.has_value())
					return bestPosition;
			}

			return std::nullopt;
		};

	auto GetGame1SafeRespawnPositionNear = [&IsGame1SafeRespawnPosition](
		const GAME1_Player& respawningPlayer,
		const GAME1_Player& anchorPlayer) -> std::optional<sf::Vector2f>
		{
			const float tileSize = static_cast<float>(GAME1_Level::TileSize);
			const sf::FloatRect anchorBounds = anchorPlayer.getBounds();
			const sf::Vector2f playerSize = respawningPlayer.getBounds().size;

			const int anchorCol = static_cast<int>(std::floor(anchorBounds.position.x / tileSize));
			const int anchorRow = static_cast<int>(std::floor(anchorBounds.position.y / tileSize));
			const int preferredCol = anchorCol - 1;
			const int preferredRow = anchorRow - 1;

			for (int radius = 0; radius <= 5; ++radius)
			{
				std::optional<sf::Vector2f> bestPosition;
				int bestScore = 0;

				for (int row = preferredRow - radius; row <= preferredRow + radius; ++row)
				{
					for (int col = preferredCol - radius; col <= preferredCol + radius; ++col)
					{
						if (std::max(std::abs(col - preferredCol), std::abs(row - preferredRow)) != radius)
							continue;

						const sf::Vector2f candidate(
							static_cast<float>(col * GAME1_Level::TileSize),
							static_cast<float>(row * GAME1_Level::TileSize));

						if (!IsGame1SafeRespawnPosition(candidate, playerSize))
							continue;

						const int distToAnchor =
							(col - anchorCol) * (col - anchorCol) +
							(row - anchorRow) * (row - anchorRow);
						const int belowAnchorPenalty = row > anchorRow ? 32 : 0;
						const int score =
							distToAnchor * 4 +
							std::abs(col - preferredCol) +
							std::abs(row - preferredRow) +
							belowAnchorPenalty;

						if (!bestPosition.has_value() || score < bestScore)
						{
							bestPosition = candidate;
							bestScore = score;
						}
					}
				}

				if (bestPosition.has_value())
					return bestPosition;
			}

			return std::nullopt;
		};

	auto MoveGame1RespawningPlayerToward = [&IsGame1SafeRespawnPosition, &GetGame1SafeRespawnPositionNear](
		GAME1_Player& respawningPlayer,
		const GAME1_Player& anchorPlayer,
		float frameDeltaTime)
		{
			if (!respawningPlayer.isRespawning())
				return;

			if (anchorPlayer.isGameOver() ||
				anchorPlayer.isRespawning() ||
				anchorPlayer.hasLevelFinishFailed())
			{
				return;
			}

			const std::optional<sf::Vector2f> targetPosition =
				GetGame1SafeRespawnPositionNear(respawningPlayer, anchorPlayer);
			if (!targetPosition.has_value())
				return;

			const sf::Vector2f currentPosition = respawningPlayer.getPosition();
			const float followRate = 12.f;
			const float followT =
				1.f - std::exp(-followRate * std::max(0.f, frameDeltaTime));

			sf::Vector2f nextPosition(
				currentPosition.x + (targetPosition->x - currentPosition.x) * followT,
				currentPosition.y + (targetPosition->y - currentPosition.y) * followT);

			const sf::Vector2f playerSize = respawningPlayer.getBounds().size;
			if (!IsGame1SafeRespawnPosition(nextPosition, playerSize))
				nextPosition = *targetPosition;

			respawningPlayer.setPosition(nextPosition);
			respawningPlayer.setNextRespawnPosition(nextPosition);
		};

	auto TryJoinGame1Player2 = [&](GAME1_PlayerBinding p2Binding) -> bool
		{
			if (appState != AppState::GAME1_Game)
				return false;

			if (game1Player2Joined)
				return false;

			if (game1TeamGameOver)
				return false;

			if (game1Player.isGameOver())
				return false;

			const std::string player2IdleDirectory =
				(kGame1ResourcesDirectory / "Player2" / "PlayerIdle").string();

			const sf::Vector2f spawnPosition = GetGame1CoopOffsetPosition(
				game1Player.getPosition(),
				-kGame1CoopJoinSpawnOffsetX);

			if (!game1Player2.load(player2IdleDirectory, spawnPosition))
			{
				const std::string msg =
					"Surfers Quest Player 2 failed to load.\n\n" +
					game1Player2.getLastError() +
					"\n\nCurrent working directory:\n" +
					std::filesystem::current_path().string();

				ShowError(msg);
				return false;
			}

			game1Player2.setSpawnPosition(game1Player.getSpawnPosition());

			// Derive P1 binding: use the detected source if available, otherwise
			// infer from P2's source (P2 on joystick 0 → P1 gets keyboard; otherwise joystick 0).
			GAME1_PlayerBinding p1Binding;
			if (!game1Player1DetectedSource.singlePlayer)
			{
				p1Binding = game1Player1DetectedSource;
			}
			else if (p2Binding.source == GAME1_InputSource::Keyboard)
			{
				p1Binding = {false, GAME1_InputSource::Joystick, 0};
			}
			else if (p2Binding.joystickIndex == 0)
			{
				p1Binding = {false, GAME1_InputSource::Keyboard, 0};
			}
			else
			{
				p1Binding = {false, GAME1_InputSource::Joystick, 0};
			}

			game1Player.setBinding(p1Binding);
			game1Player.setCoopMode(true);
			game1Player.setDrawHud(false);

			game1Player2.setBinding(p2Binding);
			game1Player2.setCoopMode(true);
			game1Player2.setDrawHud(false);

			game1Player2Joined = true;
			game1Scores[1].reset();
			return true;
		};

	auto RestartGame1Team = [&]() -> bool
		{
			return TryRestartGame1CurrentLevel();
		};

	auto TryStartBombermanLevel = [&](const std::string& mapPath) -> bool
		{
			if (!bombermanWindow.loadMapFromFile(mapPath))
			{
				std::string msg =
					"Bomberman level failed to load.\n\n" +
					bombermanWindow.getLastError() +
					"\n\nCurrent working directory:\n" +
					std::filesystem::current_path().string();

				ShowError(msg);
				return false;
			}

			bombermanWindow.suppressActionInputUntilReleased();
			ArcadeInput::consumePressedState();
			SetAppState(AppState::GAME1_Bomberman);
			return true;
		};

	auto ReportBombermanEditorResult =
		[&](const std::string& previousSavedPath, const std::string& previousError)
		{
			const std::string currentError = bombermanEditor.getLastError();
			const std::string currentSavedPath = bombermanEditor.getLastSavedPath();

			if (!currentError.empty() && currentError != previousError)
			{
				ShowError(currentError);
				return;
			}

			if (!currentSavedPath.empty() && currentSavedPath != previousSavedPath)
			{
				ShowInfo("Bomberman level saved successfully.\n\n" + currentSavedPath);
				bombermanLevelSelect.refreshLevelList();
			}
		};

	sf::Clock clock;

	while (window.isOpen())
	{
		const float deltaTime = clock.restart().asSeconds();
		totalAppTime += deltaTime;

		ArcadeInput::update();

		bombermanEditor.layout(window);
		bombermanLevelSelect.layout(window);
		bombermanMenu.layout(window);
		game1Editor.layout(window);

		while (const std::optional event = window.pollEvent())
		{
			if (event->is<sf::Event::Closed>())
			{
				window.close();
			}

			if (const auto* resized = event->getIf<sf::Event::Resized>())
			{
				(void)resized;

				ApplyWindowView(window);

				if (!ResizeTexture(crtFrameTexture, window.getSize()))
				{
					crtShaderLoaded = false;
				}
				else
				{
					crtFrameTexture.setSmooth(false);
				}

				if (appState == AppState::GAME2_Game)
				{
					game2Game.reset(window.getSize());
				}
			}

			if (appState == AppState::Hub)
			{
				const bool popupOpen = hubOptions.isOpen();

				if (!popupOpen &&
					(event->is<sf::Event::KeyPressed>() ||
					 event->is<sf::Event::KeyReleased>() ||
					 event->is<sf::Event::MouseButtonPressed>() ||
					 event->is<sf::Event::MouseButtonReleased>() ||
					 event->is<sf::Event::MouseMoved>() ||
					 event->is<sf::Event::MouseWheelScrolled>()))
				{
					hub.notifyUserActivity();
				}

				if (popupOpen)
				{
					if (const auto* keyReleased = event->getIf<sf::Event::KeyReleased>())
					{
						hubOptions.handleKeyReleased(keyReleased->code);
					}

					if (const auto* mousePressed = event->getIf<sf::Event::MouseButtonPressed>())
					{
						if (mousePressed->button == sf::Mouse::Button::Left)
						{
							hubOptions.handleMousePressed({
								static_cast<float>(mousePressed->position.x),
								static_cast<float>(mousePressed->position.y)
								});
						}
					}

					if (const auto* mouseReleased = event->getIf<sf::Event::MouseButtonReleased>())
					{
						if (mouseReleased->button == sf::Mouse::Button::Left)
						{
							hubOptions.handleMouseReleased({
								static_cast<float>(mouseReleased->position.x),
								static_cast<float>(mouseReleased->position.y)
								});
						}
					}

					if (const auto* mouseMoved = event->getIf<sf::Event::MouseMoved>())
					{
						hubOptions.handleMouseMoved({
							static_cast<float>(mouseMoved->position.x),
							static_cast<float>(mouseMoved->position.y)
							});
					}
				}
				else
				{
					if (const auto* keyReleased = event->getIf<sf::Event::KeyReleased>())
					{
						if (keyReleased->code == sf::Keyboard::Key::Escape)
						{
							ArcadeUISounds::playUIClick();
							hubOptions.open();
						}
						else if (keyReleased->code == sf::Keyboard::Key::Left)
						{
							hub.navigateLeft();
						}
						else if (keyReleased->code == sf::Keyboard::Key::Right)
						{
							hub.navigateRight();
						}
						else if (keyReleased->code == sf::Keyboard::Key::Enter)
						{
							if (!hub.isTransitioning())
							{
								TryLaunchSelectedHubGame();
							}
						}
					}

					if (const auto* mousePressed = event->getIf<sf::Event::MouseButtonPressed>())
					{
						if (mousePressed->button == sf::Mouse::Button::Left)
						{
							const sf::Vector2f mousePosition(
								static_cast<float>(mousePressed->position.x),
								static_cast<float>(mousePressed->position.y)
							);

							if (hubOptions.handleMousePressed(mousePosition))
							{
								// Click consumed by gear button.
							}
							else
							{
								switch (hub.handleClick(mousePosition))
								{
								case ArcadeHubAction::PreviousGame:
									hub.navigateLeft();
									break;

								case ArcadeHubAction::NextGame:
									hub.navigateRight();
									break;

								case ArcadeHubAction::LaunchGame:
									TryLaunchSelectedHubGame();
									break;

								case ArcadeHubAction::None:
								default:
									break;
								}
							}
						}
					}
				}
			}
			else if (appState == AppState::GAME1_BombermanMenu)
			{
				if (const auto* keyReleased = event->getIf<sf::Event::KeyReleased>())
				{
					const bool menuConsumedKey = bombermanMenu.handleKeyReleased(keyReleased->code);

					if (!menuConsumedKey)
					{
						if (keyReleased->code == sf::Keyboard::Key::Escape)
						{
							ArcadeUISounds::playUIClick();
							SetAppState(AppState::Hub);
						}
						else if (keyReleased->code == sf::Keyboard::Key::Enter ||
							keyReleased->code == sf::Keyboard::Key::Space)
						{
							HandleBombermanMenuAction(bombermanMenu.activateSelectedItem());
						}
					}
				}

				if (const auto* mousePressed = event->getIf<sf::Event::MouseButtonPressed>())
				{
					if (mousePressed->button == sf::Mouse::Button::Left)
					{
						const sf::Vector2f mousePosition(
							static_cast<float>(mousePressed->position.x),
							static_cast<float>(mousePressed->position.y)
						);

						HandleBombermanMenuAction(bombermanMenu.handleClick(mousePosition));
					}
				}

				if (const auto* mouseMoved = event->getIf<sf::Event::MouseMoved>())
				{
					const sf::Vector2f mousePosition(
						static_cast<float>(mouseMoved->position.x),
						static_cast<float>(mouseMoved->position.y)
					);

					bombermanMenu.handleMouseMoved(mousePosition);
				}
			}
			else if (appState == AppState::GAME1_BombermanLevelSelect)
			{
				if (const auto* keyReleased = event->getIf<sf::Event::KeyReleased>())
				{
					if (keyReleased->code == sf::Keyboard::Key::Escape)
					{
						ArcadeUISounds::playUIClick();
						SetAppState(AppState::GAME1_BombermanMenu);
					}
					else if (keyReleased->code == sf::Keyboard::Key::Up ||
						keyReleased->code == sf::Keyboard::Key::W)
					{
						bombermanLevelSelect.selectPreviousSlot();
					}
					else if (keyReleased->code == sf::Keyboard::Key::Down ||
						keyReleased->code == sf::Keyboard::Key::S)
					{
						bombermanLevelSelect.selectNextSlot();
					}
					else if (keyReleased->code == sf::Keyboard::Key::Left ||
						keyReleased->code == sf::Keyboard::Key::A)
					{
						bombermanLevelSelect.selectPreviousPage();
					}
					else if (keyReleased->code == sf::Keyboard::Key::Right ||
						keyReleased->code == sf::Keyboard::Key::D)
					{
						bombermanLevelSelect.selectNextPage();
					}
					else if (keyReleased->code == sf::Keyboard::Key::Enter ||
						keyReleased->code == sf::Keyboard::Key::Space)
					{
						const GAME1_BombermanLevelSelectAction action =
							bombermanLevelSelect.activateSelectedSlot();

						if (action == GAME1_BombermanLevelSelectAction::SelectedLevel)
						{
							ArcadeUISounds::playUIClick();
							if (!TryStartBombermanLevel(bombermanLevelSelect.getSelectedLevelPath()))
								return -1;
						}
					}
				}

				if (const auto* mousePressed = event->getIf<sf::Event::MouseButtonPressed>())
				{
					if (mousePressed->button == sf::Mouse::Button::Left)
					{
						const sf::Vector2f mousePosition(
							static_cast<float>(mousePressed->position.x),
							static_cast<float>(mousePressed->position.y)
						);

						const GAME1_BombermanLevelSelectAction action =
							bombermanLevelSelect.handleClick(mousePosition);

						switch (action)
						{
						case GAME1_BombermanLevelSelectAction::Back:
							ArcadeUISounds::playUIClick();
							SetAppState(AppState::GAME1_BombermanMenu);
							break;

						case GAME1_BombermanLevelSelectAction::SelectedLevel:
							ArcadeUISounds::playUIClick();
							if (!TryStartBombermanLevel(bombermanLevelSelect.getSelectedLevelPath()))
								return -1;

							break;

						case GAME1_BombermanLevelSelectAction::PreviousPage:
						case GAME1_BombermanLevelSelectAction::NextPage:
						case GAME1_BombermanLevelSelectAction::None:
						default:
							break;
						}
					}
				}

				if (const auto* mouseMoved = event->getIf<sf::Event::MouseMoved>())
				{
					const sf::Vector2f mousePosition(
						static_cast<float>(mouseMoved->position.x),
						static_cast<float>(mouseMoved->position.y)
					);

					bombermanLevelSelect.handleMouseMoved(mousePosition);
				}
			}
			else if (appState == AppState::GAME1_BombermanEditor)
			{
				if (const auto* keyReleased = event->getIf<sf::Event::KeyReleased>())
				{
					if (keyReleased->code == sf::Keyboard::Key::Escape)
					{
						SetAppState(AppState::GAME1_BombermanMenu);
					}
					else
					{
						const std::string previousSavedPath = bombermanEditor.getLastSavedPath();
						const std::string previousError = bombermanEditor.getLastError();

						bombermanEditor.handleKeyReleased(keyReleased->code);
						ReportBombermanEditorResult(previousSavedPath, previousError);
					}
				}

				if (const auto* mousePressed = event->getIf<sf::Event::MouseButtonPressed>())
				{
					const sf::Vector2i mousePixelPosition{
						mousePressed->position.x,
						mousePressed->position.y
					};

					const std::string previousSavedPath = bombermanEditor.getLastSavedPath();
					const std::string previousError = bombermanEditor.getLastError();

					bombermanEditor.handleMousePressed(mousePressed->button, mousePixelPosition);
					ReportBombermanEditorResult(previousSavedPath, previousError);
				}

				if (const auto* mouseWheelScrolled = event->getIf<sf::Event::MouseWheelScrolled>())
				{
					if (mouseWheelScrolled->wheel == sf::Mouse::Wheel::Vertical)
					{
						bombermanEditor.handleMouseWheelScrolled(mouseWheelScrolled->delta);
					}
				}
			}
			else if (appState == AppState::GAME1_Bomberman)
			{
				if (hubOptions.isOpen())
				{
					if (const auto* keyReleased = event->getIf<sf::Event::KeyReleased>())
					{
						hubOptions.handleKeyReleased(keyReleased->code);
					}
					if (const auto* mousePressed = event->getIf<sf::Event::MouseButtonPressed>())
					{
						if (mousePressed->button == sf::Mouse::Button::Left)
						{
							hubOptions.handleMousePressed({
								static_cast<float>(mousePressed->position.x),
								static_cast<float>(mousePressed->position.y)
								});
						}
					}
					if (const auto* mouseReleased = event->getIf<sf::Event::MouseButtonReleased>())
					{
						if (mouseReleased->button == sf::Mouse::Button::Left)
						{
							hubOptions.handleMouseReleased({
								static_cast<float>(mouseReleased->position.x),
								static_cast<float>(mouseReleased->position.y)
								});
						}
					}
					if (const auto* mouseMoved = event->getIf<sf::Event::MouseMoved>())
					{
						hubOptions.handleMouseMoved({
							static_cast<float>(mouseMoved->position.x),
							static_cast<float>(mouseMoved->position.y)
							});
					}
				}
				else if (pauseMenu.isOpen())
				{
					if (const auto* keyReleased = event->getIf<sf::Event::KeyReleased>())
					{
						if (keyReleased->code == sf::Keyboard::Key::Escape)
						{
							ArcadeUISounds::playUIClick();
							HandlePauseAction(GamePauseMenu::Action::Resume);
						}
						else
						{
							HandlePauseAction(pauseMenu.handleKeyReleased(keyReleased->code));
						}
					}
					if (const auto* mousePressed = event->getIf<sf::Event::MouseButtonPressed>())
					{
						if (mousePressed->button == sf::Mouse::Button::Left)
						{
							HandlePauseAction(pauseMenu.handleMousePressed({
								static_cast<float>(mousePressed->position.x),
								static_cast<float>(mousePressed->position.y)
								}));
						}
					}
					if (const auto* mouseMoved = event->getIf<sf::Event::MouseMoved>())
					{
						pauseMenu.handleMouseMoved({
							static_cast<float>(mouseMoved->position.x),
							static_cast<float>(mouseMoved->position.y)
						});
					}
				}
				else if (game1VictoryPopupOpen)
				{
					LayoutGame1VictoryPopup();

					if (const auto* mousePressed = event->getIf<sf::Event::MouseButtonPressed>())
					{
						if (mousePressed->button == sf::Mouse::Button::Left)
						{
							const sf::Vector2f mousePosition(
								static_cast<float>(mousePressed->position.x),
								static_cast<float>(mousePressed->position.y));

							if (PointInRect(game1VictoryRetryBounds, mousePosition))
							{
								SelectGame1VictoryButton(0);
								if (!ActivateGame1VictoryButton(0))
									return -1;
							}
							else if (Game1HasNextLevel() &&
								PointInRect(game1VictoryNextBounds, mousePosition))
							{
								SelectGame1VictoryButton(1);
								if (!ActivateGame1VictoryButton(1))
									return -1;
							}
						}
					}

					if (const auto* mouseMoved = event->getIf<sf::Event::MouseMoved>())
					{
						const sf::Vector2f mousePosition(
							static_cast<float>(mouseMoved->position.x),
							static_cast<float>(mouseMoved->position.y));

						if (PointInRect(game1VictoryRetryBounds, mousePosition))
							SelectGame1VictoryButton(0);
						else if (Game1HasNextLevel() &&
							PointInRect(game1VictoryNextBounds, mousePosition))
							SelectGame1VictoryButton(1);
					}
				}
				else
				{
					if (const auto* keyReleased = event->getIf<sf::Event::KeyReleased>())
					{
						if (keyReleased->code == sf::Keyboard::Key::Escape)
						{
							OpenPauseMenu();
						}
					}
				}
			}
			else if (appState == AppState::GAME1_Menu)
			{
				if (const auto* keyReleased = event->getIf<sf::Event::KeyReleased>())
				{
					const bool consumedByMenu = game1Menu.handleKeyReleased(keyReleased->code);

					if (!consumedByMenu)
					{
						if (keyReleased->code == sf::Keyboard::Key::Escape)
						{
							ArcadeUISounds::playUIClick();
							SetAppState(AppState::Hub);
						}
						else if (keyReleased->code == sf::Keyboard::Key::Enter ||
							keyReleased->code == sf::Keyboard::Key::Space)
						{
							HandleGame1MenuAction(game1Menu.activateSelectedItem());
						}
					}
				}

				if (const auto* mousePressed = event->getIf<sf::Event::MouseButtonPressed>())
				{
					if (mousePressed->button == sf::Mouse::Button::Left)
					{
						const sf::Vector2f mousePosition(
							static_cast<float>(mousePressed->position.x),
							static_cast<float>(mousePressed->position.y)
						);

						HandleGame1MenuAction(game1Menu.handleClick(mousePosition));
					}
				}

				if (const auto* mouseMoved = event->getIf<sf::Event::MouseMoved>())
				{
					const sf::Vector2f mousePosition(
						static_cast<float>(mouseMoved->position.x),
						static_cast<float>(mouseMoved->position.y)
					);

					game1Menu.handleMouseMoved(mousePosition);
				}
			}
			else if (appState == AppState::GAME1_LevelSelect)
			{
				if (const auto* keyReleased = event->getIf<sf::Event::KeyReleased>())
				{
					if (keyReleased->code == sf::Keyboard::Key::Escape)
					{
						ArcadeUISounds::playUIClick();
						SetAppState(AppState::GAME1_Menu);
					}
					else if (keyReleased->code == sf::Keyboard::Key::Up ||
						keyReleased->code == sf::Keyboard::Key::W)
					{
						game1LevelSelect.selectPreviousSlot();
					}
					else if (keyReleased->code == sf::Keyboard::Key::Down ||
						keyReleased->code == sf::Keyboard::Key::S)
					{
						game1LevelSelect.selectNextSlot();
					}
					else if (keyReleased->code == sf::Keyboard::Key::Left ||
						keyReleased->code == sf::Keyboard::Key::A)
					{
						game1LevelSelect.selectPreviousPage();
					}
					else if (keyReleased->code == sf::Keyboard::Key::Right ||
						keyReleased->code == sf::Keyboard::Key::D)
					{
						game1LevelSelect.selectNextPage();
					}
					else if (keyReleased->code == sf::Keyboard::Key::Enter ||
						keyReleased->code == sf::Keyboard::Key::Space)
					{
						const int slotIndex = game1LevelSelect.activateSelectedSlot();

						if (slotIndex >= 0 && !game1LevelSelect.getSelectedLevelPath().empty())
						{
							ArcadeUISounds::playUIClick();
							if (!TryLoadGame1Level(game1LevelSelect.getSelectedLevelPath()))
								return -1;

							SetAppState(AppState::GAME1_Game);
						}
					}
				}

				if (const auto* mousePressed = event->getIf<sf::Event::MouseButtonPressed>())
				{
					if (mousePressed->button == sf::Mouse::Button::Left)
					{
						const sf::Vector2f mousePosition(
							static_cast<float>(mousePressed->position.x),
							static_cast<float>(mousePressed->position.y)
						);

						const int slotIndex = game1LevelSelect.handleClick(mousePosition);

						if (slotIndex == GAME1_LevelSelect::BackClickedSentinel)
						{
							ArcadeUISounds::playUIClick();
							SetAppState(AppState::GAME1_Menu);
						}
						else if (slotIndex >= 0 && game1LevelSelect.hasLevelAt(slotIndex))
						{
							ArcadeUISounds::playUIClick();
							if (!TryLoadGame1Level(game1LevelSelect.getLevelPathAt(slotIndex)))
								return -1;

							SetAppState(AppState::GAME1_Game);
						}
					}
				}

				if (const auto* mouseMoved = event->getIf<sf::Event::MouseMoved>())
				{
					const sf::Vector2f mousePosition(
						static_cast<float>(mouseMoved->position.x),
						static_cast<float>(mouseMoved->position.y)
					);

					game1LevelSelect.handleMouseMoved(mousePosition);
				}
			}
			else if (appState == AppState::GAME1_Game)
			{
				if (hubOptions.isOpen())
				{
					if (const auto* keyReleased = event->getIf<sf::Event::KeyReleased>())
					{
						hubOptions.handleKeyReleased(keyReleased->code);
					}
					if (const auto* mousePressed = event->getIf<sf::Event::MouseButtonPressed>())
					{
						if (mousePressed->button == sf::Mouse::Button::Left)
						{
							hubOptions.handleMousePressed({
								static_cast<float>(mousePressed->position.x),
								static_cast<float>(mousePressed->position.y)
								});
						}
					}
					if (const auto* mouseReleased = event->getIf<sf::Event::MouseButtonReleased>())
					{
						if (mouseReleased->button == sf::Mouse::Button::Left)
						{
							hubOptions.handleMouseReleased({
								static_cast<float>(mouseReleased->position.x),
								static_cast<float>(mouseReleased->position.y)
								});
						}
					}
					if (const auto* mouseMoved = event->getIf<sf::Event::MouseMoved>())
					{
						hubOptions.handleMouseMoved({
							static_cast<float>(mouseMoved->position.x),
							static_cast<float>(mouseMoved->position.y)
							});
					}
				}
				else if (pauseMenu.isOpen())
				{
					if (const auto* keyReleased = event->getIf<sf::Event::KeyReleased>())
					{
						if (keyReleased->code == sf::Keyboard::Key::Escape)
						{
							ArcadeUISounds::playUIClick();
							HandlePauseAction(GamePauseMenu::Action::Resume);
						}
						else
						{
							HandlePauseAction(pauseMenu.handleKeyReleased(keyReleased->code));
						}
					}
					if (const auto* mousePressed = event->getIf<sf::Event::MouseButtonPressed>())
					{
						if (mousePressed->button == sf::Mouse::Button::Left)
						{
							HandlePauseAction(pauseMenu.handleMousePressed({
								static_cast<float>(mousePressed->position.x),
								static_cast<float>(mousePressed->position.y)
								}));
						}
					}
					if (const auto* mouseMoved = event->getIf<sf::Event::MouseMoved>())
					{
						pauseMenu.handleMouseMoved({
							static_cast<float>(mouseMoved->position.x),
							static_cast<float>(mouseMoved->position.y)
							});
					}
				}
				else if (game1VictoryPopupOpen)
				{
					LayoutGame1VictoryPopup();

					if (const auto* mousePressed = event->getIf<sf::Event::MouseButtonPressed>())
					{
						if (mousePressed->button == sf::Mouse::Button::Left)
						{
							const sf::Vector2f mousePosition(
								static_cast<float>(mousePressed->position.x),
								static_cast<float>(mousePressed->position.y));

							if (PointInRect(game1VictoryRetryBounds, mousePosition))
							{
								SelectGame1VictoryButton(0);
								if (!ActivateGame1VictoryButton(0))
									return -1;
							}
							else if (Game1HasNextLevel() &&
								PointInRect(game1VictoryNextBounds, mousePosition))
							{
								SelectGame1VictoryButton(1);
								if (!ActivateGame1VictoryButton(1))
									return -1;
							}
						}
					}

					if (const auto* mouseMoved = event->getIf<sf::Event::MouseMoved>())
					{
						const sf::Vector2f mousePosition(
							static_cast<float>(mouseMoved->position.x),
							static_cast<float>(mouseMoved->position.y));

						if (PointInRect(game1VictoryRetryBounds, mousePosition))
							SelectGame1VictoryButton(0);
						else if (Game1HasNextLevel() &&
							PointInRect(game1VictoryNextBounds, mousePosition))
							SelectGame1VictoryButton(1);
					}
				}
				else if (game1RunFinished)
				{
					// Wait for the End Tile animation to finish before accepting menu input.
				}
				else
				{
					if (const auto* keyReleased = event->getIf<sf::Event::KeyReleased>())
					{
						if (keyReleased->code == sf::Keyboard::Key::Escape)
						{
							if ((!game1Player2Joined && game1Player.isGameOver()) ||
								game1TeamGameOver)
							{
								ArcadeUISounds::playUIClick();
								SetAppState(AppState::GAME1_Menu);
								ArcadeInput::consumePressedState();
							}
							else
							{
								OpenPauseMenu();
							}
						}
						else if (keyReleased->code == sf::Keyboard::Key::Enter)
						{
							if (!game1Player2Joined && game1Player.isGameOver())
							{
								if (!TryRestartGame1CurrentLevel())
									return -1;
							}
							else if (game1TeamGameOver)
							{
								if (!RestartGame1Team())
									return -1;
							}
							else if (!game1Player2Joined &&
								!game1Player1DetectedSource.singlePlayer &&
								game1Player1DetectedSource.source == GAME1_InputSource::Joystick)
							{
								// One player per input: only allow Enter to spawn a
								// keyboard P2 once P1 has explicitly claimed a
								// joystick.  Otherwise P1 is still on (or could
								// still claim) the keyboard, so Enter must not
								// also fire a P2 join (e.g. the Enter that
								// restarts a single-player run would otherwise
								// immediately join P2 as keyboard).
								GAME1_PlayerBinding kb{false, GAME1_InputSource::Keyboard, 0};
								TryJoinGame1Player2(kb);
							}
						}
					}
				}
			}
			else if (appState == AppState::GAME1_Editor)
			{
				if (const auto* keyReleased = event->getIf<sf::Event::KeyReleased>())
				{
					if (keyReleased->code == sf::Keyboard::Key::Escape)
					{
						// Escape first cancels an active selection preview; only
						// leaves the editor when there is nothing to cancel.
						if (!game1Editor.handleEscape())
							SetAppState(AppState::GAME1_Menu);
					}
					else
					{
						game1Editor.handleKeyReleased(keyReleased->code);
					}
				}

				if (const auto* mousePressed = event->getIf<sf::Event::MouseButtonPressed>())
				{
					const sf::Vector2i mousePixelPosition{
						mousePressed->position.x,
						mousePressed->position.y
					};

					game1Editor.handleMousePressed(mousePressed->button, mousePixelPosition);
				}

				if (const auto* mouseReleased = event->getIf<sf::Event::MouseButtonReleased>())
				{
					const sf::Vector2i mousePixelPosition{
						mouseReleased->position.x,
						mouseReleased->position.y
					};

					game1Editor.handleMouseReleased(mouseReleased->button, mousePixelPosition);
				}

				if (const auto* mouseWheelScrolled = event->getIf<sf::Event::MouseWheelScrolled>())
				{
					if (mouseWheelScrolled->wheel == sf::Mouse::Wheel::Vertical)
					{
						game1Editor.handleMouseWheelScrolled(-mouseWheelScrolled->delta);
					}
				}
			}
			else if (appState == AppState::GAME2_Menu)
			{
				if (const auto* keyReleased = event->getIf<sf::Event::KeyReleased>())
				{
					if (keyReleased->code == sf::Keyboard::Key::Escape)
					{
						SetAppState(AppState::Hub);
					}
				}

				if (const auto* mousePressed = event->getIf<sf::Event::MouseButtonPressed>())
				{
					if (mousePressed->button == sf::Mouse::Button::Left)
					{
						const sf::Vector2f mousePosition(
							static_cast<float>(mousePressed->position.x),
							static_cast<float>(mousePressed->position.y)
						);

						switch (game2Menu.handleClick(mousePosition))
						{
						case GAME2_MenuAction::Play:
							game2Game.reset(window.getSize());
							SetAppState(AppState::GAME2_Game);
							break;

						case GAME2_MenuAction::None:
						default:
							break;
						}
					}
				}
			}
			else if (appState == AppState::GAME2_Game)
			{
				if (const auto* keyReleased = event->getIf<sf::Event::KeyReleased>())
				{
					if (keyReleased->code == sf::Keyboard::Key::Escape)
					{
						SetAppState(AppState::GAME2_Menu);
					}
				}

				if (const auto* mousePressed = event->getIf<sf::Event::MouseButtonPressed>())
				{
					if (mousePressed->button == sf::Mouse::Button::Left)
					{
						const sf::Vector2f mousePosition(
							static_cast<float>(mousePressed->position.x),
							static_cast<float>(mousePressed->position.y)
						);

						switch (game2Game.handleClick(mousePosition, window.getSize()))
						{
						case GAME2_GameAction::BackToMenu:
							SetAppState(AppState::GAME2_Menu);
							break;

						case GAME2_GameAction::None:
						default:
							break;
						}
					}
				}
			}
		}

		if (appState == AppState::Hub)
		{
			if (hubOptions.isOpen())
			{
				hubOptions.handleControllerInput();
			}
			else
			{
				if (ArcadeInput::hasControllerActivity())
				{
					hub.notifyUserActivity();
				}

				if (ArcadeInput::isControllerBackPressed())
				{
					ArcadeUISounds::playUIClick();
					hubOptions.open();
					ArcadeInput::consumePressedState();
				}
				else if (ArcadeInput::isControllerMoveLeftPressed())
				{
					hub.navigateLeft();
				}
				else if (ArcadeInput::isControllerMoveRightPressed())
				{
					hub.navigateRight();
				}
				else if (ArcadeInput::isControllerConfirmPressed())
				{
					if (!hub.isTransitioning())
					{
						TryLaunchSelectedHubGame();
						ArcadeInput::consumePressedState();
					}
				}
			}
		}
		else if (appState == AppState::GAME1_BombermanMenu)
		{
			const GAME1_BombermanMenuAction controllerAction = bombermanMenu.handleControllerInput();

			if (controllerAction != GAME1_BombermanMenuAction::None)
			{
				HandleBombermanMenuAction(controllerAction);
				ArcadeInput::consumePressedState();
			}
		}
		else if (appState == AppState::GAME1_BombermanLevelSelect)
		{
			if (ArcadeInput::isControllerCancelPressed() || ArcadeInput::isControllerBackPressed())
			{
				ArcadeUISounds::playUIClick();
				SetAppState(AppState::GAME1_BombermanMenu);
				ArcadeInput::consumePressedState();
			}
			else if (ArcadeInput::isControllerMoveUpPressed())
			{
				bombermanLevelSelect.selectPreviousSlot();
			}
			else if (ArcadeInput::isControllerMoveDownPressed())
			{
				bombermanLevelSelect.selectNextSlot();
			}
			else if (ArcadeInput::isControllerMoveLeftPressed())
			{
				bombermanLevelSelect.selectPreviousPage();
			}
			else if (ArcadeInput::isControllerMoveRightPressed())
			{
				bombermanLevelSelect.selectNextPage();
			}
			else if (ArcadeInput::isControllerConfirmPressed())
			{
				const GAME1_BombermanLevelSelectAction action = bombermanLevelSelect.activateSelectedSlot();

				if (action == GAME1_BombermanLevelSelectAction::SelectedLevel)
				{
					ArcadeUISounds::playUIClick();
					if (!TryStartBombermanLevel(bombermanLevelSelect.getSelectedLevelPath()))
						return -1;
				}

				ArcadeInput::consumePressedState();
			}
		}
		else if (appState == AppState::GAME1_BombermanEditor)
		{
			if (ArcadeInput::isControllerCancelPressed() || ArcadeInput::isControllerBackPressed())
			{
				SetAppState(AppState::GAME1_BombermanMenu);
				ArcadeInput::consumePressedState();
			}
		}
		else if (appState == AppState::GAME1_Bomberman)
		{
			if (hubOptions.isOpen())
			{
				hubOptions.handleControllerInput();
			}
			else if (pauseMenu.isOpen())
			{
				HandlePauseAction(pauseMenu.handleControllerInput());
			}
			else if (ArcadeInput::isControllerBackPressed())
			{
				OpenPauseMenu();
			}
		}
		else if (appState == AppState::GAME1_Menu)
		{
			const GAME1_MenuAction controllerAction = game1Menu.handleControllerInput();

			if (controllerAction != GAME1_MenuAction::None)
			{
				HandleGame1MenuAction(controllerAction);
				ArcadeInput::consumePressedState();
			}
		}
		else if (appState == AppState::GAME1_LevelSelect)
		{
			if (ArcadeInput::isControllerBackPressed())
			{
				ArcadeUISounds::playUIClick();
				SetAppState(AppState::GAME1_Menu);
				ArcadeInput::consumePressedState();
			}
			else if (ArcadeInput::isControllerMoveUpPressed())
			{
				game1LevelSelect.selectPreviousSlot();
			}
			else if (ArcadeInput::isControllerMoveDownPressed())
			{
				game1LevelSelect.selectNextSlot();
			}
			else if (ArcadeInput::isControllerMoveLeftPressed())
			{
				game1LevelSelect.selectPreviousPage();
			}
			else if (ArcadeInput::isControllerMoveRightPressed())
			{
				game1LevelSelect.selectNextPage();
			}
			else if (ArcadeInput::isControllerConfirmPressed())
			{
				const int slotIndex = game1LevelSelect.activateSelectedSlot();

				if (slotIndex >= 0 && !game1LevelSelect.getSelectedLevelPath().empty())
				{
					ArcadeUISounds::playUIClick();
					if (!TryLoadGame1Level(game1LevelSelect.getSelectedLevelPath()))
						return -1;

					ArcadeInput::consumePressedState();
					SetAppState(AppState::GAME1_Game);
				}
				else
				{
					ArcadeInput::consumePressedState();
				}
			}
		}
		else if (appState == AppState::GAME1_Editor)
		{
			if (ArcadeInput::isControllerBackPressed())
			{
				SetAppState(AppState::GAME1_Menu);
				ArcadeInput::consumePressedState();
			}
		}
		else if (appState == AppState::GAME1_Game)
		{
			if (hubOptions.isOpen())
			{
				hubOptions.handleControllerInput();
			}
			else if (pauseMenu.isOpen())
			{
				HandlePauseAction(pauseMenu.handleControllerInput());
			}
			else if (game1VictoryPopupOpen)
			{
				if (!HandleGame1VictoryMenuInput())
					return -1;
			}
			else if (game1RunFinished)
			{
				ArcadeInput::consumePressedState();
			}
			else if (((!game1Player2Joined && game1Player.isGameOver()) ||
				game1TeamGameOver) &&
				ArcadeInput::isControllerBackPressed())
			{
				ArcadeUISounds::playUIClick();
				SetAppState(AppState::GAME1_Menu);
				ArcadeInput::consumePressedState();
			}
			else if (!game1Player2Joined &&
				game1Player.isGameOver() &&
				ArcadeInput::isRestartPressed())
			{
				if (!TryRestartGame1CurrentLevel())
					return -1;
			}
			else if (game1TeamGameOver && ArcadeInput::isRestartPressed())
			{
				if (!RestartGame1Team())
					return -1;
			}
			else if (ArcadeInput::isControllerBackPressed())
			{
				OpenPauseMenu();
			}
			else if (!game1Player2Joined)
			{
				// Auto-detect P1's input source on first movement/action input.
				// Only runs until the source is claimed (singlePlayer stays true until then).
				if (game1Player1DetectedSource.singlePlayer)
				{
					if (ArcadeInput::isKeyboardMoveLeftHeld() || ArcadeInput::isKeyboardMoveRightHeld() ||
						ArcadeInput::isKeyboardPrimaryHeld() || ArcadeInput::isKeyboardSecondaryHeld())
					{
						game1Player1DetectedSource = {false, GAME1_InputSource::Keyboard, 0};
					}
					else
					{
						for (unsigned int i = 0; i < sf::Joystick::Count; ++i)
						{
							if (ArcadeInput::isJoystickConnected(i) &&
								(ArcadeInput::isJoystickMoveLeftHeld(i) || ArcadeInput::isJoystickMoveRightHeld(i) ||
								 ArcadeInput::isJoystickPrimaryHeld(i) || ArcadeInput::isJoystickSecondaryHeld(i)))
							{
								game1Player1DetectedSource = {false, GAME1_InputSource::Joystick, i};
								break;
							}
						}
					}
				}

				// Check any connected joystick START for P2 join.
				// Skip if that joystick is already claimed by P1.
				for (unsigned int i = 0; i < sf::Joystick::Count; ++i)
				{
					if (ArcadeInput::isJoystickStartPressed(i))
					{
						if (!game1Player1DetectedSource.singlePlayer &&
							game1Player1DetectedSource.source == GAME1_InputSource::Joystick &&
							game1Player1DetectedSource.joystickIndex == i)
						{
							break;
						}
						GAME1_PlayerBinding joy{false, GAME1_InputSource::Joystick, i};
						TryJoinGame1Player2(joy);
						break;
					}
				}
			}
		}


		ApplyWindowView(window);
		UpdateMouseCursorVisibility();

		auto DisplayFrame = [&]()
			{
				ApplyWindowView(window);

				const bool crtActive =
					crtShaderLoaded &&
					ArcadeSettings::isCrtEnabled() &&
					ResizeTexture(crtFrameTexture, window.getSize());

				if (crtActive)
				{
					crtFrameTexture.update(window);

					sf::Sprite frameSprite(crtFrameTexture);
					frameSprite.setPosition({ 0.f, 0.f });

					crtShader.setUniform("u_texture", sf::Shader::CurrentTexture);
					crtShader.setUniform("u_time", totalAppTime);
					crtShader.setUniform("u_resolution", sf::Vector2f(
						static_cast<float>(window.getSize().x),
						static_cast<float>(window.getSize().y)
					));

					sf::RenderStates shaderStates;
					shaderStates.shader = &crtShader;

					window.clear(sf::Color::Black);
					window.draw(frameSprite, shaderStates);
				}

				window.display();
			};

		if (appState == AppState::Hub)
		{
			hub.updateClockText();
			hub.updateAnimation(deltaTime);
			hub.updateVisualTheme(totalAppTime);
			hub.layout(window);
			hubOptions.layout(window);
			hubOptions.update(deltaTime);

			window.clear(sf::Color::Black);
			hub.draw(window);
			hubOptions.draw(window);
			DisplayFrame();
		}
		else if (appState == AppState::GAME1_BombermanMenu)
		{
			BombermanAudio::updateMenuMusic(deltaTime);
			bombermanMenu.layout(window);

			window.clear(sf::Color::Black);
			bombermanMenu.draw(window);
			DisplayFrame();
		}
		else if (appState == AppState::GAME1_BombermanLevelSelect)
		{
			BombermanAudio::updateMenuMusic(deltaTime);
			bombermanLevelSelect.layout(window);

			window.clear(sf::Color::Black);
			bombermanLevelSelect.draw(window);
			DisplayFrame();
		}
		else if (appState == AppState::GAME1_BombermanEditor)
		{
			bombermanEditor.layout(window);

			window.clear(sf::Color::Black);
			bombermanEditor.draw(window, sf::Mouse::getPosition(window));
			DisplayFrame();
		}
		else if (appState == AppState::GAME1_Bomberman)
		{
			const bool paused = pauseMenu.isOpen() || hubOptions.isOpen();
			window.setMouseCursorVisible(paused);

			if (!paused)
			{
				bombermanWindow.update(deltaTime, window.getSize());
			}

			bombermanWindow.layout(window);
			pauseMenu.layout(window);
			hubOptions.layout(window);
			hubOptions.update(deltaTime);

			window.clear(sf::Color::Black);
			bombermanWindow.draw(window);
			pauseMenu.draw(window);
			if (hubOptions.isOpen())
			{
				hubOptions.draw(window);
			}
			DisplayFrame();
		}
		else if (appState == AppState::GAME1_Game)
		{
			const bool p2Joined = game1Player2Joined;
			const bool game1OverlayPaused = pauseMenu.isOpen() || hubOptions.isOpen();
			const bool game1Paused = game1OverlayPaused || game1RunFinished || game1VictoryPopupOpen;
			window.setMouseCursorVisible(game1OverlayPaused || game1VictoryPopupOpen);

			auto ReviveGame1PlayerAtCheckpoint = [&](GAME1_Player& player,
				sf::Vector2f checkpointSpawn)
				{
					const sf::Vector2f revivePosition =
						GetGame1SafeRespawnPositionAround(player, checkpointSpawn)
							.value_or(checkpointSpawn);

					player.setSpawnPosition(checkpointSpawn);
					player.reviveWithOneLifeAt(revivePosition);
				};

			auto TryReviveGame1PartnerAtCheckpoint = [&](int triggeringPlayerIndex,
				sf::Vector2f checkpointSpawn)
				{
					if (!p2Joined || game1TeamGameOver || game1RunFinished)
						return;

					const bool p1Out =
						game1Player.isGameOver() && game1Player.getLives() <= 0;
					const bool p2Out =
						game1Player2.isGameOver() && game1Player2.getLives() <= 0;

					if (p1Out && p2Out)
						return;

					if (triggeringPlayerIndex == 0 && game1Player.isActive() && p2Out)
					{
						ReviveGame1PlayerAtCheckpoint(game1Player2, checkpointSpawn);
					}
					else if (triggeringPlayerIndex == 1 && game1Player2.isActive() && p1Out)
					{
						ReviveGame1PlayerAtCheckpoint(game1Player, checkpointSpawn);
					}
				};

			auto TriggerCheckpointsForPlayer = [&](const GAME1_Player& player, int playerIndex)
				{
					if (!player.isActive() || game1TeamGameOver || game1RunFinished)
						return;

					const sf::FloatRect playerBounds = player.getBounds();

					for (int i = 0; i < game1Level.getCheckpointCount(); ++i)
					{
						const sf::FloatRect checkpointBounds = game1Level.getCheckpointBounds(i);

						const bool overlaps =
							playerBounds.position.x < checkpointBounds.position.x + checkpointBounds.size.x &&
							playerBounds.position.x + playerBounds.size.x > checkpointBounds.position.x &&
							playerBounds.position.y < checkpointBounds.position.y + checkpointBounds.size.y &&
							playerBounds.position.y + playerBounds.size.y > checkpointBounds.position.y;

						if (!overlaps)
							continue;

						const bool alreadyTriggered = game1Level.isCheckpointTriggered(i);
						const int order = game1Level.getCheckpointOrderIndex(i);
						const sf::Vector2f cpSpawn =
							game1Level.getCheckpointSpawnPosition(i);

						if (!alreadyTriggered && order <= game1LastCheckpointOrder)
							continue;

						if (!alreadyTriggered)
						{
							game1Level.triggerCheckpoint(i);
							game1LastCheckpointOrder = order;

							game1Player.setSpawnPosition(cpSpawn);
							if (p2Joined)
								game1Player2.setSpawnPosition(cpSpawn);

							GAME1_SurfersQuestAudio::playCheckpoint();
						}

						TryReviveGame1PartnerAtCheckpoint(playerIndex, cpSpawn);
					}
				};

			auto TryCollectPickupsForPlayer = [&](GAME1_Player& player, int playerIndex)
				{
					if (!player.isActive())
						return;

					const sf::FloatRect playerBounds = player.getBounds();

					for (GAME1_Pickup& pickup : game1Pickups)
					{
						if (!pickup.tryCollect(playerBounds))
							continue;

						GAME1_SurfersQuestAudio::playPickupSound();

						const sf::Vector2f pickupPosition = pickup.getPosition();
						const sf::Vector2f popupPosition(
							pickupPosition.x + GAME1_Pickup::DrawSize * 0.5f,
							pickupPosition.y - 8.f);

						AwardGame1Points(
							playerIndex,
							pickup.getPointValue(),
							popupPosition,
							pickup.getType());
					}
				};

			auto UpdateGame1ScorePopups = [&](float frameDeltaTime)
				{
					for (GAME1_FloatingScorePopup& popup : game1ScorePopups)
					{
						popup.age += frameDeltaTime;
						popup.position.y -= 42.f * frameDeltaTime;
					}

					game1ScorePopups.erase(
						std::remove_if(
							game1ScorePopups.begin(),
							game1ScorePopups.end(),
							[](const GAME1_FloatingScorePopup& popup)
							{
								return popup.age >= popup.lifetime;
							}),
						game1ScorePopups.end());
				};

			auto HandleEnemyCollisionForPlayer = [&](GAME1_Enemy& enemy,
				GAME1_Player& player,
				int playerIndex)
				{
					if (!player.isActive())
						return;

					const bool wasAlive = enemy.isAlive();
					const sf::FloatRect enemyBounds = enemy.getBounds();

					if (!enemy.handlePlayerCollision(player))
						return;

					if (wasAlive && !enemy.isAlive())
					{
						const sf::Vector2f popupPosition(
							enemyBounds.position.x + enemyBounds.size.x * 0.5f,
							enemyBounds.position.y - 8.f);

						AwardGame1Points(playerIndex, 200, popupPosition, std::nullopt);
					}
				};

			auto HasGame1RunStartInput = [&]()
				{
					return ArcadeInput::isMoveLeftHeld() ||
						ArcadeInput::isMoveRightHeld() ||
						ArcadeInput::isMoveUpHeld() ||
						ArcadeInput::isMoveDownHeld() ||
						ArcadeInput::isPrimaryHeld() ||
						ArcadeInput::isSecondaryHeld();
				};

			auto MarkGame1PlayerFinished = [&](GAME1_Player& player, int playerIndex)
				{
					if (playerIndex < 0 ||
						playerIndex >= static_cast<int>(game1Scores.size()))
					{
						return;
					}

					GAME1_PlayerScoreState& scoreState =
						game1Scores[static_cast<std::size_t>(playerIndex)];
					scoreState.reachedEnd = true;
					scoreState.forceStatsNA = false;
					scoreState.completionTimeSeconds = game1RunTimerSeconds;
					player.setLevelFinished(true);
				};

			auto MarkGame1PlayerFinishFailed = [&](GAME1_Player& player, int playerIndex)
				{
					if (playerIndex < 0 ||
						playerIndex >= static_cast<int>(game1Scores.size()))
					{
						return;
					}

					GAME1_PlayerScoreState& scoreState =
						game1Scores[static_cast<std::size_t>(playerIndex)];
					if (scoreState.reachedEnd)
						return;

					scoreState.forceStatsNA = true;
					scoreState.completionTimeSeconds = 0.f;
					player.setLevelFinishFailed(true);
				};

			auto FinishGame1Run = [&](int endTileIndex)
				{
					if (game1RunFinished)
						return;

					game1RunFinished = true;
					game1RunTimerStarted = false;
					game1FinalRunTimerSeconds = game1RunTimerSeconds;
					game1VictoryPopupOpen = false;
					game1TriggeredEndTileIndex = endTileIndex;
					game1VictorySelectedButton = 0;
					game1EndCountdownActive = false;
					game1EndCountdownSeconds = 0.f;
					ArcadeInput::consumePressedState();
				};

			// Returns the index of an End Tile the player's bounds overlap, or
			// -1 if none. Unlike the old loop this does NOT skip already-
			// triggered tiles, so in co-op the second player is still detected
			// reaching the same tile the first player triggered.
			auto FindOverlappingEndTile = [&](const GAME1_Player& player) -> int
				{
					const sf::FloatRect playerBounds = player.getBounds();

					for (int i = 0; i < game1Level.getEndTileCount(); ++i)
					{
						if (RectsOverlap(playerBounds, game1Level.getEndTileBounds(i)))
							return i;
					}

					return -1;
				};

			auto TryTriggerEndTileForPlayer = [&](GAME1_Player& player, int playerIndex)
				{
					if (game1RunFinished || !player.isActive())
						return;

					if (playerIndex >= 0 &&
						playerIndex < static_cast<int>(game1Scores.size()) &&
						game1Scores[static_cast<std::size_t>(playerIndex)].reachedEnd)
						return;

					const int tileIndex = FindOverlappingEndTile(player);
					if (tileIndex < 0)
						return;

					if (!game1Level.isEndTileTriggered(tileIndex))
						game1Level.triggerEndTile(tileIndex);

					MarkGame1PlayerFinished(player, playerIndex);

					if (!p2Joined)
					{
						// Single player: finish the level immediately.
						FinishGame1Run(tileIndex);
						return;
					}

					if (!game1EndCountdownActive)
					{
						// First player to reach the End Tile starts the countdown;
						// the run keeps running so the other player can still arrive.
						game1EndCountdownActive = true;
						game1EndCountdownSeconds = kGame1EndCountdownDuration;
						game1EndCountdownFirstFinisher = playerIndex;
						game1TriggeredEndTileIndex = tileIndex;
					}
					else
					{
						// Second player arrived before the countdown expired.
						FinishGame1Run(game1TriggeredEndTileIndex);
					}
				};

			if (!game1OverlayPaused)
				game1Level.updateGoalTiles(deltaTime);

			if (game1RunFinished &&
				!game1VictoryPopupOpen &&
				game1Level.hasEndTileCompletedAnimationCycles(game1TriggeredEndTileIndex, 2))
			{
				game1VictoryPopupOpen = true;
				game1VictorySelectedButton = 0;
				ArcadeInput::consumePressedState();
			}

			if (!game1Paused)
			{
				if (p2Joined && !game1TeamGameOver)
				{
					GAME1_SurfersQuestAudio::playGameplay();
				}

				if (!game1RunTimerStarted &&
					!game1RunFinished &&
					!game1TeamGameOver &&
					(game1Player.isActive() || (p2Joined && game1Player2.isActive())) &&
					HasGame1RunStartInput())
				{
					game1RunTimerStarted = true;
				}

				if (game1RunTimerStarted && !game1RunFinished)
					game1RunTimerSeconds += deltaTime;

				// Tick the co-op end-tile countdown. The run timer keeps running
				// so a late-arriving second player still gets an accurate time.
				if (p2Joined && game1EndCountdownActive && !game1RunFinished)
				{
					game1EndCountdownSeconds -= deltaTime;
					if (game1EndCountdownSeconds <= 0.f)
					{
						game1EndCountdownSeconds = 0.f;

						MarkGame1PlayerFinishFailed(game1Player, 0);
						MarkGame1PlayerFinishFailed(game1Player2, 1);

						FinishGame1Run(game1TriggeredEndTileIndex);
					}
				}

				game1Level.updateTraps(deltaTime);

				if (p2Joined && !game1TeamGameOver)
				{
					MoveGame1RespawningPlayerToward(game1Player, game1Player2, deltaTime);
					MoveGame1RespawningPlayerToward(game1Player2, game1Player, deltaTime);
				}

				// Feed the SpikeHead traps the active players' bounds so each can
				// detect/target the closest one before the players move/collide.
				std::vector<sf::FloatRect> game1SpikeHeadTargets;
				if (game1Player.isActive())
					game1SpikeHeadTargets.push_back(game1Player.getDamageBounds());
				if (p2Joined && game1Player2.isActive())
					game1SpikeHeadTargets.push_back(game1Player2.getDamageBounds());
				game1Level.updateSpikeHeads(deltaTime, game1SpikeHeadTargets);
				game1Level.updateSaws(deltaTime);

				const bool game1PlayerWasRespawning = game1Player.isRespawning();
				const bool game1PlayerWasGameOver = game1Player.isGameOver();
				game1Player.update(deltaTime, game1Level);
				if (!p2Joined && !game1PlayerWasRespawning && game1Player.isRespawning())
				{
					ResetGame1RunState();
				}
				if (!p2Joined && !game1PlayerWasGameOver && game1Player.isGameOver())
				{
					ResetGame1CheckpointProgress();
					ResetGame1RunState();
				}
				TriggerCheckpointsForPlayer(game1Player, 0);
				TryCollectPickupsForPlayer(game1Player, 0);
				TryTriggerEndTileForPlayer(game1Player, 0);

				if (p2Joined && !game1Player2.isGameOver())
				{
					game1Player2.update(deltaTime, game1Level);
					TriggerCheckpointsForPlayer(game1Player2, 1);
					TryCollectPickupsForPlayer(game1Player2, 1);
					TryTriggerEndTileForPlayer(game1Player2, 1);
				}

				game1Level.updateCheckpoints(deltaTime);

				for (GAME1_Pickup& pickup : game1Pickups)
				{
					pickup.update(deltaTime, game1PickupAssets);
				}

				game1Pickups.erase(
					std::remove_if(
						game1Pickups.begin(),
						game1Pickups.end(),
						[&](const GAME1_Pickup& pickup)
						{
							return pickup.isFinished(game1PickupAssets);
						}),
					game1Pickups.end());

				UpdateGame1ScorePopups(deltaTime);

				if (!game1RunFinished)
				{
					game1Level.killEnemiesTouchedBySpikeHeads(game1Enemies);

					for (GAME1_Enemy& enemy : game1Enemies)
					{
						enemy.update(deltaTime, game1Level, game1Player,
							p2Joined ? &game1Player2 : nullptr);
					}

					game1Level.killEnemiesTouchedBySpikeHeads(game1Enemies);

					for (GAME1_Enemy& enemy : game1Enemies)
					{
						HandleEnemyCollisionForPlayer(enemy, game1Player, 0);

						if (p2Joined)
							HandleEnemyCollisionForPlayer(enemy, game1Player2, 1);
					}

					game1Enemies.erase(
						std::remove_if(
							game1Enemies.begin(),
							game1Enemies.end(),
							[](const GAME1_Enemy& enemy)
							{
								return !enemy.isActive();
							}),
						game1Enemies.end());
				}
			}

			sf::View worldView = window.getDefaultView();

			const sf::Vector2f viewSize = worldView.getSize();
			const float halfViewWidth = viewSize.x * 0.5f;
			const float halfViewHeight = viewSize.y * 0.5f;

			auto PlayerCenterX = [](const GAME1_Player& player)
				{
					const sf::FloatRect b = player.getBounds();
					return b.position.x + b.size.x * 0.5f;
				};

			auto PlayerCenterY = [](const GAME1_Player& player)
				{
					const sf::FloatRect b = player.getBounds();
					return b.position.y + b.size.y * 0.5f;
				};

			const bool p1Active = game1Player.isActive();
			const bool p2Active = p2Joined && game1Player2.isActive();
			const bool p1OnField = !game1Player.isGameOver();
			const bool p2OnField = p2Joined && !game1Player2.isGameOver();

			// Camera follows the furthest-ahead living player.
			const GAME1_Player* leadingPlayer = nullptr;

			if (p1Active)
			{
				leadingPlayer = &game1Player;
			}

			if (p2Active &&
				(leadingPlayer == nullptr ||
					PlayerCenterX(game1Player2) > PlayerCenterX(*leadingPlayer)))
			{
				leadingPlayer = &game1Player2;
			}

			if (leadingPlayer == nullptr)
			{
				if (p1OnField)
					leadingPlayer = &game1Player;

				if (p2OnField &&
					(leadingPlayer == nullptr ||
						PlayerCenterX(game1Player2) > PlayerCenterX(*leadingPlayer)))
				{
					leadingPlayer = &game1Player2;
				}
			}

			if (leadingPlayer == nullptr)
				leadingPlayer = &game1Player;

			const float playerCenterX = PlayerCenterX(*leadingPlayer);
			const float playerCenterY = PlayerCenterY(*leadingPlayer);

			// ---- Horizontal follow (unchanged) ----
			const float minViewCenterX = halfViewWidth;
			const float maxViewCenterX = std::max(
				halfViewWidth,
				game1Level.getPixelWidth() - halfViewWidth
			);

			const float targetViewCenterX = std::clamp(
				playerCenterX,
				minViewCenterX,
				maxViewCenterX
			);

			// ---- Vertical follow: margin-pinning camera with asymmetric smoothing ----
			const float tileSize = static_cast<float>(GAME1_Level::TileSize);

			// Screen-space scroll margins: top 3 rows (ease up slowly when crossed),
			// bottom 2 rows (ease down quickly when crossed).
			const float topMargin = 3.f * tileSize;
			const float bottomMargin = 2.f * tileSize;

			// Bottom clamp: camera Y can't show below the level. There's no top
			// clamp — the camera is allowed to pan above the level (into sky) so
			// vertical follow works on short maps when the player jumps onto a
			// high platform.
			const float maxViewCenterY = std::max(
				halfViewHeight,
				game1Level.getPixelHeight() - halfViewHeight
			);

			// On level load, snap to frame the player without showing below the
			// level OR above it. For maps that fit the screen this snaps to
			// halfViewHeight (identical to the original start framing).
			if (game1CameraNeedsSnap)
			{
				const float restScreenY = 0.5f * (topMargin + (viewSize.y - bottomMargin));
				game1CameraCenterY = std::clamp(
					playerCenterY + halfViewHeight - restScreenY,
					halfViewHeight,
					maxViewCenterY
				);
				game1CameraNeedsSnap = false;
			}

			// Margin-pinning target: when the player crosses a margin, the target
			// is the camera Y that places the player exactly on the margin edge.
			// At the boundary the target equals the current camera Y, so the
			// transition is continuous and the camera doesn't jitter.
			const float playerScreenY = playerCenterY - game1CameraCenterY + halfViewHeight;

			float targetViewCenterY = game1CameraCenterY; // Dead zone: hold.
			if (playerScreenY < topMargin)
			{
				targetViewCenterY = playerCenterY + halfViewHeight - topMargin;
			}
			else if (playerScreenY > viewSize.y - bottomMargin)
			{
				targetViewCenterY = playerCenterY + halfViewHeight - (viewSize.y - bottomMargin);
			}

			// Clamp target on the bottom only (sky above the level is allowed).
			targetViewCenterY = std::min(targetViewCenterY, maxViewCenterY);

			// Asymmetric, frame-rate-independent smoothing:
			// moving up (toward Y=0) = slow ease, moving down = fast ease.
			const float upEaseRate = 3.0f;      // Time constant ~ 0.33s
			const float downEaseRate = 12.0f;   // Time constant ~ 0.083s
			const bool cameraMovingUp = targetViewCenterY < game1CameraCenterY;
			const float easeRate = cameraMovingUp ? upEaseRate : downEaseRate;
			const float easeFactor = 1.f - std::exp(-easeRate * deltaTime);

			if (!game1Paused)
			{
				game1CameraCenterY += (targetViewCenterY - game1CameraCenterY) * easeFactor;
			}
			game1CameraCenterY = std::min(game1CameraCenterY, maxViewCenterY);

			// Off-screen kill: the trailing player has fallen too far behind
			// the camera.  Only the trailing player is penalised, and they
			// respawn near the leading player.
			if (!game1Paused && p2Joined && p1Active && p2Active && !game1TeamGameOver)
			{
				const float cameraLeftEdge = targetViewCenterX - halfViewWidth;
				const float killLine = cameraLeftEdge - kGame1CoopOffScreenKillDistance;

				auto MaybeKillTrailing = [&](GAME1_Player& trailing, GAME1_Player& leading)
					{
						if (trailing.isGameOver() || trailing.isRespawning())
							return;

						if (!leading.isActive())
							return;

						if (PlayerCenterX(trailing) >= killLine)
							return;

						if (PlayerCenterX(trailing) >= PlayerCenterX(leading))
							return;

						const std::optional<sf::Vector2f> safeRespawnNear =
							GetGame1SafeRespawnPositionNear(trailing, leading);
						const sf::Vector2f respawnNear = safeRespawnNear.value_or(
							GetGame1CoopOffsetPosition(
								leading.getPosition(),
								-kGame1CoopRespawnOffsetX));

						trailing.setNextRespawnPosition(respawnNear);
						trailing.forceDeath();
					};

				if (leadingPlayer == &game1Player)
				{
					MaybeKillTrailing(game1Player2, game1Player);
				}
				else if (leadingPlayer == &game1Player2)
				{
					MaybeKillTrailing(game1Player, game1Player2);
				}
			}

			// Team game-over detection.
			if (!game1Paused && p2Joined && !game1TeamGameOver)
			{
				if (game1Player.isGameOver() && game1Player2.isGameOver())
				{
					game1TeamGameOver = true;
					ResetGame1CheckpointProgress();
					ResetGame1RunState();
					GAME1_SurfersQuestAudio::playDeath();
				}
			}

			worldView.setCenter({
				targetViewCenterX,
				game1CameraCenterY
			});

			window.clear(sf::Color::Black);
			game1Level.drawBackground(window, worldView.getCenter());

			window.setView(worldView);
			game1Level.draw(window);

			for (const GAME1_Pickup& pickup : game1Pickups)
			{
				pickup.draw(window, game1PickupAssets);
			}

			for (const GAME1_Enemy& enemy : game1Enemies)
			{
				enemy.draw(window);
			}

			// In co-op, hide a personally game-over player until the team
			// itself is game over (then the OUT OF LIVES UI applies to both).
			if (!p2Joined || !game1Player.isGameOver() || game1TeamGameOver)
			{
				game1Player.draw(window);
			}

			if (p2Joined && (!game1Player2.isGameOver() || game1TeamGameOver))
			{
				game1Player2.draw(window);
			}

			for (const GAME1_FloatingScorePopup& popup : game1ScorePopups)
			{
				const int flickerIndex = static_cast<int>(popup.age * 34.f);
				if (flickerIndex % 2 != 0)
					continue;

				sf::Text popupText(game1UiFont);
				popupText.setString("+" + std::to_string(popup.points));
				popupText.setCharacterSize(24);
				popupText.setFillColor(sf::Color(255, 245, 0));
				popupText.setOutlineColor(sf::Color::Black);
				popupText.setOutlineThickness(2.f);

				const sf::FloatRect popupBounds = popupText.getLocalBounds();
				popupText.setPosition({
					popup.position.x - popupBounds.size.x * 0.5f - popupBounds.position.x,
					popup.position.y - popupBounds.size.y * 0.5f - popupBounds.position.y
					});

				window.draw(popupText);
			}

			ApplyWindowView(window);

			auto DrawGame1TextureFitted = [&](const sf::Texture& texture, const sf::FloatRect& bounds)
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
					window.draw(sprite);
				};

			auto DrawGame1FruitIconList = [&](const std::vector<GAME1_FruitType>& fruits,
				sf::Vector2f topLeft,
				float iconSize,
				float iconGap)
				{
					for (std::size_t i = 0; i < fruits.size(); ++i)
					{
						const int col = static_cast<int>(i % 4);
						const int row = static_cast<int>(i / 4);

						const sf::FloatRect iconBounds(
							{
								topLeft.x + static_cast<float>(col) * (iconSize + iconGap),
								topLeft.y + static_cast<float>(row) * (iconSize + iconGap)
							},
							{ iconSize, iconSize });

						const GAME1_FruitType fruitType = fruits[i];
						const sf::Texture* iconTexture = game1PickupAssets.getIconTexture(fruitType);

						if (iconTexture != nullptr)
						{
							DrawGame1TextureFitted(*iconTexture, iconBounds);
						}
						else
						{
							sf::RectangleShape fallback;
							fallback.setPosition(iconBounds.position);
							fallback.setSize(iconBounds.size);
							fallback.setFillColor(GAME1_GetFruitFallbackColor(fruitType));
							fallback.setOutlineColor(sf::Color::White);
							fallback.setOutlineThickness(1.f);
							window.draw(fallback);
						}
					}
				};

			auto DrawGame1FruitIcons = [&](const GAME1_PlayerScoreState& scoreState,
				sf::Vector2f topLeft)
				{
					DrawGame1FruitIconList(scoreState.collectedFruits, topLeft, 66.f, 6.f);
				};

			auto DrawGame1ScoreHud = [&](const GAME1_PlayerScoreState& scoreState,
				sf::Vector2f topLeft)
				{
					sf::Text scoreText(game1UiFont);
					scoreText.setString("Score: " + std::to_string(scoreState.score));
					scoreText.setCharacterSize(20);
					scoreText.setFillColor(sf::Color::White);
					scoreText.setOutlineColor(sf::Color::Black);
					scoreText.setOutlineThickness(2.f);
					scoreText.setPosition(topLeft);
					window.draw(scoreText);

					DrawGame1FruitIcons(scoreState, { topLeft.x, topLeft.y + 28.f });
				};

			auto DrawGame1TimerHud = [&]()
				{
					sf::Text timerText(game1UiFont);
					timerText.setString(FormatGame1RunTime(game1RunFinished
						? game1FinalRunTimerSeconds
						: game1RunTimerSeconds));
					timerText.setCharacterSize(28);
					timerText.setFillColor(sf::Color::White);
					timerText.setOutlineColor(sf::Color::Black);
					timerText.setOutlineThickness(2.f);

					const sf::FloatRect bounds = timerText.getLocalBounds();
					timerText.setPosition({
						(static_cast<float>(window.getSize().x) - bounds.size.x) * 0.5f - bounds.position.x,
						14.f - bounds.position.y
						});
					window.draw(timerText);
				};

			auto DrawGame1VictoryPopup = [&]()
				{
					if (!game1VictoryPopupOpen)
						return;

					const sf::FloatRect panelBounds = LayoutGame1VictoryPopup();
					const bool hasNextLevel = Game1HasNextLevel();

					sf::RectangleShape dim;
					dim.setPosition({ 0.f, 0.f });
					dim.setSize({
						static_cast<float>(window.getSize().x),
						static_cast<float>(window.getSize().y)
						});
					dim.setFillColor(sf::Color(0, 0, 0, 165));
					window.draw(dim);

					sf::RectangleShape panel;
					panel.setPosition(panelBounds.position);
					panel.setSize(panelBounds.size);
					panel.setFillColor(sf::Color(20, 22, 32, 245));
					panel.setOutlineColor(sf::Color(230, 230, 230));
					panel.setOutlineThickness(3.f);
					window.draw(panel);

					auto DrawCenteredText = [&](const std::string& value,
						unsigned int size,
						float y,
						sf::Color fill)
						{
							sf::Text text(game1UiFont);
							text.setString(value);
							text.setCharacterSize(size);
							text.setFillColor(fill);
							text.setOutlineColor(sf::Color::Black);
							text.setOutlineThickness(2.f);

							const sf::FloatRect bounds = text.getLocalBounds();
							text.setPosition({
								panelBounds.position.x + (panelBounds.size.x - bounds.size.x) * 0.5f - bounds.position.x,
								y - bounds.position.y
								});
							window.draw(text);
						};

					DrawCenteredText("VICTORY!", 46, panelBounds.position.y + 34.f, sf::Color(80, 235, 120));

					if (!game1Player2Joined)
					{
						// Single-player: combined score / fruits / time.
						{
							sf::Text label(game1UiFont);
							label.setString("Score:");
							label.setCharacterSize(25);
							label.setFillColor(sf::Color::White);
							label.setOutlineColor(sf::Color::Black);
							label.setOutlineThickness(2.f);

							sf::Text value(game1UiFont);
							value.setString(std::to_string(game1RunScore));
							value.setCharacterSize(25);
							value.setFillColor(GetGame1RainbowColor(totalAppTime));
							value.setOutlineColor(sf::Color::Black);
							value.setOutlineThickness(2.f);

							const sf::FloatRect labelBounds = label.getLocalBounds();
							const sf::FloatRect valueBounds = value.getLocalBounds();
							const float gap = 10.f;
							const float totalWidth = labelBounds.size.x + gap + valueBounds.size.x;
							const float startX = panelBounds.position.x + (panelBounds.size.x - totalWidth) * 0.5f;
							const float y = panelBounds.position.y + 104.f;

							label.setPosition({ startX - labelBounds.position.x, y - labelBounds.position.y });
							value.setPosition({
								startX + labelBounds.size.x + gap - valueBounds.position.x,
								y - valueBounds.position.y
								});
							window.draw(label);
							window.draw(value);
						}

						DrawCenteredText("Collected:", 23, panelBounds.position.y + 146.f, sf::Color::White);

						const float iconSize = 50.f;
						const float iconGap = 8.f;
						const int collectedRows = game1RunCollectedFruits.empty()
							? 1
							: static_cast<int>((game1RunCollectedFruits.size() + 3) / 4);
						const float iconsBlockWidth = 4.f * iconSize + 3.f * iconGap;
						const float iconsTop = panelBounds.position.y + 184.f;
						const float iconsLeft = panelBounds.position.x + (panelBounds.size.x - iconsBlockWidth) * 0.5f;

						if (game1RunCollectedFruits.empty())
						{
							DrawCenteredText("None", 21, iconsTop + 4.f, sf::Color(210, 210, 210));
						}
						else
						{
							DrawGame1FruitIconList(
								game1RunCollectedFruits,
								{ iconsLeft, iconsTop },
								iconSize,
								iconGap);
						}

						const float iconsHeight =
							static_cast<float>(collectedRows) * iconSize +
							static_cast<float>(std::max(0, collectedRows - 1)) * iconGap;
						const float timeY = std::min(
							iconsTop + iconsHeight + 26.f,
							game1VictoryRetryBounds.position.y - 44.f);
						DrawCenteredText(
							"Time: " + FormatGame1RunTime(game1FinalRunTimerSeconds),
							24,
							timeY,
							sf::Color::White);
					}
					else
					{
						// Multiplayer: split the stats area in half, one column
						// per player, each showing that player's own stats.
						const float columnCenters[2] = {
							panelBounds.position.x + panelBounds.size.x * 0.25f,
							panelBounds.position.x + panelBounds.size.x * 0.75f
						};

						{
							const float dividerTop = panelBounds.position.y + 80.f;
							const float dividerBottom = game1VictoryRetryBounds.position.y - 16.f;

							sf::RectangleShape divider;
							divider.setSize({ 2.f, std::max(0.f, dividerBottom - dividerTop) });
							divider.setPosition({
								panelBounds.position.x + panelBounds.size.x * 0.5f - 1.f,
								dividerTop
								});
							divider.setFillColor(sf::Color(90, 95, 115, 220));
							window.draw(divider);
						}

						auto DrawColumnCentered = [&](float centerX,
							const std::string& value,
							unsigned int size,
							float y,
							sf::Color fill)
							{
								sf::Text text(game1UiFont);
								text.setString(value);
								text.setCharacterSize(size);
								text.setFillColor(fill);
								text.setOutlineColor(sf::Color::Black);
								text.setOutlineThickness(2.f);

								const sf::FloatRect b = text.getLocalBounds();
								text.setPosition({
									centerX - b.size.x * 0.5f - b.position.x,
									y - b.position.y
									});
								window.draw(text);
							};

						for (int playerIndex = 0; playerIndex < 2; ++playerIndex)
						{
							const GAME1_PlayerScoreState& stats =
								game1Scores[static_cast<std::size_t>(playerIndex)];
							const float cx = columnCenters[playerIndex];

							float y = panelBounds.position.y + 92.f;
							DrawColumnCentered(cx, playerIndex == 0 ? "P1:" : "P2:",
								28, y, sf::Color(255, 224, 60));
							y += 42.f;

							DrawColumnCentered(cx, "Score", 18, y, sf::Color::White);
							y += 24.f;
							DrawColumnCentered(cx,
								stats.forceStatsNA ? "N/A" : std::to_string(stats.score),
								24, y, stats.forceStatsNA ? sf::Color(210, 210, 210) : GetGame1RainbowColor(totalAppTime));
							y += 40.f;

							DrawColumnCentered(cx, "Collected:", 18, y, sf::Color::White);
							y += 28.f;

							const float mpIconSize = 36.f;
							const float mpIconGap = 6.f;
							if (stats.forceStatsNA)
							{
								DrawColumnCentered(cx, "N/A", 18, y + 4.f, sf::Color(210, 210, 210));
							}
							else if (stats.collectedFruits.empty())
							{
								DrawColumnCentered(cx, "None", 18, y + 4.f, sf::Color(210, 210, 210));
							}
							else
							{
								const float blockWidth = 4.f * mpIconSize + 3.f * mpIconGap;
								DrawGame1FruitIconList(
									stats.collectedFruits,
									{ cx - blockWidth * 0.5f, y },
									mpIconSize,
									mpIconGap);
							}

							const int rows = (stats.forceStatsNA || stats.collectedFruits.empty())
								? 1
								: static_cast<int>((stats.collectedFruits.size() + 3) / 4);
							const float iconsHeight =
								static_cast<float>(rows) * mpIconSize +
								static_cast<float>(std::max(0, rows - 1)) * mpIconGap;
							const float timeY = std::min(
								y + iconsHeight + 22.f,
								game1VictoryRetryBounds.position.y - 38.f);
							DrawColumnCentered(cx,
								stats.forceStatsNA ? "Time: N/A" : "Time: " + FormatGame1RunTime(stats.completionTimeSeconds),
								18, timeY, sf::Color::White);
						}
					}

					auto DrawVictoryButton = [&](const sf::FloatRect& bounds,
						const std::string& label,
						sf::Color baseColor,
						bool selected,
						bool enabled)
						{
							const sf::Color disabledColor(
								static_cast<std::uint8_t>(std::max(25, static_cast<int>(baseColor.r) / 3)),
								static_cast<std::uint8_t>(std::max(45, static_cast<int>(baseColor.g) / 2)),
								static_cast<std::uint8_t>(std::max(35, static_cast<int>(baseColor.b) / 2)),
								210);

							sf::RectangleShape box;
							box.setPosition(bounds.position);
							box.setSize(bounds.size);
							box.setFillColor(enabled ? baseColor : disabledColor);
							box.setOutlineColor(enabled ? sf::Color(230, 230, 230) : sf::Color(120, 145, 125));
							box.setOutlineThickness(2.f);
							window.draw(box);

							sf::Text text(game1UiFont);
							text.setString(label);
							text.setCharacterSize(27);
							text.setFillColor(enabled ? sf::Color::White : sf::Color(155, 180, 160));
							text.setOutlineColor(sf::Color::Black);
							text.setOutlineThickness(2.f);

							const sf::FloatRect textBounds = text.getLocalBounds();
							text.setPosition({
								bounds.position.x + (bounds.size.x - textBounds.size.x) * 0.5f - textBounds.position.x,
								bounds.position.y + (bounds.size.y - textBounds.size.y) * 0.5f - textBounds.position.y
								});
							window.draw(text);

							if (!selected)
								return;

							const float pulse = (std::sin(totalAppTime * 4.6f) + 1.f) * 0.5f;
							const float offset = 5.f + pulse * 8.f;
							const float length = 25.f;
							const float thickness = 4.f;
							const sf::Color cornerColor(255, 224, 82);

							auto DrawCornerSegment = [&](sf::Vector2f position, sf::Vector2f size)
								{
									sf::RectangleShape segment;
									segment.setPosition(position);
									segment.setSize(size);
									segment.setFillColor(cornerColor);
									window.draw(segment);
								};

							const float left = bounds.position.x - offset;
							const float top = bounds.position.y - offset;
							const float right = bounds.position.x + bounds.size.x + offset;
							const float bottom = bounds.position.y + bounds.size.y + offset;

							DrawCornerSegment({ left, top }, { length, thickness });
							DrawCornerSegment({ left, top }, { thickness, length });
							DrawCornerSegment({ right - length, top }, { length, thickness });
							DrawCornerSegment({ right - thickness, top }, { thickness, length });
							DrawCornerSegment({ left, bottom - thickness }, { length, thickness });
							DrawCornerSegment({ left, bottom - length }, { thickness, length });
							DrawCornerSegment({ right - length, bottom - thickness }, { length, thickness });
							DrawCornerSegment({ right - thickness, bottom - length }, { thickness, length });
						};

					DrawVictoryButton(
						game1VictoryRetryBounds,
						"Retry",
						sf::Color(210, 105, 35, 235),
						game1VictorySelectedButton == 0,
						true);

					DrawVictoryButton(
						game1VictoryNextBounds,
						"Next",
						sf::Color(35, 170, 75, 245),
						game1VictorySelectedButton == 1,
						hasNextLevel);
				};

			DrawGame1TimerHud();

			if (!p2Joined)
			{
				DrawGame1ScoreHud(game1Scores[0], { 18.f, 86.f });

				if (game1Player.isRespawning())
				{
					respawnText.setCharacterSize(34);
					respawnText.setString("Respawning player in: " + std::to_string(game1Player.getRespawnCountdown()));

					const sf::FloatRect textBounds = respawnText.getLocalBounds();
					respawnText.setPosition({
						(static_cast<float>(window.getSize().x) - textBounds.size.x) * 0.5f - textBounds.position.x,
						80.f - textBounds.position.y
						});

					window.draw(respawnText);
				}
			}
			else
			{
				// Dual-player HUD.
				const float windowWidth = static_cast<float>(window.getSize().x);

				auto DrawPlayerStats = [&](const std::string& title,
					int health,
					int maxHealth,
					int lives,
					int maxLives,
					const GAME1_PlayerScoreState& scoreState,
					sf::Vector2f topLeft)
					{
						sf::Text titleText(game1UiFont);
						titleText.setString(title);
						titleText.setCharacterSize(20);
						titleText.setFillColor(sf::Color::White);
						titleText.setOutlineColor(sf::Color::Black);
						titleText.setOutlineThickness(2.f);
						titleText.setPosition(topLeft);
						window.draw(titleText);

						sf::Text healthText(game1UiFont);
						healthText.setString("Health: " + std::to_string(health) + " / " + std::to_string(maxHealth));
						healthText.setCharacterSize(20);
						healthText.setFillColor(sf::Color::White);
						healthText.setOutlineColor(sf::Color::Black);
						healthText.setOutlineThickness(2.f);
						healthText.setPosition({ topLeft.x, topLeft.y + 24.f });
						window.draw(healthText);

						sf::Text livesText(game1UiFont);
						livesText.setString("Lives: " + std::to_string(lives) + " / " + std::to_string(maxLives));
						livesText.setCharacterSize(20);
						livesText.setFillColor(sf::Color::White);
						livesText.setOutlineColor(sf::Color::Black);
						livesText.setOutlineThickness(2.f);
						livesText.setPosition({ topLeft.x, topLeft.y + 48.f });
						window.draw(livesText);

						DrawGame1ScoreHud(scoreState, { topLeft.x, topLeft.y + 72.f });
					};

				DrawPlayerStats("Player 1",
					game1Player.getHealth(), game1Player.getMaxHealth(),
					game1Player.getLives(), game1Player.getMaxLives(),
					game1Scores[0],
					{ 18.f, 14.f });

				DrawPlayerStats("Player 2",
					game1Player2.getHealth(), game1Player2.getMaxHealth(),
					game1Player2.getLives(), game1Player2.getMaxLives(),
					game1Scores[1],
					{ windowWidth - 380.f, 14.f });

				if (game1Player.isRespawning() && !game1Player.isGameOver())
				{
					respawnText.setCharacterSize(22);
					respawnText.setString("P1 respawn: " + std::to_string(game1Player.getRespawnCountdown()));
					respawnText.setPosition({ 18.f, 186.f });
					window.draw(respawnText);
				}

				if (game1Player2.isRespawning() && !game1Player2.isGameOver())
				{
					respawnText.setCharacterSize(22);
					respawnText.setString("P2 respawn: " + std::to_string(game1Player2.getRespawnCountdown()));
					respawnText.setPosition({ windowWidth - 380.f, 186.f });
					window.draw(respawnText);
				}

				if (game1TeamGameOver)
				{
					const float windowHeight = static_cast<float>(window.getSize().y);
					const sf::FloatRect popupRect(
						{ windowWidth * 0.5f - 280.f, windowHeight * 0.5f - 100.f },
						{ 560.f, 200.f });

					sf::RectangleShape popupBox;
					popupBox.setPosition(popupRect.position);
					popupBox.setSize(popupRect.size);
					popupBox.setFillColor(sf::Color(130, 45, 45, 230));
					popupBox.setOutlineColor(sf::Color::White);
					popupBox.setOutlineThickness(3.f);
					window.draw(popupBox);

					auto DrawCentered = [&](const std::string& s, unsigned int size,
						sf::Vector2f pos, sf::Color fill)
						{
							sf::Text text(game1UiFont);
							text.setString(s);
							text.setCharacterSize(size);
							text.setFillColor(fill);
							text.setOutlineColor(sf::Color::Black);
							text.setOutlineThickness(2.f);

							const sf::FloatRect b = text.getLocalBounds();
							text.setPosition({
								pos.x - b.size.x * 0.5f - b.position.x,
								pos.y - b.size.y * 0.5f - b.position.y
								});
							window.draw(text);
						};

					const float centerX = popupRect.position.x + popupRect.size.x * 0.5f;
					DrawCentered("GAME OVER", 34, { centerX, popupRect.position.y + 38.f }, sf::Color::White);
					DrawCentered("OUT OF LIVES!", 20, { centerX, popupRect.position.y + 78.f }, sf::Color(230, 230, 230));
					DrawCentered("ENTER/START - RESTART", 20, { centerX, popupRect.position.y + 126.f }, sf::Color(255, 230, 120));
					DrawCentered("ESC/SELECT - BACK TO THE MENU", 20, { centerX, popupRect.position.y + 162.f }, sf::Color(230, 230, 230));
				}

				// Restore single-player default size for the next respawn render.
				respawnText.setCharacterSize(34);
			}

			// Co-op end-tile countdown number. Screen-space UI layer, drawn above
			// gameplay so the waiting player can see how long they have left.
			if (p2Joined && game1EndCountdownActive && !game1VictoryPopupOpen)
			{
				// Flicker visible/invisible like the stomp score popups.
				const int flickerIndex = static_cast<int>(totalAppTime * 34.f);
				if (flickerIndex % 2 == 0)
				{
					const float secs = game1EndCountdownSeconds;
					const int shownNumber =
						std::clamp(static_cast<int>(std::ceil(secs)), 1, 10);

					// Local progress through the current number: 0 when it first
					// appears, approaching 1 just before the next number.
					float localTime = static_cast<float>(shownNumber) - secs;
					localTime = std::clamp(localTime, 0.f, 1.f);

					const float growPhase = 0.75f;
					float growT = 1.f;
					float alpha = 1.f;
					if (localTime < growPhase)
					{
						// Float up and rapidly grow.
						growT = localTime / growPhase;
						alpha = 1.f;
					}
					else
					{
						// Final quarter second: fade out to invisible.
						growT = 1.f;
						alpha = 1.f - (localTime - growPhase) / (1.f - growPhase);
					}

					const float windowWidth = static_cast<float>(window.getSize().x);
					const float windowHeight = static_cast<float>(window.getSize().y);

					const float floatUp = growT * 70.f;
					const float centerX = windowWidth * 0.5f;
					// Slightly above the exact centre.
					const float centerY = windowHeight * 0.42f - floatUp;

					const unsigned int minSize = 80;
					const unsigned int maxSize = 190;
					const unsigned int charSize = minSize +
						static_cast<unsigned int>(
							static_cast<float>(maxSize - minSize) * growT);

					// Flash between red and white.
					const bool useWhite =
						(static_cast<int>(totalAppTime * 12.f) % 2) == 0;
					const std::uint8_t a = static_cast<std::uint8_t>(
						std::clamp(static_cast<int>(alpha * 255.f), 0, 255));

					sf::Color fill = useWhite
						? sf::Color(255, 255, 255, a)
						: sf::Color(235, 40, 40, a);
					sf::Color outline(0, 0, 0, a);

					sf::Text countdownText(game1UiFont);
					countdownText.setString(std::to_string(shownNumber));
					countdownText.setCharacterSize(charSize);
					countdownText.setFillColor(fill);
					countdownText.setOutlineColor(outline);
					countdownText.setOutlineThickness(5.f);

					const sf::FloatRect bounds = countdownText.getLocalBounds();
					countdownText.setPosition({
						centerX - bounds.size.x * 0.5f - bounds.position.x,
						centerY - bounds.size.y * 0.5f - bounds.position.y
						});
					window.draw(countdownText);
				}
			}

			DrawGame1VictoryPopup();

			pauseMenu.layout(window);
			hubOptions.layout(window);
			hubOptions.update(deltaTime);
			pauseMenu.draw(window);
			if (hubOptions.isOpen())
			{
				hubOptions.draw(window);
			}

			DisplayFrame();
		}
		else if (appState == AppState::GAME1_Editor)
		{
			game1Editor.update(deltaTime, window.getSize());

			window.clear(sf::Color(80, 170, 255));
			game1Editor.draw(window, sf::Mouse::getPosition(window));
			DisplayFrame();
		}
		else if (appState == AppState::GAME1_LevelSelect)
		{
			game1LevelSelect.layout(window);

			window.clear(sf::Color(25, 25, 35));
			game1LevelSelect.draw(window);
			DisplayFrame();
		}
		else if (appState == AppState::GAME1_Menu)
		{
			game1Menu.layout(window);

			window.clear(sf::Color(30, 30, 40));
			game1Menu.draw(window);
			DisplayFrame();
		}
		else if (appState == AppState::GAME2_Menu)
		{
			game2Menu.layout(window);

			window.clear(sf::Color(24, 24, 34));
			game2Menu.draw(window);
			DisplayFrame();
		}

	}

	return 0;
}
