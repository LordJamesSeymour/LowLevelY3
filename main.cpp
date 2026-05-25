#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>
#include <windows.h>

#include "ArcadeHub.h"
#include "ArcadeInput.h"

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
#include "GAME1_SurfersQuestAudio.h"

#include "GAME2_Menu.h"
#include "GAME2_Game.h"

namespace
{
	const std::filesystem::path kGlobalFontPath = "assets/menu.ttf";
	const std::filesystem::path kHubShaderPath = "assets/Shaders/ArcadeHubCRT.frag";
	const std::filesystem::path kLockedImagePath = "assets/LockedImage.png";

	const std::filesystem::path kBombermanRootDirectory = "assets/Game#0/Bomberman";
	const std::filesystem::path kBombermanMapsDirectory = "assets/Game#0/Bomberman/Maps";
	const std::filesystem::path kBombermanSplashStillImagePath = "assets/Game#0/SplashScreen/BombermanSplashScreen.png";
	const std::filesystem::path kBombermanSplashFramesDirectory = "assets/Game#0/SplashScreen/GIFs";

	const std::filesystem::path kGame1RootDirectory = "assets/Game#1/SurfersQuest";
	const std::filesystem::path kGame1ResourcesDirectory = kGame1RootDirectory / "Resources";
	const std::filesystem::path kGame1MapsDirectory = kGame1RootDirectory / "Maps";
	const std::filesystem::path kGame1SplashStillImagePath = "assets/Game#1/SplashScreen/SidescrollerSplashScreen.png";
	const std::filesystem::path kGame1SplashFramesDirectory = "assets/Game#1/SplashScreen/GIFs";

	const std::filesystem::path kGame2ResourcesDirectory = "assets/Game#2/Resources";
	const std::filesystem::path kGame2SplashStillImagePath = "assets/Game#2/SplashScreen/Game2SplashScreen.png";

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

	void ShowError(const std::string& message)
	{
		MessageBoxA(nullptr, message.c_str(), "Project Error", MB_OK | MB_ICONERROR);
	}

	void ShowInfo(const std::string& message)
	{
		MessageBoxA(nullptr, message.c_str(), "Info", MB_OK | MB_ICONINFORMATION);
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

	std::vector<std::string> GetFirstFiveLevelPaths()
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

		const std::size_t count = paths.size() < 5 ? paths.size() : 5;

		for (std::size_t i = 0; i < count; ++i)
		{
			result.push_back(paths[i].string());
		}

		return result;
	}

	bool LoadGame1(GAME1_Level& level, GAME1_Player& player, std::vector<GAME1_Enemy>& enemies, const std::string& mapPath)
	{
		const std::string playerIdleDirectory =
			(kGame1ResourcesDirectory / "Player" / "PlayerIdle").string();

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

		enemies.clear();

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

		return true;
	}
}

