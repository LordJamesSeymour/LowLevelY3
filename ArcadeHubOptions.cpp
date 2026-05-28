#include "ArcadeHubOptions.h"

#include "ArcadeInput.h"
#include "ArcadeSettings.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <sstream>

namespace
{
	std::string formatVolume(float value)
	{
		std::ostringstream stream;
		stream << std::fixed << std::setprecision(1) << value;
		return stream.str();
	}
}

bool ArcadeHubOptions::load(const std::string& fontPath,
	const std::string& settingsIconPath,
	const std::string& volumeOnIconPath,
	const std::string& volumeOffIconPath)
{
	m_lastError.clear();
	m_hasSettingsIcon = false;
	m_hasVolumeOnIcon = false;
	m_hasVolumeOffIcon = false;

	if (!m_font.openFromFile(fontPath))
	{
		m_lastError = "Failed to load options font: " + fontPath;
		return false;
	}

	if (!settingsIconPath.empty())
	{
		if (m_settingsIconTexture.loadFromFile(settingsIconPath))
		{
			m_hasSettingsIcon = true;
			m_settingsIconTexture.setSmooth(true);
		}
	}

	if (!volumeOnIconPath.empty())
	{
		if (m_volumeOnTexture.loadFromFile(volumeOnIconPath))
		{
			m_hasVolumeOnIcon = true;
			m_volumeOnTexture.setSmooth(true);
		}
	}

	if (!volumeOffIconPath.empty())
	{
		if (m_volumeOffTexture.loadFromFile(volumeOffIconPath))
		{
			m_hasVolumeOffIcon = true;
			m_volumeOffTexture.setSmooth(true);
		}
	}

	m_musicMuteAnimT = ArcadeSettings::isMusicMuted() ? 1.f : 0.f;
	m_sfxMuteAnimT = ArcadeSettings::isSfxMuted() ? 1.f : 0.f;

	return true;
}

void ArcadeHubOptions::layout(const sf::RenderWindow& window)
{
	m_lastLayoutSize = window.getSize();
	const float width = static_cast<float>(m_lastLayoutSize.x);
	const float height = static_cast<float>(m_lastLayoutSize.y);

	const float gearSize = 56.f;
	const float gearMargin = 24.f;

	m_gearBounds = sf::FloatRect(
		{ width - gearSize - gearMargin, height - gearSize - gearMargin },
		{ gearSize, gearSize }
	);

	const float panelWidth = std::min(640.f, std::max(420.f, width - 120.f));
	const float panelHeight = std::min(460.f, std::max(360.f, height - 120.f));
	const float panelX = (width - panelWidth) * 0.5f;
	const float panelY = (height - panelHeight) * 0.5f;

	m_panelBounds = sf::FloatRect({ panelX, panelY }, { panelWidth, panelHeight });

	const float contentLeft = panelX + 40.f;
	const float contentRight = panelX + panelWidth - 40.f;
	const float rowHeight = 50.f;
	const float topContent = panelY + 110.f;
	const float rowGap = 22.f;

	m_crtToggleBounds = sf::FloatRect(
		{ contentLeft, topContent },
		{ contentRight - contentLeft, rowHeight }
	);

	m_musicSliderBounds = sf::FloatRect(
		{ contentLeft, topContent + rowHeight + rowGap },
		{ contentRight - contentLeft, rowHeight }
	);

	m_sfxSliderBounds = sf::FloatRect(
		{ contentLeft, topContent + (rowHeight + rowGap) * 2.f },
		{ contentRight - contentLeft, rowHeight }
	);

	const float muteButtonSize = 40.f;
	const float muteButtonOffsetY = (rowHeight - muteButtonSize) * 0.5f;

	m_musicMuteButtonBounds = sf::FloatRect(
		{ m_musicSliderBounds.position.x + m_musicSliderBounds.size.x - muteButtonSize,
		  m_musicSliderBounds.position.y + muteButtonOffsetY },
		{ muteButtonSize, muteButtonSize }
	);

	m_sfxMuteButtonBounds = sf::FloatRect(
		{ m_sfxSliderBounds.position.x + m_sfxSliderBounds.size.x - muteButtonSize,
		  m_sfxSliderBounds.position.y + muteButtonOffsetY },
		{ muteButtonSize, muteButtonSize }
	);

	const float closeWidth = 180.f;
	const float closeHeight = 48.f;
	m_closeButtonBounds = sf::FloatRect(
		{ panelX + (panelWidth - closeWidth) * 0.5f, panelY + panelHeight - closeHeight - 24.f },
		{ closeWidth, closeHeight }
	);
}

