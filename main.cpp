#include <SFML/Graphics.hpp>
#include <filesystem>
#include <optional>
#include <string>
#include <windows.h>

#include "Level.h"
#include "Player.h"
#include "Menu.h"

enum class AppState
{
    Menu,
    Game
};

void ShowError(const std::string& message)
{
    MessageBoxA(nullptr, message.c_str(), "Project Error", MB_OK | MB_ICONERROR);
}

bool LoadGame(Level& level, Player& player)
{
    if (!level.loadFromFile("maps/level01.txt", "assets/FloorTile.png", "assets/breakblock.png"))
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

    level.spawnRandomBreakBlocks(6, player.getBounds());
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

    AppState appState = AppState::Menu;

    Level level;
    Player player;
    bool gameLoaded = false;

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
                            if (!gameLoaded)
                            {
                                if (!LoadGame(level, player))
                                    return -1;

                                gameLoaded = true;
                            }

                            appState = AppState::Game;
                            break;

                        case MenuAction::LevelEditor:
                            break;

                        case MenuAction::None:
                        default:
                            break;
                        }
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