int main()
{
	sf::RenderWindow window(sf::VideoMode({ 1024, 640 }), "Arcade Collection");
	window.setFramerateLimit(60);

	ApplyWindowView(window);

	sf::Texture crtFrameTexture;
	if (!crtFrameTexture.resize(window.getSize()))
	{
		ShowError("Failed to create the CRT frame texture.");
		return -1;
	}

	crtFrameTexture.setSmooth(false);

	sf::Shader crtShader;
	bool crtShaderEnabled = false;

	if (sf::Shader::isAvailable())
	{
		if (crtShader.loadFromFile(kHubShaderPath.string(), sf::Shader::Type::Fragment))
		{
			crtShaderEnabled = true;
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
	std::vector<GAME1_Enemy> game1Enemies;

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

	AppState appState = AppState::Hub;

	auto IsSurfersQuestState = [](AppState state)
		{
			return state == AppState::GAME1_Menu ||
				state == AppState::GAME1_LevelSelect ||
				state == AppState::GAME1_Game ||
				state == AppState::GAME1_Editor;
		};

	auto SetAppState = [&](AppState newState)
		{
			const AppState previousState = appState;

			if (previousState == AppState::GAME1_BombermanMenu &&
				newState != AppState::GAME1_BombermanMenu)
			{
				bombermanMenu.stopMusic();
			}

			if (IsSurfersQuestState(previousState) &&
				!IsSurfersQuestState(newState))
			{
				game1Menu.stopMusic();
			}

			appState = newState;

			if (appState == AppState::Hub)
			{
				hub.notifyUserActivity();
			}

			if (appState == AppState::GAME1_BombermanMenu)
			{
				bombermanMenu.resetSelection();
				bombermanMenu.startMusic();
			}

			if (appState == AppState::GAME1_Menu)
			{
				game1Menu.resetSelection();
				game1Menu.startMusic();
			}

			if (appState == AppState::GAME1_LevelSelect)
			{
				game1Menu.startMusic();
			}

			if (appState == AppState::GAME1_Editor)
			{
				game1Menu.stopMusic();
			}
		};

	auto TryLaunchSelectedHubGame = [&]()
		{
			if (hub.isSelectedGameLocked())
				return;

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
				game1LevelSelect.setLevels(GetFirstFiveLevelPaths());
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
					crtShaderEnabled = false;
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
				if (event->is<sf::Event::KeyPressed>() ||
					event->is<sf::Event::KeyReleased>() ||
					event->is<sf::Event::MouseButtonPressed>() ||
					event->is<sf::Event::MouseButtonReleased>() ||
					event->is<sf::Event::MouseMoved>() ||
					event->is<sf::Event::MouseWheelScrolled>())
				{
					hub.notifyUserActivity();
				}

				if (const auto* keyReleased = event->getIf<sf::Event::KeyReleased>())
				{
					if (keyReleased->code == sf::Keyboard::Key::Left)
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
			else if (appState == AppState::GAME1_BombermanMenu)
			{
				if (const auto* keyReleased = event->getIf<sf::Event::KeyReleased>())
				{
					const bool menuConsumedKey = bombermanMenu.handleKeyReleased(keyReleased->code);

					if (!menuConsumedKey)
					{
						if (keyReleased->code == sf::Keyboard::Key::Escape)
						{
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
			}
			else if (appState == AppState::GAME1_BombermanLevelSelect)
			{
				if (const auto* keyReleased = event->getIf<sf::Event::KeyReleased>())
				{
					if (keyReleased->code == sf::Keyboard::Key::Escape)
					{
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
							SetAppState(AppState::GAME1_BombermanMenu);
							break;

						case GAME1_BombermanLevelSelectAction::SelectedLevel:
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
				if (const auto* keyReleased = event->getIf<sf::Event::KeyReleased>())
				{
					if (keyReleased->code == sf::Keyboard::Key::Escape)
					{
						SetAppState(AppState::GAME1_BombermanMenu);
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
			}
			else if (appState == AppState::GAME1_LevelSelect)
			{
				if (const auto* keyReleased = event->getIf<sf::Event::KeyReleased>())
				{
					if (keyReleased->code == sf::Keyboard::Key::Escape)
					{
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
							if (!LoadGame1(game1Level, game1Player, game1Enemies, game1LevelSelect.getSelectedLevelPath()))
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

						if (slotIndex >= 0 && game1LevelSelect.hasLevelAt(slotIndex))
						{
							if (!LoadGame1(game1Level, game1Player, game1Enemies, game1LevelSelect.getLevelPathAt(slotIndex)))
								return -1;

							SetAppState(AppState::GAME1_Game);
						}
					}
				}
			}
			else if (appState == AppState::GAME1_Game)
			{
				if (const auto* keyReleased = event->getIf<sf::Event::KeyReleased>())
				{
					if (keyReleased->code == sf::Keyboard::Key::Escape)
					{
						SetAppState(AppState::GAME1_Menu);
					}
				}
			}
			else if (appState == AppState::GAME1_Editor)
			{
				if (const auto* keyReleased = event->getIf<sf::Event::KeyReleased>())
				{
					if (keyReleased->code == sf::Keyboard::Key::Escape)
					{
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
			if (ArcadeInput::hasControllerActivity())
			{
				hub.notifyUserActivity();
			}

			if (ArcadeInput::isControllerMoveLeftPressed())
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
					if (!LoadGame1(game1Level, game1Player, game1Enemies, game1LevelSelect.getSelectedLevelPath()))
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
			if (ArcadeInput::isControllerBackPressed())
			{
				SetAppState(AppState::GAME1_Menu);
				ArcadeInput::consumePressedState();
			}
		}


		ApplyWindowView(window);

		auto DisplayFrame = [&]()
			{
				ApplyWindowView(window);

				if (crtShaderEnabled && ResizeTexture(crtFrameTexture, window.getSize()))
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

			window.clear(sf::Color::Black);
			hub.draw(window);
			DisplayFrame();
		}
		else if (appState == AppState::GAME1_BombermanMenu)
		{
			bombermanMenu.layout(window);

			window.clear(sf::Color::Black);
			bombermanMenu.draw(window);
			DisplayFrame();
		}
		else if (appState == AppState::GAME1_BombermanLevelSelect)
		{
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
			bombermanWindow.update(deltaTime, window.getSize());
			bombermanWindow.layout(window);

			window.clear(sf::Color::Black);
			bombermanWindow.draw(window);
			DisplayFrame();
		}
		else if (appState == AppState::GAME1_Game)
		{
			game1Player.update(deltaTime, game1Level);

			for (GAME1_Enemy& enemy : game1Enemies)
			{
				enemy.update(deltaTime, game1Level, game1Player);
			}

			for (GAME1_Enemy& enemy : game1Enemies)
			{
				enemy.handlePlayerCollision(game1Player);
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

			sf::View worldView = window.getDefaultView();

			const sf::FloatRect playerBounds = game1Player.getBounds();
			const float playerCenterX = playerBounds.position.x + playerBounds.size.x * 0.5f;

			const float halfViewWidth = worldView.getSize().x * 0.5f;
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

			worldView.setCenter({
				targetViewCenterX,
				worldView.getSize().y * 0.5f
				});

			window.setView(worldView);

			window.clear(sf::Color(120, 190, 255));
			game1Level.draw(window);

			for (const GAME1_Enemy& enemy : game1Enemies)
			{
				enemy.draw(window);
			}

			game1Player.draw(window);

			ApplyWindowView(window);

			if (game1Player.isRespawning())
			{
				respawnText.setString("Respawning player in: " + std::to_string(game1Player.getRespawnCountdown()));

				const sf::FloatRect textBounds = respawnText.getLocalBounds();
				respawnText.setPosition({
					(static_cast<float>(window.getSize().x) - textBounds.size.x) * 0.5f - textBounds.position.x,
					80.f - textBounds.position.y
					});

				window.draw(respawnText);
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