void ArcadeHubOptions::update(float deltaTime)
{
	// Animation duration ~0.15s. We move animT toward the boolean target.
	const float animSpeed = 1.f / 0.15f;
	const float step = animSpeed * deltaTime;

	const float musicTarget = ArcadeSettings::isMusicMuted() ? 1.f : 0.f;
	if (m_musicMuteAnimT < musicTarget)
		m_musicMuteAnimT = std::min(musicTarget, m_musicMuteAnimT + step);
	else if (m_musicMuteAnimT > musicTarget)
		m_musicMuteAnimT = std::max(musicTarget, m_musicMuteAnimT - step);

	const float sfxTarget = ArcadeSettings::isSfxMuted() ? 1.f : 0.f;
	if (m_sfxMuteAnimT < sfxTarget)
		m_sfxMuteAnimT = std::min(sfxTarget, m_sfxMuteAnimT + step);
	else if (m_sfxMuteAnimT > sfxTarget)
		m_sfxMuteAnimT = std::max(sfxTarget, m_sfxMuteAnimT - step);
}

bool ArcadeHubOptions::isOpen() const
{
	return m_isOpen;
}

void ArcadeHubOptions::open()
{
	m_isOpen = true;
	m_selectedRow = Row::CrtShader;
	m_draggingMusic = false;
	m_draggingSfx = false;
	ArcadeInput::consumePressedState();
}

void ArcadeHubOptions::close()
{
	m_isOpen = false;
	m_draggingMusic = false;
	m_draggingSfx = false;
	ArcadeInput::consumePressedState();
}

const sf::FloatRect& ArcadeHubOptions::getGearBounds() const
{
	return m_gearBounds;
}

bool ArcadeHubOptions::containsPoint(const sf::FloatRect& rect, sf::Vector2f point)
{
	return point.x >= rect.position.x &&
		point.x <= rect.position.x + rect.size.x &&
		point.y >= rect.position.y &&
		point.y <= rect.position.y + rect.size.y;
}

void ArcadeHubOptions::getSliderTrackEdges(const sf::FloatRect& row, float& outLeft, float& outRight) const
{
	// Right side reserves room for the value text (~52 px) AND the mute
	// button (~48 px including padding).
	outLeft = row.position.x + 120.f;
	outRight = row.position.x + row.size.x - 132.f;

	if (outRight < outLeft + 1.f)
		outRight = outLeft + 1.f;
}

float ArcadeHubOptions::sliderFractionFromX(const sf::FloatRect& row, float x) const
{
	float left = 0.f;
	float right = 0.f;
	getSliderTrackEdges(row, left, right);

	const float range = right - left;
	if (range <= 0.f)
		return 0.f;

	return std::clamp((x - left) / range, 0.f, 1.f);
}

