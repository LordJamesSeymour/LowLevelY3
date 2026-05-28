#pragma once

#include <SFML/Audio.hpp>

#include "ArcadeSettings.h"

#include <filesystem>
#include <string>

class BombermanAudio
{
public:
	static void initialise(const std::string& bombermanRootDirectory)
	{
		namespace fs = std::filesystem;

		const fs::path rootPath(bombermanRootDirectory);
		const std::string normalisedRoot = rootPath.lexically_normal().string();

		if (s_loaded && s_rootDirectory == normalisedRoot)
			return;

		stopAllMusic();

		s_rootDirectory = normalisedRoot;
		s_loaded = true;
		s_activeTrack = Track::None;
		s_menuWaiting = false;
		s_menuWaitTimer = 0.f;

		const fs::path ostDirectory = rootPath / "Resources" / "Audio" / "OST";

		s_hasMenuMusic = s_menuMusic.openFromFile((ostDirectory / "bomberman_title.mp3").string());
		s_hasGameplayMusic = s_gameplayMusic.openFromFile((ostDirectory / "bomberman_level1.mp3").string());
		s_hasGameOverMusic = s_gameOverMusic.openFromFile((ostDirectory / "bomberman_gameover.mp3").string());
		s_hasLevelCompleteMusic = s_levelCompleteMusic.openFromFile((ostDirectory / "bomberman_levelcomplete.mp3").string());

		if (s_hasMenuMusic)
		{
			s_menuMusic.setLooping(false);
			s_menuMusic.setVolume(ArcadeSettings::scaleMusic(kBaseMenuVolume));
		}

		if (s_hasGameplayMusic)
		{
			s_gameplayMusic.setLooping(true);
			s_gameplayMusic.setVolume(ArcadeSettings::scaleMusic(kBaseGameplayVolume));
		}

		if (s_hasGameOverMusic)
		{
			s_gameOverMusic.setLooping(false);
			s_gameOverMusic.setVolume(ArcadeSettings::scaleMusic(kBaseGameOverVolume));
		}

		if (s_hasLevelCompleteMusic)
		{
			s_levelCompleteMusic.setLooping(false);
			s_levelCompleteMusic.setVolume(ArcadeSettings::scaleMusic(kBaseLevelCompleteVolume));
		}
	}

	static void playMenuMusic()
	{
		if (s_activeTrack == Track::Menu)
			return;

		stopAllMusicInternal();
		s_activeTrack = Track::Menu;
		s_menuWaiting = false;
		s_menuWaitTimer = 0.f;

		if (s_hasMenuMusic)
		{
			s_menuMusic.play();
		}
	}

	static void updateMenuMusic(float deltaTime)
	{
		if (s_activeTrack != Track::Menu)
			return;

		if (!s_hasMenuMusic)
			return;

		if (s_menuWaiting)
		{
			s_menuWaitTimer -= deltaTime;
			if (s_menuWaitTimer <= 0.f)
			{
				s_menuWaiting = false;
				s_menuWaitTimer = 0.f;
				s_menuMusic.play();
			}
		}
		else
		{
			if (s_menuMusic.getStatus() == sf::SoundSource::Status::Stopped)
			{
				s_menuWaiting = true;
				s_menuWaitTimer = kMenuReplayDelaySeconds;
			}
		}
	}

	static void playGameplayMusic()
	{
		if (s_activeTrack == Track::Gameplay &&
			s_hasGameplayMusic &&
			s_gameplayMusic.getStatus() == sf::SoundSource::Status::Playing)
		{
			return;
		}

		stopAllMusicInternal();
		s_activeTrack = Track::Gameplay;

		if (s_hasGameplayMusic)
		{
			s_gameplayMusic.play();
		}
	}

	static void playGameOverMusic()
	{
		if (s_activeTrack == Track::GameOver)
			return;

		stopAllMusicInternal();
		s_activeTrack = Track::GameOver;

		if (s_hasGameOverMusic)
		{
			s_gameOverMusic.play();
		}
	}

	static void playLevelCompleteMusic()
	{
		if (s_activeTrack == Track::LevelComplete)
			return;

		stopAllMusicInternal();
		s_activeTrack = Track::LevelComplete;

		if (s_hasLevelCompleteMusic)
		{
			s_levelCompleteMusic.play();
		}
	}

	static void stopAllMusic()
	{
		stopAllMusicInternal();
		s_activeTrack = Track::None;
		s_menuWaiting = false;
		s_menuWaitTimer = 0.f;
	}

	static void refreshVolumes()
	{
		if (s_hasMenuMusic)
			s_menuMusic.setVolume(ArcadeSettings::scaleMusic(kBaseMenuVolume));

		if (s_hasGameplayMusic)
			s_gameplayMusic.setVolume(ArcadeSettings::scaleMusic(kBaseGameplayVolume));

		if (s_hasGameOverMusic)
			s_gameOverMusic.setVolume(ArcadeSettings::scaleMusic(kBaseGameOverVolume));

		if (s_hasLevelCompleteMusic)
			s_levelCompleteMusic.setVolume(ArcadeSettings::scaleMusic(kBaseLevelCompleteVolume));
	}

private:
	enum class Track
	{
		None,
		Menu,
		Gameplay,
		GameOver,
		LevelComplete
	};

	static void stopAllMusicInternal()
	{
		if (s_hasMenuMusic)
			s_menuMusic.stop();

		if (s_hasGameplayMusic)
			s_gameplayMusic.stop();

		if (s_hasGameOverMusic)
			s_gameOverMusic.stop();

		if (s_hasLevelCompleteMusic)
			s_levelCompleteMusic.stop();
	}

private:
	static constexpr float kBaseMenuVolume = 65.f;
	static constexpr float kBaseGameplayVolume = 55.f;
	static constexpr float kBaseGameOverVolume = 70.f;
	static constexpr float kBaseLevelCompleteVolume = 70.f;
	static constexpr float kMenuReplayDelaySeconds = 15.f;

	inline static sf::Music s_menuMusic;
	inline static sf::Music s_gameplayMusic;
	inline static sf::Music s_gameOverMusic;
	inline static sf::Music s_levelCompleteMusic;

	inline static bool s_hasMenuMusic = false;
	inline static bool s_hasGameplayMusic = false;
	inline static bool s_hasGameOverMusic = false;
	inline static bool s_hasLevelCompleteMusic = false;

	inline static bool s_loaded = false;
	inline static std::string s_rootDirectory;

	inline static Track s_activeTrack = Track::None;

	inline static bool s_menuWaiting = false;
	inline static float s_menuWaitTimer = 0.f;
};
