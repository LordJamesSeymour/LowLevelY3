#include "ArcadeHub.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <sstream>

namespace
{
	std::string BuildCurrentClockText()
	{
		const std::time_t now = std::time(nullptr);

		std::tm localTime{};
#if defined(_WIN32)
		localtime_s(&localTime, &now);
#else
		localtime_r(&now, &localTime);
#endif

		std::ostringstream stream;
		stream << std::setw(2) << std::setfill('0') << localTime.tm_hour
			<< ":"
			<< std::setw(2) << std::setfill('0') << localTime.tm_min;

		return stream.str();
	}

	sf::FloatRect ExpandRect(const sf::FloatRect& rect, float padding)
	{
		return sf::FloatRect(
			{ rect.position.x - padding, rect.position.y - padding },
			{ rect.size.x + padding * 2.f, rect.size.y + padding * 2.f }
		);
	}

	std::uint8_t FloatToByte(float value)
	{
		return static_cast<std::uint8_t>(std::clamp(value, 0.f, 255.f));
	}

	std::string ToLower(std::string value)
	{
		std::transform(value.begin(), value.end(), value.begin(),
			[](unsigned char c)
			{
				return static_cast<char>(std::tolower(c));
			});

		return value;
	}

	bool IsPngFile(const std::filesystem::path& path)
	{
		return path.has_extension() && ToLower(path.extension().string()) == ".png";
	}
}

bool ArcadeHub::load(const std::string& fontPath,
	const std::string& projectName,
	const std::vector<ArcadeHubGameEntry>& games)
{
	namespace fs = std::filesystem;

	m_lastError.clear();
	m_games.clear();
	m_selectedIndex = 0;
	m_currentFrameIndex = 0;
	m_animationTimer = 0.f;

	if (!m_font.openFromFile(fontPath))
	{
		m_lastError = "Failed to load hub font: " + fontPath;
		return false;
	}

	if (games.empty())
	{
		m_lastError = "Arcade hub has no games configured.";
		return false;
	}

	m_projectNameText.emplace(m_font);
	m_projectNameText->setString(projectName);
	m_projectNameText->setCharacterSize(40);
	m_projectNameText->setOutlineColor(sf::Color::Black);
	m_projectNameText->setOutlineThickness(2.f);

	m_clockText.emplace(m_font);
	m_clockText->setCharacterSize(40);
	m_clockText->setOutlineColor(sf::Color::Black);
	m_clockText->setOutlineThickness(2.f);

	m_creditText.emplace(m_font);
	m_creditText->setString("CREDIT: 0$");
	m_creditText->setCharacterSize(36);
	m_creditText->setOutlineColor(sf::Color::Black);
	m_creditText->setOutlineThickness(2.f);

	m_gameLabelText.emplace(m_font);
	m_gameLabelText->setCharacterSize(54);
	m_gameLabelText->setOutlineColor(sf::Color::Black);
	m_gameLabelText->setOutlineThickness(2.f);

	m_gameNameText.emplace(m_font);
	m_gameNameText->setCharacterSize(28);
	m_gameNameText->setOutlineColor(sf::Color::Black);
	m_gameNameText->setOutlineThickness(2.f);

	m_launchHintText.emplace(m_font);
	m_launchHintText->setString("[ CLICK IMAGE OR PRESS ENTER TO LAUNCH ]");
	m_launchHintText->setCharacterSize(22);
	m_launchHintText->setOutlineColor(sf::Color::Black);
	m_launchHintText->setOutlineThickness(1.f);

	m_leftArrowText.emplace(m_font);
	m_leftArrowText->setString("<");
	m_leftArrowText->setCharacterSize(120);
	m_leftArrowText->setOutlineColor(sf::Color::Black);
	m_leftArrowText->setOutlineThickness(2.f);

	m_rightArrowText.emplace(m_font);
	m_rightArrowText->setString(">");
	m_rightArrowText->setCharacterSize(120);
	m_rightArrowText->setOutlineColor(sf::Color::Black);
	m_rightArrowText->setOutlineThickness(2.f);

	m_games.reserve(games.size());

	for (const ArcadeHubGameEntry& entry : games)
	{
		LoadedGameCard card;
		card.data = entry;

		const fs::path framesDirectory = entry.splashFramesDirectory;

		if (!fs::exists(framesDirectory) || !fs::is_directory(framesDirectory))
		{
			m_lastError = "Failed to open splash frames directory: " + framesDirectory.string();
			return false;
		}

		std::vector<fs::path> framePaths;

		for (const auto& fileEntry : fs::directory_iterator(framesDirectory))
		{
			if (fileEntry.is_regular_file() && IsPngFile(fileEntry.path()))
			{
				framePaths.push_back(fileEntry.path());
			}
		}

		std::sort(framePaths.begin(), framePaths.end(),
			[](const fs::path& a, const fs::path& b)
			{
				return a.filename().string() < b.filename().string();
			});

		if (framePaths.empty())
		{
			m_lastError = "No PNG splash frames found in: " + framesDirectory.string();
			return false;
		}

		card.splashFrames.reserve(framePaths.size());

		for (const fs::path& framePath : framePaths)
		{
			sf::Texture texture;
			if (!texture.loadFromFile(framePath.string()))
			{
				m_lastError = "Failed to load splash frame: " + framePath.string();
				return false;
			}

			card.splashFrames.push_back(std::move(texture));
		}

		m_games.push_back(std::move(card));
	}

	updateDisplayedTexts();
	updateClockText();
	updateVisualTheme(0.f);
	return true;
}