bool ArcadeHubOptions::handleMousePressed(sf::Vector2f mousePosition)
{
	if (!m_isOpen)
	{
		if (containsPoint(m_gearBounds, mousePosition))
		{
			open();
			return true;
		}
		return false;
	}

	if (containsPoint(m_crtToggleBounds, mousePosition))
	{
		m_selectedRow = Row::CrtShader;
		ArcadeSettings::setCrtEnabled(!ArcadeSettings::isCrtEnabled());
		return true;
	}

	// Mute buttons are inside the slider row bounds, so check them first.
	if (containsPoint(m_musicMuteButtonBounds, mousePosition))
	{
		m_selectedRow = Row::Music;
		ArcadeSettings::setMusicMuted(!ArcadeSettings::isMusicMuted());
		return true;
	}

	if (containsPoint(m_sfxMuteButtonBounds, mousePosition))
	{
		m_selectedRow = Row::Sfx;
		ArcadeSettings::setSfxMuted(!ArcadeSettings::isSfxMuted());
		return true;
	}

	if (containsPoint(m_musicSliderBounds, mousePosition))
	{
		m_selectedRow = Row::Music;
		m_draggingMusic = true;
		const float t = sliderFractionFromX(m_musicSliderBounds, mousePosition.x);
		ArcadeSettings::setMusicVolume(t * ArcadeSettings::kMaxVolume);
		return true;
	}

	if (containsPoint(m_sfxSliderBounds, mousePosition))
	{
		m_selectedRow = Row::Sfx;
		m_draggingSfx = true;
		const float t = sliderFractionFromX(m_sfxSliderBounds, mousePosition.x);
		ArcadeSettings::setSfxVolume(t * ArcadeSettings::kMaxVolume);
		return true;
	}

	if (containsPoint(m_closeButtonBounds, mousePosition))
	{
		close();
		return true;
	}

	if (containsPoint(m_panelBounds, mousePosition))
		return true;

	close();
	return true;
}

void ArcadeHubOptions::handleMouseReleased(sf::Vector2f mousePosition)
{
	(void)mousePosition;
	m_draggingMusic = false;
	m_draggingSfx = false;
}

void ArcadeHubOptions::handleMouseMoved(sf::Vector2f mousePosition)
{
	if (!m_isOpen)
		return;

	if (m_draggingMusic)
	{
		const float t = sliderFractionFromX(m_musicSliderBounds, mousePosition.x);
		ArcadeSettings::setMusicVolume(t * ArcadeSettings::kMaxVolume);
	}

	if (m_draggingSfx)
	{
		const float t = sliderFractionFromX(m_sfxSliderBounds, mousePosition.x);
		ArcadeSettings::setSfxVolume(t * ArcadeSettings::kMaxVolume);
	}
}

bool ArcadeHubOptions::handleKeyReleased(sf::Keyboard::Key key)
{
	if (!m_isOpen)
		return false;

	if (key == sf::Keyboard::Key::Escape)
	{
		close();
		return true;
	}

	if (key == sf::Keyboard::Key::Up || key == sf::Keyboard::Key::W)
	{
		moveSelection(-1);
		return true;
	}

	if (key == sf::Keyboard::Key::Down || key == sf::Keyboard::Key::S)
	{
		moveSelection(+1);
		return true;
	}

	if (key == sf::Keyboard::Key::Left || key == sf::Keyboard::Key::A)
	{
		adjustSelected(-1);
		return true;
	}

	if (key == sf::Keyboard::Key::Right || key == sf::Keyboard::Key::D)
	{
		adjustSelected(+1);
		return true;
	}

	if (key == sf::Keyboard::Key::Enter || key == sf::Keyboard::Key::Space)
	{
		activateSelected();
		return true;
	}

	return true;
}

void ArcadeHubOptions::handleControllerInput()
{
	if (!m_isOpen)
		return;

	if (ArcadeInput::isControllerMoveUpPressed())
	{
		moveSelection(-1);
		ArcadeInput::consumePressedState();
		return;
	}

	if (ArcadeInput::isControllerMoveDownPressed())
	{
		moveSelection(+1);
		ArcadeInput::consumePressedState();
		return;
	}

	if (ArcadeInput::isControllerMoveLeftPressed())
	{
		adjustSelected(-1);
		ArcadeInput::consumePressedState();
		return;
	}

	if (ArcadeInput::isControllerMoveRightPressed())
	{
		adjustSelected(+1);
		ArcadeInput::consumePressedState();
		return;
	}

	if (ArcadeInput::isControllerSecondaryPressed())
	{
		close();
		return;
	}

	if (ArcadeInput::isControllerPrimaryPressed() || ArcadeInput::isControllerConfirmPressed())
	{
		activateSelected();
		ArcadeInput::consumePressedState();
		return;
	}
}

