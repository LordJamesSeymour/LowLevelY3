#pragma once

#include <SFML/Audio.hpp>

#include <filesystem>
#include <string>

class GAME1_SurfersQuestAudio
{
public:
	static void initialise(const std::string& resourcesDirectory)
	{
		namespace fs = std::filesystem;

		const fs::path resourcesPath(resourcesDirectory);
		const std::string normalisedResourcesPath = resourcesPath.lexically_normal().string();

		if (s_loaded && s_resourcesDirectory == normalisedResourcesPath)
			return;

		stopAll();

		s_resourcesDirectory = normalisedResourcesPath;
		s_loaded = true;
		s_activeTrack = Track::None;

		const fs::path ostDirectory = resourcesPath / "Audio" / "OST";

		s_hasMenuMusic = s_menuMusic.openFromFile((ostDirectory / "Menu_01.wav").string());
		s_hasGameplayMusic = s_gameplayMusic.openFromFile((ostDirectory / "World1_Play.wav").string());
		s_hasDeathMusic = s_deathMusic.openFromFile((ostDirectory / "World1_Death.wav").string());

		if (s_hasMenuMusic)
		{
			s_menuMusic.setLooping(true);
			s_menuMusic.setVolume(65.f);
		}

		if (s_hasGameplayMusic)
		{
			s_gameplayMusic.setLooping(true);
			s_gameplayMusic.setVolume(55.f);
		}

		if (s_hasDeathMusic)
		{
			s_deathMusic.setLooping(true);
			s_deathMusic.setVolume(65.f);
		}
	}

	static void playMenu()
	{
		playTrack(Track::Menu);
	}

	static void playGameplay()
	{
		playTrack(Track::Gameplay);
	}

	static void playDeath()
	{
		playTrack(Track::Death);
	}

	static void stopAll()
	{
		if (s_hasMenuMusic)
			s_menuMusic.stop();

		if (s_hasGameplayMusic)
			s_gameplayMusic.stop();

		if (s_hasDeathMusic)
			s_deathMusic.stop();

		s_activeTrack = Track::None;
	}

private:
	enum class Track
	{
		None,
		Menu,
		Gameplay,
		Death
	};

private:
	static void playTrack(Track track)
	{
		if (track == s_activeTrack)
		{
			if (getMusicStatus(track) != sf::SoundSource::Status::Playing)
				startMusic(track);

			return;
		}

		if (s_hasMenuMusic)
			s_menuMusic.stop();

		if (s_hasGameplayMusic)
			s_gameplayMusic.stop();

		if (s_hasDeathMusic)
			s_deathMusic.stop();

		s_activeTrack = Track::None;

		if (startMusic(track))
			s_activeTrack = track;
	}

	static bool startMusic(Track track)
	{
		sf::Music* music = nullptr;
		bool hasMusic = false;

		switch (track)
		{
		case Track::Menu:
			music = &s_menuMusic;
			hasMusic = s_hasMenuMusic;
			break;

		case Track::Gameplay:
			music = &s_gameplayMusic;
			hasMusic = s_hasGameplayMusic;
			break;

		case Track::Death:
			music = &s_deathMusic;
			hasMusic = s_hasDeathMusic;
			break;

		case Track::None:
		default:
			return false;
		}

		if (!hasMusic || music == nullptr)
			return false;

		music->play();
		return true;
	}

	static sf::SoundSource::Status getMusicStatus(Track track)
	{
		switch (track)
		{
		case Track::Menu:
			return s_hasMenuMusic ? s_menuMusic.getStatus() : sf::SoundSource::Status::Stopped;

		case Track::Gameplay:
			return s_hasGameplayMusic ? s_gameplayMusic.getStatus() : sf::SoundSource::Status::Stopped;

		case Track::Death:
			return s_hasDeathMusic ? s_deathMusic.getStatus() : sf::SoundSource::Status::Stopped;

		case Track::None:
		default:
			return sf::SoundSource::Status::Stopped;
		}
	}

private:
	inline static sf::Music s_menuMusic;
	inline static sf::Music s_gameplayMusic;
	inline static sf::Music s_deathMusic;

	inline static bool s_hasMenuMusic = false;
	inline static bool s_hasGameplayMusic = false;
	inline static bool s_hasDeathMusic = false;

	inline static bool s_loaded = false;
	inline static std::string s_resourcesDirectory;
	inline static Track s_activeTrack = Track::None;
};
