#pragma once

#include <SFML/Graphics.hpp>

#include <array>
#include <filesystem>
#include <string>

// Tiled parallax background for Surfers Quest (Game #1).
// Loads a 3-layer set (layer01/02/03) from a directory, where files follow
// the pattern <setName>_layer0N.png. Layers are drawn screen-aligned, tiled
// horizontally, scaled uniformly to fill the view height, with a per-layer
// parallax scroll factor derived from the gameplay camera centre.
//
// Layer speed contract (per project spec):
//   layer01 = BASE (slowest, furthest)
//   layer02 = BASE * 1.20
//   layer03 = BASE * 1.20 * 1.15
class GAME1_ParallaxBackground
{
public:
    static constexpr float kBaseFactor = 0.20f;

    // Scan parallaxDirectory for sets and load the first complete set
    // (alphabetical by set name). Returns true if any layer loaded.
    // A "set" is complete when all three layerNN.png files exist; an
    // incomplete set is still loaded for whatever layers are present
    // if no complete set is found alphabetically before it.
    bool loadFromDirectory(const std::filesystem::path& parallaxDirectory);

    // Optional: load a specific named set (without the _layerNN.png suffix).
    // Provided so future code can assign different sets per world/level.
    bool loadSet(const std::filesystem::path& parallaxDirectory,
                 const std::string& setName);

    bool hasAnyLayer() const { return m_loadedLayerCount > 0; }
    const std::string& getSetName() const { return m_setName; }

    // Draw all loaded layers screen-aligned, using cameraCenter (in world
    // pixels) to compute per-layer parallax scroll. Temporarily switches the
    // window to a screen-rect view and restores the previous view on exit.
    void draw(sf::RenderWindow& window, sf::Vector2f cameraCenter) const;

private:
    struct Layer
    {
        sf::Texture texture;
        bool loaded = false;
    };

    static float parallaxFactorForLayer(std::size_t layerIndex);

    std::array<Layer, 3> m_layers{};
    std::size_t m_loadedLayerCount = 0;
    std::string m_setName;
};
