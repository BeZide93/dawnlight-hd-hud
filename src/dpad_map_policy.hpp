#pragma once
#include <cstdint>
namespace twilight_hd_hud {
struct DpadMapMasks {
    std::uint32_t map;
    std::uint32_t minimap;
    std::uint32_t released;
    bool combined;
};
constexpr DpadMapMasks dpad_map_masks(std::uint32_t map, std::uint32_t minimap,
    bool combined, bool combineUp, bool leftEnabled, bool rightEnabled,
    std::uint32_t midna, std::uint32_t up, std::uint32_t left, std::uint32_t right) {
    const auto released = (leftEnabled ? 0u : left) | (rightEnabled ? 0u : right) |
        (combineUp ? left | right : 0u);
    if (combineUp) {
        // Never steal the user's dedicated Midna direction.
        map = minimap = (midna & up) ? 0u : up;
        combined = true;
    }
    return {map & ~released, minimap & ~released, released, combined};
}
} // namespace twilight_hd_hud
