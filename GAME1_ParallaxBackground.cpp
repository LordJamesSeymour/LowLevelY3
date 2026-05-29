#include "GAME1_ParallaxBackground.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <system_error>

namespace
{
    // Returns true if filename matches "<set>_layerNN.png" where NN is 01..99.
    // On match, fills outSetName with <set> and outLayerIndex with NN-1.
    bool parseLayerFilename(const std::string& filename,
                            std::string& outSetName,
                            int& outLayerIndex)
    {
        const std::string suffixHead = "_layer";
        const std::string suffixTail = ".png";

        if (filename.size() < suffixHead.size() + 2 + suffixTail.size())
            return false;

        const std::size_t tailPos = filename.size() - suffixTail.size();
        if (filename.compare(tailPos, suffixTail.size(), suffixTail) != 0)
            return false;

        // Digits directly before ".png".
        std::size_t digitsEnd = tailPos;
        std::size_t digitsBegin = digitsEnd;
        while (digitsBegin > 0 &&
               filename[digitsBegin - 1] >= '0' &&
               filename[digitsBegin - 1] <= '9')
        {
            --digitsBegin;
        }
        if (digitsBegin == digitsEnd)
            return false;

        if (digitsBegin < suffixHead.size())
            return false;
        if (filename.compare(digitsBegin - suffixHead.size(),
                             suffixHead.size(), suffixHead) != 0)
            return false;

        const std::string digits = filename.substr(digitsBegin,
                                                   digitsEnd - digitsBegin);
        const int n = std::stoi(digits);
        if (n < 1)
            return false;

        outLayerIndex = n - 1;
        outSetName = filename.substr(0, digitsBegin - suffixHead.size());
        return true;
    }
}

float GAME1_ParallaxBackground::parallaxFactorForLayer(std::size_t layerIndex)
{
    // layer01 = BASE; layer02 = BASE * 1.20; layer03 = BASE * 1.20 * 1.15.
    switch (layerIndex)
    {
    case 0: return kBaseFactor;
    case 1: return kBaseFactor * 1.20f;
    case 2: return kBaseFactor * 1.20f * 1.15f;
    default: return kBaseFactor;
    }
}

bool GAME1_ParallaxBackground::loadFromDirectory(
    const std::filesystem::path& parallaxDirectory)
{
    namespace fs = std::filesystem;

    m_loadedLayerCount = 0;
    m_setName.clear();
    for (Layer& layer : m_layers)
        layer.loaded = false;

    std::error_code ec;
    if (!fs::exists(parallaxDirectory, ec) ||
        !fs::is_directory(parallaxDirectory, ec))
    {
        return false;
    }

    // Group available layer files by set name. std::map keeps sets ordered
    // alphabetically so the first complete set wins deterministically.
    std::map<std::string, std::array<bool, 3>> sets;

    for (const fs::directory_entry& entry :
         fs::directory_iterator(parallaxDirectory, ec))
    {
        if (ec) break;
        if (!entry.is_regular_file())
            continue;

        const std::string filename = entry.path().filename().string();
        std::string setName;
        int layerIndex = 0;
        if (!parseLayerFilename(filename, setName, layerIndex))
            continue;
        if (layerIndex < 0 || layerIndex >= 3)
            continue;

        sets[setName][static_cast<std::size_t>(layerIndex)] = true;
    }

    if (sets.empty())
        return false;

    // Prefer the first complete (all three layers present) set alphabetically.
    // To assign different sets per world/level later, call loadSet() instead.
    std::string chosenSet;
    for (const auto& kv : sets)
    {
        if (kv.second[0] && kv.second[1] && kv.second[2])
        {
            chosenSet = kv.first;
            break;
        }
    }
    if (chosenSet.empty())
    {
        // No complete set: fall back to the alphabetically-first partial set.
        chosenSet = sets.begin()->first;
        std::cerr << "[GAME1_ParallaxBackground] No complete 3-layer set in "
                  << parallaxDirectory.string()
                  << "; falling back to partial set '" << chosenSet << "'.\n";
    }

    return loadSet(parallaxDirectory, chosenSet);
}

bool GAME1_ParallaxBackground::loadSet(
    const std::filesystem::path& parallaxDirectory,
    const std::string& setName)
{
    namespace fs = std::filesystem;

    m_loadedLayerCount = 0;
    m_setName = setName;
    for (Layer& layer : m_layers)
        layer.loaded = false;

    for (std::size_t i = 0; i < m_layers.size(); ++i)
    {
        const std::string suffix = (i + 1 < 10)
            ? std::string("_layer0") + static_cast<char>('0' + static_cast<int>(i + 1)) + ".png"
            : std::string("_layer") + std::to_string(i + 1) + ".png";

        const fs::path layerPath = parallaxDirectory / (setName + suffix);

        std::error_code ec;
        if (!fs::exists(layerPath, ec) || !fs::is_regular_file(layerPath, ec))
        {
            std::cerr << "[GAME1_ParallaxBackground] Missing layer "
                      << (i + 1) << " for set '" << setName << "' at "
                      << layerPath.string() << "\n";
            continue;
        }

        if (!m_layers[i].texture.loadFromFile(layerPath.string()))
        {
            std::cerr << "[GAME1_ParallaxBackground] Failed to load "
                      << layerPath.string() << "\n";
            continue;
        }

        m_layers[i].texture.setSmooth(false);
        m_layers[i].texture.setRepeated(true);
        m_layers[i].loaded = true;
        ++m_loadedLayerCount;
    }

    return m_loadedLayerCount > 0;
}

void GAME1_ParallaxBackground::draw(sf::RenderWindow& window,
                                    sf::Vector2f cameraCenter) const
{
    if (m_loadedLayerCount == 0)
        return;

    const sf::Vector2u windowSize = window.getSize();
    if (windowSize.x == 0 || windowSize.y == 0)
        return;

    const sf::View previousView = window.getView();

    const sf::FloatRect screenRect(
        { 0.f, 0.f },
        { static_cast<float>(windowSize.x), static_cast<float>(windowSize.y) });
    window.setView(sf::View(screenRect));

    const float viewWidth = screenRect.size.x;
    const float viewHeight = screenRect.size.y;

    for (std::size_t i = 0; i < m_layers.size(); ++i)
    {
        const Layer& layer = m_layers[i];
        if (!layer.loaded)
            continue;

        const sf::Vector2u texSize = layer.texture.getSize();
        if (texSize.x == 0 || texSize.y == 0)
            continue;

        // Uniform scale to fill the screen vertically (pixel-art crisp).
        const float scale = viewHeight / static_cast<float>(texSize.y);
        const float scaledTexWidth = static_cast<float>(texSize.x) * scale;
        if (scaledTexWidth <= 0.f)
            continue;

        const float factor = parallaxFactorForLayer(i);
        // Scroll relative to camera centre; positive camera => layer moves
        // left on screen, so subtract.
        float scroll = cameraCenter.x * factor;
        float wrapped = std::fmod(scroll, scaledTexWidth);
        if (wrapped < 0.f)
            wrapped += scaledTexWidth;

        sf::Sprite sprite(layer.texture);
        sprite.setScale({ scale, scale });

        for (float x = -wrapped; x < viewWidth; x += scaledTexWidth)
        {
            sprite.setPosition({ x, 0.f });
            window.draw(sprite);
        }
    }

    window.setView(previousView);
}