void ArcadeHub::updateClockText()
{
	if (m_clockText)
	{
		m_clockText->setString(BuildCurrentClockText());
	}
}

void ArcadeHub::updateAnimation(float deltaTime)
{
	if (m_games.empty())
		return;

	const LoadedGameCard& selectedCard = m_games[m_selectedIndex];

	if (selectedCard.splashFrames.size() <= 1)
		return;

	m_animationTimer += deltaTime;

	while (m_animationTimer >= m_animationFrameDuration)
	{
		m_animationTimer -= m_animationFrameDuration;
		m_currentFrameIndex = (m_currentFrameIndex + 1) % selectedCard.splashFrames.size();
	}
}

void ArcadeHub::updateVisualTheme(float totalTimeSeconds)
{
	const float hue = std::fmod(totalTimeSeconds * 8.f, 360.f);

	m_themeBright = ColorFromHSV(hue, 0.60f, 1.00f);
	m_themeMid = ColorFromHSV(hue + 25.f, 0.45f, 0.90f);
	m_themeDark = ColorFromHSV(hue + 170.f, 0.30f, 0.20f);
	m_themeBackground = ColorFromHSV(hue + 210.f, 0.28f, 0.10f);

	if (m_projectNameText) m_projectNameText->setFillColor(m_themeBright);
	if (m_clockText) m_clockText->setFillColor(m_themeBright);
	if (m_creditText) m_creditText->setFillColor(m_themeBright);
	if (m_gameLabelText) m_gameLabelText->setFillColor(m_themeBright);
	if (m_gameNameText) m_gameNameText->setFillColor(m_themeMid);
	if (m_launchHintText) m_launchHintText->setFillColor(m_themeMid);

	if (m_leftArrowText && m_rightArrowText)
	{
		if (m_games.size() > 1)
		{
			m_leftArrowText->setFillColor(m_themeBright);
			m_rightArrowText->setFillColor(m_themeBright);
		}
		else
		{
			const sf::Color subdued(
				FloatToByte(m_themeMid.r * 0.45f),
				FloatToByte(m_themeMid.g * 0.45f),
				FloatToByte(m_themeMid.b * 0.45f)
			);

			m_leftArrowText->setFillColor(subdued);
			m_rightArrowText->setFillColor(subdued);
		}
	}
}

