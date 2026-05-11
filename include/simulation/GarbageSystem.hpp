#pragma once

#include <cstdint>

namespace tetris {

inline std::uint64_t garbageHoleSplitMix64(std::uint64_t z) noexcept {
    z += 0x9E3779B97F4A7C15ULL;
    z = (z ^ (z >> 30U)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27U)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31U);
}

/// Deterministic garbage hole column shared by all LAN nodes (match seed + frame + players + batch index).
inline int garbageHoleColumn(std::uint32_t matchSeed, std::uint32_t effectiveFrame, int sourcePlayerId, int targetPlayerId,
                             int batchIndex, int boardWidth) noexcept {
    std::uint64_t key = static_cast<std::uint64_t>(matchSeed);
    key ^= static_cast<std::uint64_t>(effectiveFrame + 2654435761ULL * static_cast<std::uint32_t>(sourcePlayerId));
    key ^= static_cast<std::uint64_t>((static_cast<std::uint32_t>(targetPlayerId) + 314159263U)
                                     * (static_cast<std::uint32_t>(batchIndex) + 974200651U));
    const std::uint64_t sm = garbageHoleSplitMix64(key);
    if (boardWidth <= 0) {
        return 0;
    }
    return static_cast<int>(sm % static_cast<unsigned>(boardWidth));
}

} // namespace tetris