void ArcadeHubOptions::moveSelection(int delta)
{
	int selection = static_cast<int>(m_selectedRow) + delta;
	const int count = static_cast<int>(Row::Count);

	if (selection < 0)
		selection = count - 1;

	if (selection >= count)
		selection = 0;

	m_selectedRow = static_cast<Row>(selection);
}

void ArcadeHubOptions::adjustSelected(int direction)
{
	switch (m_selectedRow)
	{
	case Row::CrtShader:
		ArcadeSettings::setCrtEnabled(!ArcadeSettings::isCrtEnabled());
		break;

	case Row::Music:
		ArcadeSettings::setMusicVolume(
			ArcadeSettings::getMusicVolume() +
			static_cast<float>(direction) * ArcadeSettings::kVolumeStep
		);
		break;

	case Row::Sfx:
		ArcadeSettings::setSfxVolume(
			ArcadeSettings::getSfxVolume() +
			static_cast<float>(direction) * ArcadeSettings::kVolumeStep
		);
		break;

	case Row::Close:
	case Row::Count:
	default:
		break;
	}
}

void ArcadeHubOptions::activateSelected()
{
	switch (m_selectedRow)
	{
	case Row::CrtShader:
		ArcadeSettings::setCrtEnabled(!ArcadeSettings::isCrtEnabled());
		break;

	case Row::Close:
		close();
		break;

	case Row::Music:
	case Row::Sfx:
	case Row::Count:
	default:
		break;
	}
}

void ArcadeHubOptions::drawRowHighlight(sf::RenderTarget& target,
	const sf::FloatRect& bounds,
	bool selected) const
{
	if (!selected)
		return;

	sf::RectangleShape highlight;
	highlight.setPosition({ bounds.position.x - 10.f, bounds.position.y - 4.f });
	highlight.setSize({ bounds.size.x + 20.f, bounds.size.y + 8.f });
	highlight.setFillColor(sf::Color(255, 220, 80, 55));
	highlight.setOutlineColor(sf::Color(255, 220, 80));
	highlight.setOutlineThickness(2.f);
	target.draw(highlight);
}

void ArcadeHubOptions::drawSliderRow(sf::RenderTarget& target,
	const sf::FloatRect& row,
	const std::string& labelStr,
	float value,
	float maxValue,
	bool selected) const
{
	drawRowHighlight(target, row, selected);

	sf::Text label(m_font);
	label.setString(labelStr);
	label.setCharacterSize(26);
	label.setFillColor(sf::Color::White);
	label.setOutlineColor(sf::Color::Black);
	label.setOutlineThickness(1.5f);

	const sf::FloatRect labelBounds = label.getLocalBounds();
	label.setPosition({
		row.position.x,
		row.position.y + (row.size.y - labelBounds.size.y) * 0.5f - labelBounds.position.y
		});
	target.draw(label);

	float trackLeft = 0.f;
	float trackRight = 0.f;
	getSliderTrackEdges(row, trackLeft, trackRight);

	const float trackY = row.position.y + row.size.y * 0.5f;
	const float trackThickness = 6.f;

	sf::RectangleShape track;
	track.setPosition({ trackLeft, trackY - trackThickness * 0.5f });
	track.setSize({ trackRight - trackLeft, trackThickness });
	track.setFillColor(sf::Color(80, 80, 100));
	target.draw(track);

	const float fraction = std::clamp(value / maxValue, 0.f, 1.f);

	sf::RectangleShape fill;
	fill.setPosition({ trackLeft, trackY - trackThickness * 0.5f });
	fill.setSize({ (trackRight - trackLeft) * fraction, trackThickness });
	fill.setFillColor(selected ? sf::Color(255, 220, 80) : sf::Color(180, 220, 255));
	target.draw(fill);

	const float knobRadius = 11.f;
	sf::CircleShape knob(knobRadius);
	knob.setOrigin({ knobRadius, knobRadius });
	knob.setPosition({ trackLeft + (trackRight - trackLeft) * fraction, trackY });
	knob.setFillColor(selected ? sf::Color(255, 220, 80) : sf::Color::White);
	knob.setOutlineColor(sf::Color::Black);
	knob.setOutlineThickness(2.f);
	target.draw(knob);

	sf::Text valueText(m_font);
	valueText.setString(formatVolume(value));
	valueText.setCharacterSize(24);
	valueText.setFillColor(sf::Color::White);
	valueText.setOutlineColor(sf::Color::Black);
	valueText.setOutlineThickness(1.5f);

	const sf::FloatRect valueBounds = valueText.getLocalBounds();
	// Right-align value text just to the left of the mute button (~48 px wide).
	valueText.setPosition({
		row.position.x + row.size.x - 56.f - valueBounds.size.x - valueBounds.position.x,
		row.position.y + (row.size.y - valueBounds.size.y) * 0.5f - valueBounds.position.y
		});
	target.draw(valueText);
}