void ArcadeHub::layout(const sf::RenderWindow& window)
{
	if (m_games.empty() || !m_projectNameText || !m_clockText || !m_creditText ||
		!m_gameLabelText || !m_gameNameText || !m_launchHintText ||
		!m_leftArrowText || !m_rightArrowText)
	{
		return;
	}

	m_lastLayoutSize = window.getSize();

	const float windowWidth = static_cast<float>(m_lastLayoutSize.x);
	const float windowHeight = static_cast<float>(m_lastLayoutSize.y);

	{
		const sf::FloatRect bounds = m_projectNameText->getLocalBounds();
		m_projectNameText->setPosition({
			20.f - bounds.position.x,
			16.f - bounds.position.y
			});
	}

	{
		const sf::FloatRect bounds = m_clockText->getLocalBounds();
		m_clockText->setPosition({
			windowWidth - bounds.size.x - 20.f - bounds.position.x,
			16.f - bounds.position.y
			});
	}

	{
		const sf::FloatRect bounds = m_creditText->getLocalBounds();
		m_creditText->setPosition({
			20.f - bounds.position.x,
			windowHeight - bounds.size.y - 18.f - bounds.position.y
			});
	}

	const LoadedGameCard& selectedCard = m_games[m_selectedIndex];
	const sf::Texture& currentTexture = selectedCard.splashFrames[m_currentFrameIndex % selectedCard.splashFrames.size()];
	const sf::Vector2u textureSize = currentTexture.getSize();

	const float targetWidth = 560.f;
	const float targetHeight = 320.f;

	float scaledWidth = targetWidth;
	float scaledHeight = targetHeight;

	if (textureSize.x > 0 && textureSize.y > 0)
	{
		const float widthScale = targetWidth / static_cast<float>(textureSize.x);
		const float heightScale = targetHeight / static_cast<float>(textureSize.y);
		const float chosenScale = std::min(widthScale, heightScale);

		scaledWidth = static_cast<float>(textureSize.x) * chosenScale;
		scaledHeight = static_cast<float>(textureSize.y) * chosenScale;
	}

	m_splashButtonBounds = sf::FloatRect(
		{ (windowWidth - scaledWidth) * 0.5f, (windowHeight - scaledHeight) * 0.5f - 10.f },
		{ scaledWidth, scaledHeight }
	);

	{
		const sf::FloatRect bounds = m_gameLabelText->getLocalBounds();
		m_gameLabelText->setPosition({
			(windowWidth - bounds.size.x) * 0.5f - bounds.position.x,
			m_splashButtonBounds.position.y - 62.f - bounds.position.y
			});
	}

	{
		const sf::FloatRect bounds = m_gameNameText->getLocalBounds();
		m_gameNameText->setPosition({
			(windowWidth - bounds.size.x) * 0.5f - bounds.position.x,
			m_splashButtonBounds.position.y + m_splashButtonBounds.size.y + 18.f - bounds.position.y
			});
	}

	{
		const sf::FloatRect bounds = m_launchHintText->getLocalBounds();
		m_launchHintText->setPosition({
			(windowWidth - bounds.size.x) * 0.5f - bounds.position.x,
			m_splashButtonBounds.position.y + m_splashButtonBounds.size.y + 54.f - bounds.position.y
			});
	}

	const float splashCenterY = m_splashButtonBounds.position.y + m_splashButtonBounds.size.y * 0.5f;

	{
		const sf::FloatRect bounds = m_leftArrowText->getLocalBounds();
		m_leftArrowText->setPosition({
			m_splashButtonBounds.position.x - 120.f - bounds.size.x - bounds.position.x,
			splashCenterY - bounds.size.y * 0.5f - bounds.position.y
			});

		m_leftButtonBounds = ExpandRect(m_leftArrowText->getGlobalBounds(), 20.f);
	}

	{
		const sf::FloatRect bounds = m_rightArrowText->getLocalBounds();
		m_rightArrowText->setPosition({
			m_splashButtonBounds.position.x + m_splashButtonBounds.size.x + 120.f - bounds.position.x,
			splashCenterY - bounds.size.y * 0.5f - bounds.position.y
			});

		m_rightButtonBounds = ExpandRect(m_rightArrowText->getGlobalBounds(), 20.f);
	}
}

void ArcadeHub::navigateLeft()
{
	if (m_games.empty())
		return;

	if (m_selectedIndex == 0)
		m_selectedIndex = m_games.size() - 1;
	else
		--m_selectedIndex;

	updateDisplayedTexts();
}

void ArcadeHub::navigateRight()
{
	if (m_games.empty())
		return;

	m_selectedIndex = (m_selectedIndex + 1) % m_games.size();
	updateDisplayedTexts();
}

ArcadeHubAction ArcadeHub::handleClick(sf::Vector2f mousePosition) const
{
	if (containsPoint(m_leftButtonBounds, mousePosition))
		return ArcadeHubAction::PreviousGame;

	if (containsPoint(m_rightButtonBounds, mousePosition))
		return ArcadeHubAction::NextGame;

	if (containsPoint(m_splashButtonBounds, mousePosition))
		return ArcadeHubAction::LaunchGame;

	return ArcadeHubAction::None;
}

