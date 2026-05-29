#pragma once

#include <SFML/Audio.hpp>

#include "ArcadeSettings.h"
#include "GAME1_Level.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <optional>
#include <random>
#include <string>
#include <vector>

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
		clearSfx();

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
			s_menuMusic.setVolume(ArcadeSettings::scaleMusic(kBaseMenuMusicVolume));
		}

		if (s_hasGameplayMusic)
		{
			s_gameplayMusic.setLooping(true);
			s_gameplayMusic.setVolume(ArcadeSettings::scaleMusic(kBaseGameplayMusicVolume));
		}

		if (s_hasDeathMusic)
		{
			s_deathMusic.setLooping(true);
			s_deathMusic.setVolume(ArcadeSettings::scaleMusic(kBaseDeathMusicVolume));
		}

		const fs::path audioDirectory = resourcesPath / "Audio";
		const fs::path playerAudioDirectory = audioDirectory / "Player";

		loadBuffer(s_playerDamageBuffer, s_hasPlayerDamageBuffer, (playerAudioDirectory / "PlayerDamage.wav").string());
		loadBuffer(s_playerDeathBuffer, s_hasPlayerDeathBuffer, (playerAudioDirectory / "PlayerDeath.wav").string());
		loadBuffer(s_playerJumpBuffer, s_hasPlayerJumpBuffer, (playerAudioDirectory / "PlayerJump.wav").string());
		loadBuffer(s_playerDoubleJumpBuffer, s_hasPlayerDoubleJumpBuffer, (playerAudioDirectory / "PlayerDoubleJump.wav").string());
		loadBuffer(s_playerPhaseBuffer, s_hasPlayerPhaseBuffer, (playerAudioDirectory / "PlayerPhase.wav").string());
		loadBuffer(s_enemyDeathBuffer, s_hasEnemyDeathBuffer, (audioDirectory / "Enemy" / "EnemyDeath.wav").string());
		loadBuffer(s_checkpointBuffer, s_hasCheckpointBuffer, (audioDirectory / "Misc" / "CheckPoint.wav").string());
		loadBuffer(s_pickupBuffer, s_hasPickupBuffer, (audioDirectory / "Misc" / "PickUp.wav").string());

		loadSoundPoolFromDirectory(s_floorStepBuffers, playerAudioDirectory / "Steps" / "Floor");
		loadSoundPoolFromDirectory(s_grassStepBuffers, playerAudioDirectory / "Steps" / "Grass");
		loadSoundPoolFromDirectory(s_rockStepBuffers, playerAudioDirectory / "Steps" / "Rock");
		loadSoundPoolFromDirectory(s_wallgrabBuffers, playerAudioDirectory / "Wallgrab");

		if (!s_rngSeeded)
		{
			std::random_device rd;
			s_rng.seed(rd());
			s_rngSeeded = true;
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

	static void playPlayerDamage()
	{
		playBuffer(s_playerDamageBuffer, s_hasPlayerDamageBuffer, s_playerDamageSound, 100.f);
	}

	static void playPlayerDeath()
	{
		playBuffer(s_playerDeathBuffer, s_hasPlayerDeathBuffer, s_playerDeathSound, 100.f);
	}

	static void playPlayerJump()
	{
		playBuffer(s_playerJumpBuffer, s_hasPlayerJumpBuffer, s_playerJumpSound, 95.f);
	}

	static void playPlayerDoubleJump()
	{
		playBuffer(s_playerDoubleJumpBuffer, s_hasPlayerDoubleJumpBuffer, s_playerDoubleJumpSound, 95.f);
	}

	static void playPlayerPhase()
	{
		playBuffer(s_playerPhaseBuffer, s_hasPlayerPhaseBuffer, s_playerPhaseSound, 95.f);
	}

	static void playEnemyDeath()
	{
		playBuffer(s_enemyDeathBuffer, s_hasEnemyDeathBuffer, s_enemyDeathSound, 100.f);
	}

	static void playCheckpoint()
	{
		playBuffer(s_checkpointBuffer, s_hasCheckpointBuffer, s_checkpointSound, 100.f);
	}

	static void playPickupSound()
	{
		playBuffer(s_pickupBuffer, s_hasPickupBuffer, s_pickupSound, 100.f);
	}

	static void playFootstep(GAME1_SurfaceTag surfaceTag)
	{
		switch (surfaceTag)
		{
		case GAME1_SurfaceTag::Floor:
			playFromPool(s_floorStepBuffers, s_footstepSound, 78.f);
			break;

		case GAME1_SurfaceTag::Rock:
			playFromPool(s_rockStepBuffers, s_footstepSound, 78.f);
			break;

		case GAME1_SurfaceTag::Grass:
		default:
			playFromPool(s_grassStepBuffers, s_footstepSound, 78.f);
			break;
		}
	}

	static void playWallgrab()
	{
		playFromPool(s_wallgrabBuffers, s_wallgrabSound, 78.f);
	}

	static void refreshVolumes()
	{
		if (s_hasMenuMusic)
			s_menuMusic.setVolume(ArcadeSettings::scaleMusic(kBaseMenuMusicVolume));

		if (s_hasGameplayMusic)
			s_gameplayMusic.setVolume(ArcadeSettings::scaleMusic(kBaseGameplayMusicVolume));

		if (s_hasDeathMusic)
			s_deathMusic.setVolume(ArcadeSettings::scaleMusic(kBaseDeathMusicVolume));
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

	static std::string toLower(std::string value)
	{
		std::transform(value.begin(), value.end(), value.begin(),
			[](unsigned char c)
			{
				return static_cast<char>(std::tolower(c));
			});

		return value;
	}

	static bool isWavFile(const std::filesystem::path& path)
	{
		return path.has_extension() && toLower(path.extension().string()) == ".wav";
	}

	static bool loadBuffer(sf::SoundBuffer& buffer, bool& hasBuffer, const std::string& path)
	{
		hasBuffer = buffer.loadFromFile(path);
		return hasBuffer;
	}

	static void loadSoundPoolFromDirectory(std::vector<sf::SoundBuffer>& buffers, const std::filesystem::path& directory)
	{
		namespace fs = std::filesystem;

		buffers.clear();

		if (!fs::exists(directory) || !fs::is_directory(directory))
			return;

		std::vector<fs::path> paths;

		for (const auto& entry : fs::directory_iterator(directory))
		{
			if (entry.is_regular_file() && isWavFile(entry.path()))
				paths.push_back(entry.path());
		}

		std::sort(paths.begin(), paths.end(),
			[](const fs::path& a, const fs::path& b)
			{
				return a.filename().string() < b.filename().string();
			});

		buffers.reserve(paths.size());

		for (const fs::path& path : paths)
		{
			sf::SoundBuffer buffer;

			if (buffer.loadFromFile(path.string()))
				buffers.push_back(std::move(buffer));
		}
	}

	static void playBuffer(const sf::SoundBuffer& buffer,
		bool hasBuffer,
		std::optional<sf::Sound>& sound,
		float volume)
	{
		if (!hasBuffer)
			return;

		sound.emplace(buffer);
		sound->setVolume(ArcadeSettings::scaleSfx(volume));
		sound->setPitch(1.f);
		sound->play();
	}

	static void playFromPool(const std::vector<sf::SoundBuffer>& buffers,
		std::optional<sf::Sound>& sound,
		float volume)
	{
		if (buffers.empty())
			return;

		std::uniform_int_distribution<std::size_t> distribution(0, buffers.size() - 1);
		const sf::SoundBuffer& buffer = buffers[distribution(s_rng)];

		sound.emplace(buffer);
		sound->setVolume(ArcadeSettings::scaleSfx(volume));
		sound->setPitch(1.f);
		sound->play();
	}

	static void clearSfx()
	{
		s_playerDamageSound.reset();
		s_playerDeathSound.reset();
		s_playerJumpSound.reset();
		s_playerDoubleJumpSound.reset();
		s_playerPhaseSound.reset();
		s_enemyDeathSound.reset();
		s_footstepSound.reset();
		s_wallgrabSound.reset();
		s_checkpointSound.reset();
		s_pickupSound.reset();

		s_floorStepBuffers.clear();
		s_grassStepBuffers.clear();
		s_rockStepBuffers.clear();
		s_wallgrabBuffers.clear();

		s_hasPlayerDamageBuffer = false;
		s_hasPlayerDeathBuffer = false;
		s_hasPlayerJumpBuffer = false;
		s_hasPlayerDoubleJumpBuffer = false;
		s_hasPlayerPhaseBuffer = false;
		s_hasEnemyDeathBuffer = false;
		s_hasCheckpointBuffer = false;
		s_hasPickupBuffer = false;
	}

private:
	static constexpr float kBaseMenuMusicVolume = 65.f;
	static constexpr float kBaseGameplayMusicVolume = 55.f;
	static constexpr float kBaseDeathMusicVolume = 65.f;

	inline static sf::Music s_menuMusic;
	inline static sf::Music s_gameplayMusic;
	inline static sf::Music s_deathMusic;

	inline static bool s_hasMenuMusic = false;
	inline static bool s_hasGameplayMusic = false;
	inline static bool s_hasDeathMusic = false;

	inline static sf::SoundBuffer s_playerDamageBuffer;
	inline static sf::SoundBuffer s_playerDeathBuffer;
	inline static sf::SoundBuffer s_playerJumpBuffer;
	inline static sf::SoundBuffer s_playerDoubleJumpBuffer;
	inline static sf::SoundBuffer s_playerPhaseBuffer;
	inline static sf::SoundBuffer s_enemyDeathBuffer;
	inline static sf::SoundBuffer s_checkpointBuffer;
	inline static sf::SoundBuffer s_pickupBuffer;

	inline static bool s_hasPlayerDamageBuffer = false;
	inline static bool s_hasPlayerDeathBuffer = false;
	inline static bool s_hasPlayerJumpBuffer = false;
	inline static bool s_hasPlayerDoubleJumpBuffer = false;
	inline static bool s_hasPlayerPhaseBuffer = false;
	inline static bool s_hasEnemyDeathBuffer = false;
	inline static bool s_hasCheckpointBuffer = false;
	inline static bool s_hasPickupBuffer = false;

	inline static std::vector<sf::SoundBuffer> s_floorStepBuffers;
	inline static std::vector<sf::SoundBuffer> s_grassStepBuffers;
	inline static std::vector<sf::SoundBuffer> s_rockStepBuffers;
	inline static std::vector<sf::SoundBuffer> s_wallgrabBuffers;

	inline static std::optional<sf::Sound> s_playerDamageSound;
	inline static std::optional<sf::Sound> s_playerDeathSound;
	inline static std::optional<sf::Sound> s_playerJumpSound;
	inline static std::optional<sf::Sound> s_playerDoubleJumpSound;
	inline static std::optional<sf::Sound> s_playerPhaseSound;
	inline static std::optional<sf::Sound> s_enemyDeathSound;
	inline static std::optional<sf::Sound> s_footstepSound;
	inline static std::optional<sf::Sound> s_wallgrabSound;
	inline static std::optional<sf::Sound> s_checkpointSound;
	inline static std::optional<sf::Sound> s_pickupSound;

	inline static bool s_loaded = false;
	inline static std::string s_resourcesDirectory;
	inline static Track s_activeTrack = Track::None;

	inline static std::mt19937 s_rng;
	inline static bool s_rngSeeded = false;
};