void ArcadeHubOptions::drawMuteButton(sf::RenderTarget& target,
	const sf::FloatRect& bounds,
	float animationT) const
{
	// Texture swap happens at the midpoint of the animation.
	const bool useOffTexture = animationT >= 0.5f;
	const sf::Texture* texture = useOffTexture ? &m_volumeOffTexture : &m_volumeOnTexture;
	const bool textureLoaded = useOffTexture ? m_hasVolumeOffIcon : m_hasVolumeOnIcon;

	// Subtle background plate so the button reads as clickable.
	sf::RectangleShape plate;
	plate.setPosition(bounds.position);
	plate.setSize(bounds.size);
	plate.setFillColor(sf::Color(30, 30, 45, 200));
	plate.setOutlineColor(sf::Color(200, 200, 200));
	plate.setOutlineThickness(1.5f);
	target.draw(plate);

	if (textureLoaded)
	{
		const sf::Vector2u textureSize = texture->getSize();

		if (textureSize.x > 0 && textureSize.y > 0)
		{
			const float scale = std::min(
				(bounds.size.x - 8.f) / static_cast<float>(textureSize.x),
				(bounds.size.y - 8.f) / static_cast<float>(textureSize.y)
			);

			sf::Sprite icon(*texture);
			icon.setScale({ scale, scale });
			icon.setPosition({
				bounds.position.x + (bounds.size.x - textureSize.x * scale) * 0.5f,
				bounds.position.y + (bounds.size.y - textureSize.y * scale) * 0.5f
				});
			target.draw(icon);
		}
	}
	else
	{
		sf::Text fallback(m_font);
		fallback.setString(useOffTexture ? "M" : "V");
		fallback.setCharacterSize(20);
		fallback.setFillColor(sf::Color::White);
		const sf::FloatRect fb = fallback.getLocalBounds();
		fallback.setPosition({
			bounds.position.x + (bounds.size.x - fb.size.x) * 0.5f - fb.position.x,
			bounds.position.y + (bounds.size.y - fb.size.y) * 0.5f - fb.position.y
			});
		target.draw(fallback);
	}

	// Red X overlay: fades in during second half of animation (0.5 → 1.0).
	const float xAlpha = std::clamp((animationT - 0.5f) * 2.f, 0.f, 1.f);
	if (xAlpha > 0.f)
	{
		const std::uint8_t alpha = static_cast<std::uint8_t>(xAlpha * 255.f);
		const sf::Color xColor(230, 40, 40, alpha);
		const sf::Color xOutline(0, 0, 0, alpha);

		const float padding = 6.f;
		const float x0 = bounds.position.x + padding;
		const float y0 = bounds.position.y + padding;
		const float x1 = bounds.position.x + bounds.size.x - padding;
		const float y1 = bounds.position.y + bounds.size.y - padding;

		const float dx = x1 - x0;
		const float dy = y1 - y0;
		const float length = std::sqrt(dx * dx + dy * dy);
		const float thickness = 4.5f;

		// Diagonal 1: top-left to bottom-right
		{
			sf::RectangleShape diag;
			diag.setSize({ length, thickness });
			diag.setOrigin({ 0.f, thickness * 0.5f });
			diag.setPosition({ x0, y0 });
			const float angle = std::atan2(dy, dx) * 180.f / 3.14159265f;
			diag.setRotation(sf::degrees(angle));
			diag.setFillColor(xColor);
			diag.setOutlineColor(xOutline);
			diag.setOutlineThickness(1.f);
			target.draw(diag);
		}

		// Diagonal 2: bottom-left to top-right
		{
			sf::RectangleShape diag;
			diag.setSize({ length, thickness });
			diag.setOrigin({ 0.f, thickness * 0.5f });
			diag.setPosition({ x0, y1 });
			const float angle = std::atan2(-dy, dx) * 180.f / 3.14159265f;
			diag.setRotation(sf::degrees(angle));
			diag.setFillColor(xColor);
			diag.setOutlineColor(xOutline);
			diag.setOutlineThickness(1.f);
			target.draw(diag);
		}
	}
}

