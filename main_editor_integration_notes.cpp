// main.cpp integration notes for the updated GAME1_LevelEditor.
// IMPORTANT: mouse-wheel selection only works if main.cpp forwards the MouseWheelScrolled event.
// If you only call paintAtPixel() on left click, the editor can draw the hotbar but it cannot receive wheel input.

// Preferred editor load call:
// const std::filesystem::path kSurfersQuestRootDirectory = "assets/Game#1/SurfersQuest";
// editor.load(kGlobalFontPath.string(), kSurfersQuestRootDirectory.string());

// In your event loop, inside the SurfersQuest editor state branch, use this pattern:
/*
if (const auto* keyReleased = event->getIf<sf::Event::KeyReleased>())
{
    if (keyReleased->code == sf::Keyboard::Key::Escape)
    {
        SetAppState(AppState::GAME1_Menu); // or ArcadeHub, depending on your current state flow
    }
    else
    {
        editor.handleKeyReleased(keyReleased->code);
    }
}

if (const auto* mousePressed = event->getIf<sf::Event::MouseButtonPressed>())
{
    editor.handleMousePressed(mousePressed->button, mousePressed->position);
}

if (const auto* mouseWheel = event->getIf<sf::Event::MouseWheelScrolled>())
{
    editor.handleMouseWheelScrolled(mouseWheel->delta);
}
*/

// In the editor update/draw branch:
/*
editor.update(deltaTime, window.getSize());
editor.draw(window, sf::Mouse::getPosition(window));
*/

// Remove old editor controls from main.cpp:
// - B released -> toggleFloorBrush()
// - F released -> toggleBreakBrush()
// - P released -> saveToNextLevelFile()
// - Left mouse only -> paintAtPixel(...)
// Those bypass the real UI events, especially mouse wheel and middle-click picker.
