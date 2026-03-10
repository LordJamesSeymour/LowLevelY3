#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>
#include <windows.h>

#include "ArcadeHub.h"
#include "GAME1_Level.h"
#include "GAME1_Player.h"
#include "GAME1_Menu.h"
#include "GAME1_LevelEditor.h"
#include "GAME1_LevelSelect.h"

namespace
{
	// -------------------------------------------------------------------------
	// Shared/global asset paths
	// -------------------------------------------------------------------------
	const std::filesystem::path kGlobalFontPath = "Assets/menu.ttf";
	const std::filesystem::path kHubShaderPath = "Assets/Shaders/ArcadeHubCRT.frag";

	// -------------------------------------------------------------------------
	// Game 1 asset paths
	// -------------------------------------------------------------------------
	const std::filesystem::path kGame1ResourcesDirectory = "Assets/Game#1/Resources";
	const std::filesystem::path kGame1MapsDirectory = "Assets/Game#1/Maps";
	const std::filesystem::path kGame1SplashScreenPath = "Assets/Game#1/SplashScreen/SidescrollerSplashScreen.png";

	// -------------------------------------------------------------------------
	// Application states
	// -------------------------------------------------------------------------
	enum class AppState
	{
		Hub,
		GAME1_Menu,
		GAME1_LevelSelect,
		GAME1_Game,
		GAME1_Editor
	};

	// -------------------------------------------------------------------------
	// Simple Windows popup helpers for readable debugging
	// -------------------------------------------------------------------------
	void ShowError(const std::string& message)
	{
		MessageBoxA(nullptr, message.c_str(), "Project Error", MB_OK | MB_ICONERROR);
	}

	void ShowInfo(const std::string& message)
	{
		MessageBoxA(nullptr, message.c_str(), "Info", MB_OK | MB_ICONINFORMATION);
	}

	// -------------------------------------------------------------------------
	// Tries to find a .sln file in the current directory or nearby parent folders.
	// The hub uses that filename as the top-left project title.
	// -------------------------------------------------------------------------
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

	// -------------------------------------------------------------------------
	// Shared level filename helpers for Game 1
	// -------------------------------------------------------------------------
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

	// -------------------------------------------------------------------------
	// Loads a chosen Game 1 level and the Game 1 player assets
	// -------------------------------------------------------------------------
	bool LoadGame1(GAME1_Level& level, GAME1_Player& player, const std::string& mapPath)
	{
		const std::string floorTilePath = (kGame1ResourcesDirectory / "FloorTile.png").string();
		const std::string breakBlockPath = (kGame1ResourcesDirectory / "breakblock.png").string();
		const std::string playerPath = (kGame1ResourcesDirectory / "player.png").string();

		if (!level.loadFromFile(mapPath, floorTilePath, breakBlockPath))
		{
			std::string msg =
				"Game 1 level failed to load.\n\n" +
				level.getLastError() +
				"\n\nCurrent working directory:\n" +
				std::filesystem::current_path().string();

			ShowError(msg);
			return false;
		}

		if (!player.load(playerPath, { 100.f, 100.f }))
		{
			std::string msg =
				"Game 1 player failed to load.\n\n" +
				player.getLastError() +
				"\n\nCurrent working directory:\n" +
				std::filesystem::current_path().string();

			ShowError(msg);
			return false;
		}

		return true;
	}
}