void ArcadeHubOptions::draw(sf::RenderTarget& target) const
{
	{
		sf::RectangleShape gearBackground;
		gearBackground.setPosition(m_gearBounds.position);
		gearBackground.setSize(m_gearBounds.size);
		gearBackground.setFillColor(sf::Color(20, 20, 30, 200));
		gearBackground.setOutlineColor(sf::Color(220, 220, 220));
		gearBackground.setOutlineThickness(2.f);
		target.draw(gearBackground);

		if (m_hasSettingsIcon)
		{
			const sf::Vector2u textureSize = m_settingsIconTexture.getSize();

			if (textureSize.x > 0 && textureSize.y > 0)
			{
				const float scale = std::min(
					(m_gearBounds.size.x - 10.f) / static_cast<float>(textureSize.x),
					(m_gearBounds.size.y - 10.f) / static_cast<float>(textureSize.y)
				);

				sf::Sprite icon(m_settingsIconTexture);
				icon.setScale({ scale, scale });
				icon.setPosition({
					m_gearBounds.position.x + (m_gearBounds.size.x - textureSize.x * scale) * 0.5f,
					m_gearBounds.position.y + (m_gearBounds.size.y - textureSize.y * scale) * 0.5f
					});
				target.draw(icon);
			}
		}
		else
		{
			sf::Text fallback(m_font);
			fallback.setString("[*]");
			fallback.setCharacterSize(22);
			fallback.setFillColor(sf::Color::White);
			const sf::FloatRect bounds = fallback.getLocalBounds();
			fallback.setPosition({
				m_gearBounds.position.x + (m_gearBounds.size.x - bounds.size.x) * 0.5f - bounds.position.x,
				m_gearBounds.position.y + (m_gearBounds.size.y - bounds.size.y) * 0.5f - bounds.position.y
				});
			target.draw(fallback);
		}
	}

	if (!m_isOpen)
		return;

	sf::RectangleShape dim;
	dim.setPosition({ 0.f, 0.f });
	dim.setSize({ static_cast<float>(m_lastLayoutSize.x), static_cast<float>(m_lastLayoutSize.y) });
	dim.setFillColor(sf::Color(0, 0, 0, 175));
	target.draw(dim);

	sf::RectangleShape panel;
	panel.setPosition(m_panelBounds.position);
	panel.setSize(m_panelBounds.size);
	panel.setFillColor(sf::Color(20, 22, 32, 245));
	panel.setOutlineColor(sf::Color(230, 230, 230));
	panel.setOutlineThickness(3.f);
	target.draw(panel);

	{
		sf::Text title(m_font);
		title.setString("OPTIONS");
		title.setCharacterSize(42);
		title.setFillColor(sf::Color::White);
		title.setOutlineColor(sf::Color::Black);
		title.setOutlineThickness(2.f);
		const sf::FloatRect bounds = title.getLocalBounds();
		title.setPosition({
			m_panelBounds.position.x + (m_panelBounds.size.x - bounds.size.x) * 0.5f - bounds.position.x,
			m_panelBounds.position.y + 30.f - bounds.position.y
			});
		target.draw(title);
	}

	drawRowHighlight(target, m_crtToggleBounds, m_selectedRow == Row::CrtShader);

	{
		sf::Text label(m_font);
		label.setString("CRT Shader:");
		label.setCharacterSize(26);
		label.setFillColor(sf::Color::White);
		label.setOutlineColor(sf::Color::Black);
		label.setOutlineThickness(1.5f);
		const sf::FloatRect labelBounds = label.getLocalBounds();
		label.setPosition({
			m_crtToggleBounds.position.x,
			m_crtToggleBounds.position.y + (m_crtToggleBounds.size.y - labelBounds.size.y) * 0.5f - labelBounds.position.y
			});
		target.draw(label);

		const bool crtOn = ArcadeSettings::isCrtEnabled();

		sf::Text stateText(m_font);
		stateText.setString(crtOn ? "ON" : "OFF");
		stateText.setCharacterSize(30);
		stateText.setFillColor(crtOn ? sf::Color(60, 220, 80) : sf::Color(235, 70, 70));
		stateText.setOutlineColor(sf::Color::Black);
		stateText.setOutlineThickness(2.f);
		const sf::FloatRect stateBounds = stateText.getLocalBounds();
		stateText.setPosition({
			m_crtToggleBounds.position.x + m_crtToggleBounds.size.x - stateBounds.size.x - 4.f - stateBounds.position.x,
			m_crtToggleBounds.position.y + (m_crtToggleBounds.size.y - stateBounds.size.y) * 0.5f - stateBounds.position.y
			});
		target.draw(stateText);
	}

	drawSliderRow(target, m_musicSliderBounds, "Music:",
		ArcadeSettings::getMusicVolume(), ArcadeSettings::kMaxVolume,
		m_selectedRow == Row::Music);

	drawMuteButton(target, m_musicMuteButtonBounds, m_musicMuteAnimT);

	drawSliderRow(target, m_sfxSliderBounds, "SFX:",
		ArcadeSettings::getSfxVolume(), ArcadeSettings::kMaxVolume,
		m_selectedRow == Row::Sfx);

	drawMuteButton(target, m_sfxMuteButtonBounds, m_sfxMuteAnimT);

	{
		const bool selected = (m_selectedRow == Row::Close);

		sf::RectangleShape closeButton;
		closeButton.setPosition(m_closeButtonBounds.position);
		closeButton.setSize(m_closeButtonBounds.size);
		closeButton.setFillColor(selected ? sf::Color(255, 220, 80, 90) : sf::Color(40, 40, 55, 220));
		closeButton.setOutlineColor(selected ? sf::Color(255, 220, 80) : sf::Color(220, 220, 220));
		closeButton.setOutlineThickness(2.f);
		target.draw(closeButton);

		sf::Text closeLabel(m_font);
		closeLabel.setString("CLOSE");
		closeLabel.setCharacterSize(26);
		closeLabel.setFillColor(sf::Color::White);
		closeLabel.setOutlineColor(sf::Color::Black);
		closeLabel.setOutlineThickness(1.5f);
		const sf::FloatRect closeBounds = closeLabel.getLocalBounds();
		closeLabel.setPosition({
			m_closeButtonBounds.position.x + (m_closeButtonBounds.size.x - closeBounds.size.x) * 0.5f - closeBounds.position.x,
			m_closeButtonBounds.position.y + (m_closeButtonBounds.size.y - closeBounds.size.y) * 0.5f - closeBounds.position.y
			});
		target.draw(closeLabel);
	}
}

const std::string& ArcadeHubOptions::getLastError() const
{
	return m_lastError;
}
