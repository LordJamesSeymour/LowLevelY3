#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>
#include <windows.h>

#include "Level.h"
#include "Player.h"
#include "Menu.h"
#include "LevelEditor.h"
#include "LevelSelect.h"

enum class AppState
{
	Menu,
	LevelSelect,
	Game,
	Editor
};

void ShowError(const std::string& message)
{
	MessageBoxA(nullptr, message.c_str(), "Project Error", MB_OK | MB_ICONERROR);
}

void ShowInfo(const std::string& message)
{
	MessageBoxA(nullptr, message.c_str(), "Info", MB_OK | MB_ICONINFORMATION);
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

	if (!fs::exists("maps"))
		return {};

	for (const auto& entry : fs::directory_iterator("maps"))
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

bool LoadGame(Level& level, Player& player, const std::string& mapPath)
{
	if (!level.loadFromFile(mapPath, "assets/FloorTile.png", "assets/breakblock.png"))
	{
		std::string msg =
			"Level failed to load.\n\n" +
			level.getLastError() +
			"\n\nCurrent working directory:\n" +
			std::filesystem::current_path().string();

		ShowError(msg);
		return false;
	}

	if (!player.load("assets/player.png", { 100.f, 100.f }))
	{
		std::string msg =
			"Player failed to load.\n\n" +
			player.getLastError() +
			"\n\nCurrent working directory:\n" +
			std::filesystem::current_path().string();

		ShowError(msg);
		return false;
	}

	return true;
}

int main()
{
	sf::RenderWindow window(sf::VideoMode({ 1024, 640 }), "Mini Platformer");
	window.setFramerateLimit(60);

	Menu menu;
	if (!menu.load("assets/xbutton.png",
		"assets/logo.png",
		"assets/play.png",
		"assets/leveleditor.png"))
	{
		std::string msg =
			"Menu failed to load.\n\n" +
			menu.getLastError() +
			"\n\nCurrent working directory:\n" +
			std::filesystem::current_path().string();

		ShowError(msg);
		return -1;
	}

	LevelSelect levelSelect;
	if (!levelSelect.load("assets/menu.ttf"))
	{
		std::string msg =
			"Level select failed to load.\n\n" +
			levelSelect.getLastError() +
			"\n\nCurrent working directory:\n" +
			std::filesystem::current_path().string();

		ShowError(msg);
		return -1;
	}

	LevelEditor editor;
	if (!editor.load("assets/FloorTile.png", "assets/breakblock.png"))
	{
		std::string msg =
			"Level editor failed to load.\n\n" +
			editor.getLastError() +
			"\n\nCurrent working directory:\n" +
			std::filesystem::current_path().string();

		ShowError(msg);
		return -1;
	}

	AppState appState = AppState::Menu;

	Level level;
	Player player;

	sf::Clock clock;

	while (window.isOpen())
	{
		const float deltaTime = clock.restart().asSeconds();

		while (const std::optional event = window.pollEvent())
		{
			if (event->is<sf::Event::Closed>())
			{
				window.close();
			}

			if (appState == AppState::Menu)
			{
				if (const auto* mousePressed = event->getIf<sf::Event::MouseButtonPressed>())
				{
					if (mousePressed->button == sf::Mouse::Button::Left)
					{
						const sf::Vector2f mousePosition(
							static_cast<float>(mousePressed->position.x),
							static_cast<float>(mousePressed->position.y)
						);

						switch (menu.handleClick(mousePosition))
						{
						case MenuAction::Quit:
							window.close();
							break;

						case MenuAction::Play:
							levelSelect.setLevels(GetFirstFiveLevelPaths());
							appState = AppState::LevelSelect;
							break;

						case MenuAction::LevelEditor:
							editor.resetEmpty();
							appState = AppState::Editor;
							break;

						case MenuAction::None:
						default:
							break;
						}
					}
				}
			}
			else if (appState == AppState::LevelSelect)
			{
				if (const auto* keyReleased = event->getIf<sf::Event::KeyReleased>())
				{
					if (keyReleased->code == sf::Keyboard::Key::Escape)
					{
						appState = AppState::Menu;
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

						const int slotIndex = levelSelect.handleClick(mousePosition);

						if (slotIndex >= 0 && levelSelect.hasLevelAt(slotIndex))
						{
							if (!LoadGame(level, player, levelSelect.getLevelPathAt(slotIndex)))
								return -1;

							appState = AppState::Game;
						}
					}
				}
			}
			else if (appState == AppState::Game)
			{
				if (const auto* keyReleased = event->getIf<sf::Event::KeyReleased>())
				{
					if (keyReleased->code == sf::Keyboard::Key::Escape)
					{
						appState = AppState::Menu;
					}
				}
			}
			else if (appState == AppState::Editor)
			{
				if (const auto* keyReleased = event->getIf<sf::Event::KeyReleased>())
				{
					if (keyReleased->code == sf::Keyboard::Key::Escape)
					{
						appState = AppState::Menu;
					}
					else if (keyReleased->code == sf::Keyboard::Key::B)
					{
						editor.toggleFloorBrush();
					}
					else if (keyReleased->code == sf::Keyboard::Key::F)
					{
						editor.toggleBreakBrush();
					}
					else if (keyReleased->code == sf::Keyboard::Key::P)
					{
						if (!editor.saveToNextLevelFile())
						{
							ShowError(editor.getLastError());
						}
						else
						{
							ShowInfo("Level saved successfully.\n\n" + editor.getLastSavedPath());
						}
					}
				}

				if (const auto* mousePressed = event->getIf<sf::Event::MouseButtonPressed>())
				{
					if (mousePressed->button == sf::Mouse::Button::Left)
					{
						editor.paintAtPixel({
							mousePressed->position.x,
							mousePressed->position.y
							});
					}
				}
			}
		}

		if (appState == AppState::Game)
		{
			player.update(deltaTime, level, window.getSize().x);

			window.clear(sf::Color(120, 190, 255));
			level.draw(window);
			player.draw(window);
			window.display();
		}
		else if (appState == AppState::Editor)
		{
			window.clear(sf::Color(80, 170, 255));
			editor.draw(window);
			window.display();
		}
		else if (appState == AppState::LevelSelect)
		{
			levelSelect.layout(window);

			window.clear(sf::Color(25, 25, 35));
			levelSelect.draw(window);
			window.display();
		}
		else
		{
			menu.layout(window);

			window.clear(sf::Color(30, 30, 40));
			menu.draw(window);
			window.display();
		}
	}

	return 0;
}