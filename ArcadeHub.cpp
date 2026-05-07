#include "ArcadeHub.h"

#include <algorithm>
#include <cmath>
#include <cctype>
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
	const std::vector<ArcadeHubGameEntry>& games,
	const std::string& lockedImagePath)
{
	namespace fs = std::filesystem;

	m_lastError.clear();
	m_games.clear();

	m_selectedIndex = 0;
	m_previousIndex = 0;
	m_isSwiping = false;
	m_swipeTimer = 0.f;
	m_swipeDirection = 1;
	m_hasLockTexture = false;

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

	const bool hasLockedGames = std::any_of(
		games.begin(),
		games.end(),
		[](const ArcadeHubGameEntry& entry)
		{
			return entry.isLocked;
		});

	if (hasLockedGames)
	{
		if (lockedImagePath.empty())
		{
			m_lastError = "Locked games exist, but no locked image path was provided.";
			return false;
		}

		if (!m_lockTexture.loadFromFile(lockedImagePath))
		{
			m_lastError = "Failed to load locked image: " + lockedImagePath;
			return false;
		}

		m_hasLockTexture = true;
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

		bool loadedAnyTexture = false;

		if (!entry.splashFramesDirectory.empty())
		{
			const fs::path framesDirectory = entry.splashFramesDirectory;

			if (fs::exists(framesDirectory) && fs::is_directory(framesDirectory))
			{
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

				loadedAnyTexture = !card.splashFrames.empty();
			}
		}

		if (!loadedAnyTexture && !entry.splashStillImagePath.empty())
		{
			sf::Texture texture;

			if (texture.loadFromFile(entry.splashStillImagePath))
			{
				card.splashFrames.push_back(std::move(texture));
				loadedAnyTexture = true;
			}
		}

		m_games.push_back(std::move(card));
	}

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
	for (LoadedGameCard& card : m_games)
	{
		if (card.splashFrames.size() <= 1)
			continue;

		card.animationTimer += deltaTime;

		while (card.animationTimer >= m_animationFrameDuration)
		{
			card.animationTimer -= m_animationFrameDuration;
			card.currentFrameIndex = (card.currentFrameIndex + 1) % card.splashFrames.size();
		}
	}

	if (m_isSwiping)
	{
		m_swipeTimer += deltaTime;

		if (m_swipeTimer >= m_swipeDuration)
		{
			m_swipeTimer = 0.f;
			m_isSwiping = false;
		}
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
	if (!m_projectNameText || !m_clockText || !m_creditText ||
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

	const sf::Texture* selectedTexture = getCurrentTextureForGame(m_selectedIndex);

	const float maxSplashWidth = 560.f;
	const float maxSplashHeight = 340.f;
	const float splashTopY = 138.f;

	float scaledWidth = maxSplashWidth;
	float scaledHeight = maxSplashHeight;

	if (selectedTexture != nullptr)
	{
		const sf::Vector2u textureSize = selectedTexture->getSize();

		if (textureSize.x > 0 && textureSize.y > 0)
		{
			const float widthScale = maxSplashWidth / static_cast<float>(textureSize.x);
			const float heightScale = maxSplashHeight / static_cast<float>(textureSize.y);
			const float chosenScale = std::min(widthScale, heightScale);

			scaledWidth = static_cast<float>(textureSize.x) * chosenScale;
			scaledHeight = static_cast<float>(textureSize.y) * chosenScale;
		}
	}

	m_splashButtonBounds = sf::FloatRect(
		{ (windowWidth - scaledWidth) * 0.5f, splashTopY },
		{ scaledWidth, scaledHeight }
	);

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
	if (m_games.size() <= 1 || m_isSwiping)
		return;

	m_previousIndex = m_selectedIndex;

	if (m_selectedIndex == 0)
		m_selectedIndex = m_games.size() - 1;
	else
		--m_selectedIndex;

	m_swipeDirection = -1;
	m_swipeTimer = 0.f;
	m_isSwiping = true;
}

void ArcadeHub::navigateRight()
{
	if (m_games.size() <= 1 || m_isSwiping)
		return;

	m_previousIndex = m_selectedIndex;
	m_selectedIndex = (m_selectedIndex + 1) % m_games.size();

	m_swipeDirection = 1;
	m_swipeTimer = 0.f;
	m_isSwiping = true;
}

ArcadeHubAction ArcadeHub::handleClick(sf::Vector2f mousePosition) const
{
	if (m_isSwiping)
		return ArcadeHubAction::None;

	if (containsPoint(m_leftButtonBounds, mousePosition))
		return ArcadeHubAction::PreviousGame;

	if (containsPoint(m_rightButtonBounds, mousePosition))
		return ArcadeHubAction::NextGame;

	if (containsPoint(m_splashButtonBounds, mousePosition))
		return ArcadeHubAction::LaunchGame;

	return ArcadeHubAction::None;
}

void ArcadeHub::drawGameCard(sf::RenderTarget& target, std::size_t gameIndex, float offsetX) const
{
	if (gameIndex >= m_games.size())
		return;

	const LoadedGameCard& card = m_games[gameIndex];
	const sf::Texture* texture = getCurrentTextureForGame(gameIndex);

	const float windowWidth = static_cast<float>(m_lastLayoutSize.x);

	const float maxSplashWidth = 560.f;
	const float maxSplashHeight = 340.f;
	const float splashTopY = m_splashButtonBounds.position.y;

	float scaledWidth = maxSplashWidth;
	float scaledHeight = maxSplashHeight;

	if (texture != nullptr)
	{
		const sf::Vector2u textureSize = texture->getSize();

		if (textureSize.x > 0 && textureSize.y > 0)
		{
			const float widthScale = maxSplashWidth / static_cast<float>(textureSize.x);
			const float heightScale = maxSplashHeight / static_cast<float>(textureSize.y);
			const float chosenScale = std::min(widthScale, heightScale);

			scaledWidth = static_cast<float>(textureSize.x) * chosenScale;
			scaledHeight = static_cast<float>(textureSize.y) * chosenScale;
		}
	}

	const float panelCenterX = windowWidth * 0.5f + offsetX;

	const sf::FloatRect panelBounds(
		{ panelCenterX - scaledWidth * 0.5f, splashTopY },
		{ scaledWidth, scaledHeight }
	);

	sf::Text labelText(m_font);
	labelText.setString(card.data.label);
	labelText.setCharacterSize(54);
	labelText.setFillColor(card.data.isLocked ? sf::Color(180, 180, 180) : m_themeBright);
	labelText.setOutlineColor(sf::Color::Black);
	labelText.setOutlineThickness(2.f);

	{
		const sf::FloatRect bounds = labelText.getLocalBounds();
		labelText.setPosition({
			panelBounds.position.x + (panelBounds.size.x - bounds.size.x) * 0.5f - bounds.position.x,
			panelBounds.position.y - 62.f - bounds.position.y
			});
	}

	sf::Text nameText(m_font);
	nameText.setString(card.data.displayName);
	nameText.setCharacterSize(28);
	nameText.setFillColor(card.data.isLocked ? sf::Color(150, 150, 150) : m_themeMid);
	nameText.setOutlineColor(sf::Color::Black);
	nameText.setOutlineThickness(2.f);

	{
		const sf::FloatRect bounds = nameText.getLocalBounds();
		nameText.setPosition({
			panelBounds.position.x + (panelBounds.size.x - bounds.size.x) * 0.5f - bounds.position.x,
			panelBounds.position.y + panelBounds.size.y + 18.f - bounds.position.y
			});
	}

	sf::Text launchHintText(m_font);
	launchHintText.setString(
		card.data.isLocked
		? "[ LOCKED - NOT AVAILABLE YET ]"
		: "[ CLICK IMAGE OR PRESS ENTER TO LAUNCH ]"
	);
	launchHintText.setCharacterSize(22);
	launchHintText.setFillColor(card.data.isLocked ? sf::Color(140, 140, 140) : m_themeMid);
	launchHintText.setOutlineColor(sf::Color::Black);
	launchHintText.setOutlineThickness(1.f);

	{
		const sf::FloatRect bounds = launchHintText.getLocalBounds();
		launchHintText.setPosition({
			panelBounds.position.x + (panelBounds.size.x - bounds.size.x) * 0.5f - bounds.position.x,
			panelBounds.position.y + panelBounds.size.y + 50.f - bounds.position.y
			});
	}

	sf::RectangleShape splashPanel;
	splashPanel.setPosition({
		panelBounds.position.x - 12.f,
		panelBounds.position.y - 12.f
		});
	splashPanel.setSize({
		panelBounds.size.x + 24.f,
		panelBounds.size.y + 24.f
		});
	splashPanel.setFillColor(m_themeDark);
	splashPanel.setOutlineColor(card.data.isLocked ? sf::Color(130, 130, 130) : m_themeBright);
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
	splashHeaderLine.setFillColor(card.data.isLocked ? sf::Color(100, 100, 100) : m_themeMid);
	target.draw(splashHeaderLine);

	if (texture != nullptr)
	{
		sf::Sprite splashSprite(*texture);

		const sf::FloatRect localBounds = splashSprite.getLocalBounds();
		if (localBounds.size.x > 0.f && localBounds.size.y > 0.f)
		{
			const float widthScale = panelBounds.size.x / localBounds.size.x;
			const float heightScale = panelBounds.size.y / localBounds.size.y;
			const float chosenScale = std::min(widthScale, heightScale);

			const float drawWidth = localBounds.size.x * chosenScale;
			const float drawHeight = localBounds.size.y * chosenScale;

			splashSprite.setScale({ chosenScale, chosenScale });
			splashSprite.setPosition({
				panelBounds.position.x + (panelBounds.size.x - drawWidth) * 0.5f,
				panelBounds.position.y + (panelBounds.size.y - drawHeight) * 0.5f
				});

			target.draw(splashSprite);
		}
	}
	else
	{
		sf::Text placeholderText(m_font);
		placeholderText.setString("SPLASH IMAGE\nCOMING SOON");
		placeholderText.setCharacterSize(34);
		placeholderText.setFillColor(m_themeMid);
		placeholderText.setOutlineColor(sf::Color::Black);
		placeholderText.setOutlineThickness(2.f);

		const sf::FloatRect bounds = placeholderText.getLocalBounds();
		placeholderText.setPosition({
			panelBounds.position.x + (panelBounds.size.x - bounds.size.x) * 0.5f - bounds.position.x,
			panelBounds.position.y + (panelBounds.size.y - bounds.size.y) * 0.5f - bounds.position.y
			});

		target.draw(placeholderText);
	}

	if (card.data.isLocked)
	{
		sf::RectangleShape lockedOverlay;
		lockedOverlay.setPosition(panelBounds.position);
		lockedOverlay.setSize(panelBounds.size);
		lockedOverlay.setFillColor(sf::Color(0, 0, 0, 115));
		target.draw(lockedOverlay);

		if (m_hasLockTexture)
		{
			sf::Sprite lockSprite(m_lockTexture);

			const sf::FloatRect lockBounds = lockSprite.getLocalBounds();
			if (lockBounds.size.x > 0.f && lockBounds.size.y > 0.f)
			{
				lockSprite.setScale({
					panelBounds.size.x / lockBounds.size.x,
					panelBounds.size.y / lockBounds.size.y
					});

				lockSprite.setPosition({
					panelBounds.position.x,
					panelBounds.position.y
					});

				lockSprite.setColor(sf::Color(255, 255, 255, 235));
				target.draw(lockSprite);
			}
		}
	}

	target.draw(labelText);
	target.draw(nameText);
	target.draw(launchHintText);
}

const sf::Texture* ArcadeHub::getCurrentTextureForGame(std::size_t gameIndex) const
{
	if (gameIndex >= m_games.size())
		return nullptr;

	const LoadedGameCard& card = m_games[gameIndex];

	if (card.splashFrames.empty())
		return nullptr;

	return &card.splashFrames[card.currentFrameIndex % card.splashFrames.size()];
}

void ArcadeHub::draw(sf::RenderTarget& target) const
{
	if (!m_projectNameText || !m_clockText || !m_creditText ||
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
		static_cast<std::uint8_t>(std::clamp(m_themeBright.r * 0.55f, 0.f, 255.f)),
		static_cast<std::uint8_t>(std::clamp(m_themeBright.g * 0.55f, 0.f, 255.f)),
		static_cast<std::uint8_t>(std::clamp(m_themeBright.b * 0.55f, 0.f, 255.f))
	));
	innerFrame.setOutlineThickness(1.f);
	target.draw(innerFrame);

	if (!m_isSwiping)
	{
		drawGameCard(target, m_selectedIndex, 0.f);
	}
	else
	{
		const float linearT = std::clamp(m_swipeTimer / m_swipeDuration, 0.f, 1.f);
		const float curvedT = ApplySwipeCurve(linearT);

		const float travelDistance = width;
		const float outgoingOffset = -static_cast<float>(m_swipeDirection) * curvedT * travelDistance;
		const float incomingOffset = static_cast<float>(m_swipeDirection) * (1.f - curvedT) * travelDistance;

		drawGameCard(target, m_previousIndex, outgoingOffset);
		drawGameCard(target, m_selectedIndex, incomingOffset);
	}

	const float dotRadius = 7.f;
	const float dotGap = 16.f;
	const float totalDotsWidth = m_games.empty()
		? 0.f
		: static_cast<float>(m_games.size()) * (dotRadius * 2.f) + static_cast<float>(m_games.size() - 1) * dotGap;

	const float dotsStartX = (width - totalDotsWidth) * 0.5f;
	const float dotsY = m_splashButtonBounds.position.y + m_splashButtonBounds.size.y + 108.f;

	for (std::size_t i = 0; i < m_games.size(); ++i)
	{
		sf::CircleShape dot(dotRadius);
		dot.setPosition({
			dotsStartX + static_cast<float>(i) * (dotRadius * 2.f + dotGap),
			dotsY
			});

		if (m_games[i].data.isLocked)
		{
			if (i == m_selectedIndex)
				dot.setFillColor(sf::Color(150, 150, 150));
			else
				dot.setFillColor(sf::Color(70, 70, 70));

			dot.setOutlineColor(sf::Color(20, 20, 20));
			dot.setOutlineThickness(2.f);
		}
		else if (i == m_selectedIndex)
		{
			dot.setFillColor(m_themeBright);
		}
		else
		{
			dot.setFillColor(sf::Color(110, 110, 110));
		}

		target.draw(dot);
	}

	target.draw(*m_projectNameText);
	target.draw(*m_clockText);
	target.draw(*m_creditText);
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

bool ArcadeHub::isTransitioning() const
{
	return m_isSwiping;
}

bool ArcadeHub::isGameLocked(std::size_t gameIndex) const
{
	if (gameIndex >= m_games.size())
		return true;

	return m_games[gameIndex].data.isLocked;
}

bool ArcadeHub::isSelectedGameLocked() const
{
	return isGameLocked(m_selectedIndex);
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
		static_cast<std::uint8_t>(std::clamp((r1 + m) * 255.f, 0.f, 255.f)),
		static_cast<std::uint8_t>(std::clamp((g1 + m) * 255.f, 0.f, 255.f)),
		static_cast<std::uint8_t>(std::clamp((b1 + m) * 255.f, 0.f, 255.f))
	);
}

float ArcadeHub::ApplySwipeCurve(float t)
{
	t = std::clamp(t, 0.f, 1.f);

	if (t < 0.22f)
	{
		const float local = t / 0.22f;
		return local * local * 0.18f;
	}
	else if (t < 0.72f)
	{
		const float local = (t - 0.22f) / 0.50f;
		return 0.18f + local * 0.62f;
	}
	else
	{
		const float local = (t - 0.72f) / 0.28f;
		return 0.80f + (1.f - (1.f - local) * (1.f - local)) * 0.20f;
	}
}