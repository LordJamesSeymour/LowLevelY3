#pragma once

#include <SFML/Audio.hpp>
#include "ArcadeSettings.h"
#include <iostream>
#include <optional>
#include <string>

// Shared singleton for the global UI button-click sound.
// Call loadUIClick() once at startup, then playUIClick() at every confirmed
// button/menu action.  Volume follows the SFX setting automatically because
// scaleSfx() is queried at play time.
class ArcadeUISounds
{
public:
    static bool loadUIClick(const std::string& path = "assets/UIClick.wav")
    {
        s_hasBuffer = s_buffer.loadFromFile(path);
        if (!s_hasBuffer)
            std::cerr << "[ArcadeUISounds] Warning: could not load UI click sound: " << path << "\n";
        return s_hasBuffer;
    }

    static void playUIClick()
    {
        if (!s_hasBuffer)
            return;

        s_sound.emplace(s_buffer);
        s_sound->setVolume(ArcadeSettings::scaleSfx(100.f));
        s_sound->play();
    }

private:
    inline static sf::SoundBuffer s_buffer;
    inline static bool s_hasBuffer = false;
    inline static std::optional<sf::Sound> s_sound;
};
