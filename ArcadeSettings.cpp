#include "ArcadeSettings.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

namespace
{
	bool s_loaded = false;
	std::string s_filePath = "settings.cfg";

	bool s_crtEnabled = true;
	float s_musicVolume = ArcadeSettings::kMaxVolume;
	float s_sfxVolume = ArcadeSettings::kMaxVolume;

	std::vector<ArcadeSettings::AudioRefreshCallback> s_callbacks;

	std::string trim(const std::string& value)
	{
		const std::size_t first = value.find_first_not_of(" \t\r\n");
		if (first == std::string::npos)
			return {};

		const std::size_t last = value.find_last_not_of(" \t\r\n");
		return value.substr(first, last - first + 1);
	}

	std::string toLower(std::string value)
	{
		std::transform(value.begin(), value.end(), value.begin(),
			[](unsigned char c)
			{
				return static_cast<char>(std::tolower(c));
			});

		return value;
	}

	bool parseBool(const std::string& value)
	{
		const std::string v = toLower(trim(value));
		return v == "1" || v == "true" || v == "yes" || v == "on";
	}

	float clampVolume(float value)
	{
		return std::clamp(value, 0.f, ArcadeSettings::kMaxVolume);
	}

	float roundToStep(float value)
	{
		return std::round(value / ArcadeSettings::kVolumeStep) * ArcadeSettings::kVolumeStep;
	}

	void clampStoredValues()
	{
		s_musicVolume = clampVolume(roundToStep(s_musicVolume));
		s_sfxVolume = clampVolume(roundToStep(s_sfxVolume));
	}
}

void ArcadeSettings::initialize(const std::string& filePath)
{
	s_filePath = filePath;
	s_loaded = true;

	std::ifstream stream(filePath);
	if (!stream.is_open())
	{
		s_crtEnabled = true;
		s_musicVolume = kMaxVolume;
		s_sfxVolume = kMaxVolume;
		save();
		return;
	}

	std::string line;
	while (std::getline(stream, line))
	{
		const std::size_t equals = line.find('=');
		if (equals == std::string::npos)
			continue;

		const std::string key = trim(line.substr(0, equals));
		const std::string value = trim(line.substr(equals + 1));

		if (key == "crt_shader_enabled")
		{
			s_crtEnabled = parseBool(value);
		}
		else if (key == "music_volume")
		{
			try { s_musicVolume = std::stof(value); }
			catch (...) { s_musicVolume = kMaxVolume; }
		}
		else if (key == "sfx_volume")
		{
			try { s_sfxVolume = std::stof(value); }
			catch (...) { s_sfxVolume = kMaxVolume; }
		}
	}

	clampStoredValues();
}

bool ArcadeSettings::save()
{
	std::ofstream stream(s_filePath, std::ios::trunc);
	if (!stream.is_open())
		return false;

	stream << "crt_shader_enabled=" << (s_crtEnabled ? 1 : 0) << "\n";

	stream << std::fixed;
	stream.precision(1);
	stream << "music_volume=" << s_musicVolume << "\n";
	stream << "sfx_volume=" << s_sfxVolume << "\n";

	return true;
}

bool ArcadeSettings::isCrtEnabled()
{
	return s_crtEnabled;
}

void ArcadeSettings::setCrtEnabled(bool enabled)
{
	if (s_crtEnabled == enabled)
		return;

	s_crtEnabled = enabled;
	save();
}

float ArcadeSettings::getMusicVolume()
{
	return s_musicVolume;
}

void ArcadeSettings::setMusicVolume(float value)
{
	value = clampVolume(roundToStep(value));

	if (s_musicVolume == value)
		return;

	s_musicVolume = value;
	notifyAudioChanged();
	save();
}

float ArcadeSettings::getSfxVolume()
{
	return s_sfxVolume;
}

void ArcadeSettings::setSfxVolume(float value)
{
	value = clampVolume(roundToStep(value));

	if (s_sfxVolume == value)
		return;

	s_sfxVolume = value;
	notifyAudioChanged();
	save();
}

float ArcadeSettings::scaleMusic(float baseVolume)
{
	return std::clamp(baseVolume * (s_musicVolume / kMaxVolume), 0.f, 100.f);
}

float ArcadeSettings::scaleSfx(float baseVolume)
{
	return std::clamp(baseVolume * (s_sfxVolume / kMaxVolume), 0.f, 100.f);
}

void ArcadeSettings::registerAudioRefreshCallback(AudioRefreshCallback callback)
{
	if (callback)
		s_callbacks.push_back(std::move(callback));
}

void ArcadeSettings::notifyAudioChanged()
{
	for (const auto& cb : s_callbacks)
	{
		if (cb)
			cb();
	}
}
