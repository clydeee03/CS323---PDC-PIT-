#pragma once

#include <cstdint>
#include <string>

namespace tetris {

enum class Mode {
    Unset,
    Solo,
    /// Two players on one machine (no network). For lockstep/multiplayer mechanic testing.
    Local2P,
    Host,
    Client
};

struct AppConfig {
    Mode mode{Mode::Unset};
    std::string hostAddress{"127.0.0.1"};
    std::uint16_t port{7777};
    int maxPlayers{2};
    std::uint32_t tickRate{30};
    std::uint32_t hashIntervalFrames{120};
};

} // namespace tetris