void ArcadeHub::draw(sf::RenderTarget& target) const
{
	if (m_games.empty() || !m_projectNameText || !m_clockText || !m_creditText ||
		!m_gameLabelText || !m_gameNameText || !m_launchHintText ||
		!m_leftArrowText || !m_rightArrowText)
	{
		return;
	}

	const float width = static_cast<float>(m_lastLayoutSize.x);
	const float height = static_cast<float>(m_lastLayoutSize.y);

	sf::RectangleShape background;
	background.setPosition({ 0.f, 0.f });
	background.setSize({ width, height });
	background.setFillColor(m_themeBackground);
	target.draw(background);

	sf::RectangleShape screenFrame;
	screenFrame.setPosition({ 8.f, 8.f });
	screenFrame.setSize({ width - 16.f, height - 16.f });
	screenFrame.setFillColor(sf::Color::Transparent);
	screenFrame.setOutlineColor(m_themeMid);
	screenFrame.setOutlineThickness(3.f);
	target.draw(screenFrame);

	sf::RectangleShape innerFrame;
	innerFrame.setPosition({ 18.f, 18.f });
	innerFrame.setSize({ width - 36.f, height - 36.f });
	innerFrame.setFillColor(sf::Color::Transparent);
	innerFrame.setOutlineColor(sf::Color(
		FloatToByte(m_themeBright.r * 0.55f),
		FloatToByte(m_themeBright.g * 0.55f),
		FloatToByte(m_themeBright.b * 0.55f)
	));
	innerFrame.setOutlineThickness(1.f);
	target.draw(innerFrame);

	sf::RectangleShape splashPanel;
	splashPanel.setPosition({
		m_splashButtonBounds.position.x - 12.f,
		m_splashButtonBounds.position.y - 12.f
		});
	splashPanel.setSize({
		m_splashButtonBounds.size.x + 24.f,
		m_splashButtonBounds.size.y + 24.f
		});
	splashPanel.setFillColor(m_themeDark);
	splashPanel.setOutlineColor(m_themeBright);
	splashPanel.setOutlineThickness(3.f);
	target.draw(splashPanel);

	sf::RectangleShape splashHeaderLine;
	splashHeaderLine.setPosition({
		splashPanel.getPosition().x + 10.f,
		splashPanel.getPosition().y + 10.f
		});
	splashHeaderLine.setSize({
		splashPanel.getSize().x - 20.f,
		3.f
		});
	splashHeaderLine.setFillColor(m_themeMid);
	target.draw(splashHeaderLine);

	const LoadedGameCard& selectedCard = m_games[m_selectedIndex];
	const sf::Texture& currentTexture = selectedCard.splashFrames[m_currentFrameIndex % selectedCard.splashFrames.size()];

	sf::Sprite splashSprite(currentTexture);

	const sf::FloatRect localBounds = splashSprite.getLocalBounds();
	if (localBounds.size.x > 0.f && localBounds.size.y > 0.f)
	{
		splashSprite.setScale({
			m_splashButtonBounds.size.x / localBounds.size.x,
			m_splashButtonBounds.size.y / localBounds.size.y
			});

		splashSprite.setPosition(m_splashButtonBounds.position);
		target.draw(splashSprite);
	}

	target.draw(*m_projectNameText);
	target.draw(*m_clockText);
	target.draw(*m_creditText);
	target.draw(*m_gameLabelText);
	target.draw(*m_gameNameText);
	target.draw(*m_launchHintText);
	target.draw(*m_leftArrowText);
	target.draw(*m_rightArrowText);
}

std::size_t ArcadeHub::getSelectedIndex() const
{
	return m_selectedIndex;
}

const ArcadeHubGameEntry& ArcadeHub::getSelectedGame() const
{
	return m_games[m_selectedIndex].data;
}

const std::string& ArcadeHub::getLastError() const
{
	return m_lastError;
}

bool ArcadeHub::containsPoint(const sf::FloatRect& bounds, sf::Vector2f point)
{
	return point.x >= bounds.position.x &&
		point.x <= bounds.position.x + bounds.size.x &&
		point.y >= bounds.position.y &&
		point.y <= bounds.position.y + bounds.size.y;
}

sf::Color ArcadeHub::ColorFromHSV(float hueDegrees, float saturation, float value)
{
	while (hueDegrees < 0.f)
		hueDegrees += 360.f;

	hueDegrees = std::fmod(hueDegrees, 360.f);
	saturation = std::clamp(saturation, 0.f, 1.f);
	value = std::clamp(value, 0.f, 1.f);

	const float chroma = value * saturation;
	const float hueSection = hueDegrees / 60.f;
	const float x = chroma * (1.f - std::fabs(std::fmod(hueSection, 2.f) - 1.f));
	const float m = value - chroma;

	float r1 = 0.f;
	float g1 = 0.f;
	float b1 = 0.f;

	if (hueSection >= 0.f && hueSection < 1.f)
	{
		r1 = chroma; g1 = x; b1 = 0.f;
	}
	else if (hueSection < 2.f)
	{
		r1 = x; g1 = chroma; b1 = 0.f;
	}
	else if (hueSection < 3.f)
	{
		r1 = 0.f; g1 = chroma; b1 = x;
	}
	else if (hueSection < 4.f)
	{
		r1 = 0.f; g1 = x; b1 = chroma;
	}
	else if (hueSection < 5.f)
	{
		r1 = x; g1 = 0.f; b1 = chroma;
	}
	else
	{
		r1 = chroma; g1 = 0.f; b1 = x;
	}

	return sf::Color(
		FloatToByte((r1 + m) * 255.f),
		FloatToByte((g1 + m) * 255.f),
		FloatToByte((b1 + m) * 255.f)
	);
}

void ArcadeHub::updateDisplayedTexts()
{
	if (m_games.empty() || !m_gameLabelText || !m_gameNameText)
		return;

	const LoadedGameCard& selectedCard = m_games[m_selectedIndex];

	m_gameLabelText->setString(selectedCard.data.label);
	m_gameNameText->setString(selectedCard.data.displayName);

	m_currentFrameIndex = 0;
	m_animationTimer = 0.f;
}