int main()
{
	sf::RenderWindow window(sf::VideoMode({ 1024, 640 }), "Arcade Collection");
	window.setFramerateLimit(60);

	// -------------------------------------------------------------------------
	// Hub post-processing setup
	// We render the hub to a texture first, then pass that texture through
	// a CRT shader to get scanlines, flicker, refresh sweep, etc.
	// -------------------------------------------------------------------------
	sf::RenderTexture hubRenderTexture;
	if (!hubRenderTexture.resize(window.getSize()))
	{
		ShowError("Failed to create the hub render texture.");
		return -1;
	}

	hubRenderTexture.setSmooth(true);

	sf::Shader hubCrtShader;
	bool hubCrtShaderEnabled = false;

	if (sf::Shader::isAvailable())
	{
		if (hubCrtShader.loadFromFile(kHubShaderPath.string(), sf::Shader::Type::Fragment))
		{
			hubCrtShaderEnabled = true;
		}
	}

	float totalAppTime = 0.f;

	// -------------------------------------------------------------------------
	// Top-level arcade hub setup
	// -------------------------------------------------------------------------
	ArcadeHub hub;

	std::vector<ArcadeHubGameEntry> hubGames;
	hubGames.push_back({
		"GAME #1",
		"Sidescroller Platformer",
		kGame1SplashScreenPath.string()
		});

	if (!hub.load(kGlobalFontPath.string(), GetProjectNameFromSolution(), hubGames))
	{
		std::string msg =
			"Arcade hub failed to load.\n\n" +
			hub.getLastError() +
			"\n\nCurrent working directory:\n" +
			std::filesystem::current_path().string();

		ShowError(msg);
		return -1;
	}

	// -------------------------------------------------------------------------
	// Game 1 setup
	// -------------------------------------------------------------------------
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
	if (!game1Editor.load((kGame1ResourcesDirectory / "FloorTile.png").string(),
		(kGame1ResourcesDirectory / "breakblock.png").string()))
	{
		std::string msg =
			"Game 1 level editor failed to load.\n\n" +
			game1Editor.getLastError() +
			"\n\nCurrent working directory:\n" +
			std::filesystem::current_path().string();

		ShowError(msg);
		return -1;
	}

	AppState appState = AppState::Hub;

	GAME1_Level game1Level;
	GAME1_Player game1Player;

	sf::Clock clock;

	while (window.isOpen())
	{
		const float deltaTime = clock.restart().asSeconds();
		totalAppTime += deltaTime;

		while (const std::optional event = window.pollEvent())
		{
			// Always allow the window to close no matter what state we are in.
			if (event->is<sf::Event::Closed>())
			{
				window.close();
			}

			// -----------------------------------------------------------------
			// Arcade hub input
			// -----------------------------------------------------------------
			if (appState == AppState::Hub)
			{
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
						if (hub.getSelectedIndex() == 0)
						{
							appState = AppState::GAME1_Menu;
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
							if (hub.getSelectedIndex() == 0)
							{
								appState = AppState::GAME1_Menu;
							}
							break;

						case ArcadeHubAction::None:
						default:
							break;
						}
					}
				}
			}

			// -----------------------------------------------------------------
			// Game 1 menu input
			// -----------------------------------------------------------------
			else if (appState == AppState::GAME1_Menu)
			{
				if (const auto* keyReleased = event->getIf<sf::Event::KeyReleased>())
				{
					if (keyReleased->code == sf::Keyboard::Key::Escape)
					{
						appState = AppState::Hub;
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

						switch (game1Menu.handleClick(mousePosition))
						{
						case GAME1_MenuAction::Quit:
							window.close();
							break;

						case GAME1_MenuAction::Play:
							game1LevelSelect.setLevels(GetFirstFiveLevelPaths());
							appState = AppState::GAME1_LevelSelect;
							break;

						case GAME1_MenuAction::LevelEditor:
							game1Editor.resetEmpty();
							appState = AppState::GAME1_Editor;
							break;

						case GAME1_MenuAction::None:
						default:
							break;
						}
					}
				}
			}

			// -----------------------------------------------------------------
			// Game 1 level select input
			// -----------------------------------------------------------------
			else if (appState == AppState::GAME1_LevelSelect)
			{
				if (const auto* keyReleased = event->getIf<sf::Event::KeyReleased>())
				{
					if (keyReleased->code == sf::Keyboard::Key::Escape)
					{
						appState = AppState::GAME1_Menu;
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
							if (!LoadGame1(game1Level, game1Player, game1LevelSelect.getLevelPathAt(slotIndex)))
								return -1;

							appState = AppState::GAME1_Game;
						}
					}
				}
			}

			// -----------------------------------------------------------------
			// Game 1 gameplay input
			// -----------------------------------------------------------------
			else if (appState == AppState::GAME1_Game)
			{
				if (const auto* keyReleased = event->getIf<sf::Event::KeyReleased>())
				{
					if (keyReleased->code == sf::Keyboard::Key::Escape)
					{
						appState = AppState::GAME1_Menu;
					}
				}
			}

			// -----------------------------------------------------------------
			// Game 1 editor input
			// -----------------------------------------------------------------
			else if (appState == AppState::GAME1_Editor)
			{
				if (const auto* keyReleased = event->getIf<sf::Event::KeyReleased>())
				{
					if (keyReleased->code == sf::Keyboard::Key::Escape)
					{
						appState = AppState::GAME1_Menu;
					}
					else if (keyReleased->code == sf::Keyboard::Key::B)
					{
						game1Editor.toggleFloorBrush();
					}
					else if (keyReleased->code == sf::Keyboard::Key::F)
					{
						game1Editor.toggleBreakBrush();
					}
					else if (keyReleased->code == sf::Keyboard::Key::P)
					{
						if (!game1Editor.saveToNextLevelFile())
						{
							ShowError(game1Editor.getLastError());
						}
						else
						{
							ShowInfo("Level saved successfully.\n\n" + game1Editor.getLastSavedPath());
						}
					}
				}

				if (const auto* mousePressed = event->getIf<sf::Event::MouseButtonPressed>())
				{
					if (mousePressed->button == sf::Mouse::Button::Left)
					{
						game1Editor.paintAtPixel({
							mousePressed->position.x,
							mousePressed->position.y
							});
					}
				}
			}
		}

		// ---------------------------------------------------------------------
		// Per-frame update + drawing
		// ---------------------------------------------------------------------
		if (appState == AppState::Hub)
		{
			hub.updateClockText();
			hub.updateVisualTheme(totalAppTime);
			hub.layout(window);

			// Keep render texture size synced to the window size.
			if (hubRenderTexture.getSize() != window.getSize())
			{
				if (!hubRenderTexture.resize(window.getSize()))
				{
					hubCrtShaderEnabled = false;
				}
			}

			if (hubCrtShaderEnabled)
			{
				hubRenderTexture.clear(sf::Color::Black);
				hub.draw(hubRenderTexture);
				hubRenderTexture.display();

				sf::Sprite hubSprite(hubRenderTexture.getTexture());

				hubCrtShader.setUniform("u_texture", sf::Shader::CurrentTexture);
				hubCrtShader.setUniform("u_time", totalAppTime);
				hubCrtShader.setUniform("u_resolution", sf::Vector2f(
					static_cast<float>(window.getSize().x),
					static_cast<float>(window.getSize().y)
				));

				sf::RenderStates shaderStates;
				shaderStates.shader = &hubCrtShader;

				window.clear(sf::Color::Black);
				window.draw(hubSprite, shaderStates);
				window.display();
			}
			else
			{
				window.clear(sf::Color::Black);
				hub.draw(window);
				window.display();
			}
		}
		else if (appState == AppState::GAME1_Game)
		{
			game1Player.update(deltaTime, game1Level, window.getSize().x);

			window.clear(sf::Color(120, 190, 255));
			game1Level.draw(window);
			game1Player.draw(window);
			window.display();
		}
		else if (appState == AppState::GAME1_Editor)
		{
			window.clear(sf::Color(80, 170, 255));
			game1Editor.draw(window);
			window.display();
		}
		else if (appState == AppState::GAME1_LevelSelect)
		{
			game1LevelSelect.layout(window);

			window.clear(sf::Color(25, 25, 35));
			game1LevelSelect.draw(window);
			window.display();
		}
		else // GAME1_Menu
		{
			game1Menu.layout(window);

			window.clear(sf::Color(30, 30, 40));
			game1Menu.draw(window);
			window.display();
		}
	}

	return 0;
}