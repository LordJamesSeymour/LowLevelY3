#pragma once

#include <functional>
#include <string>
#include <vector>

// Persistent global settings for the arcade hub:
//  - CRT shader on/off
//  - Music volume (user-facing 0..10)
//  - SFX volume   (user-facing 0..10)
//
// Loaded once at startup from settings.cfg next to the working directory
// and re-saved automatically whenever a value changes. Audio classes
// can register callbacks to refresh live sf::Music volumes when the
// sliders move.
class ArcadeSettings
{
public:
	static constexpr float kMaxVolume = 10.f;
	static constexpr float kVolumeStep = 0.5f;

	using AudioRefreshCallback = std::function<void()>;

	static void initialize(const std::string& filePath = "settings.cfg");
	static bool save();

	static bool isFullscreenEnabled();
	static void setFullscreenEnabled(bool enabled);

	static bool isCrtEnabled();
	static void setCrtEnabled(bool enabled);

	static float getMusicVolume();
	static void setMusicVolume(float value);

	static float getSfxVolume();
	static void setSfxVolume(float value);

	static bool isMusicMuted();
	static void setMusicMuted(bool muted);

	static bool isSfxMuted();
	static void setSfxMuted(bool muted);

	// Convert a base 0..100 SFML volume into the current scaled volume.
	static float scaleMusic(float baseVolume);
	static float scaleSfx(float baseVolume);

	static void registerAudioRefreshCallback(AudioRefreshCallback callback);
	static void notifyAudioChanged();

	// Window-apply callbacks fire when a window-affecting setting (currently
	// fullscreen) changes, so main() can recreate/reconfigure the SFML window
	// immediately. Modelled on the audio-refresh callback mechanism.
	using WindowApplyCallback = std::function<void()>;
	static void registerWindowApplyCallback(WindowApplyCallback callback);
	static void notifyWindowChanged();
};
