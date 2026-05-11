#include "core/App.hpp"

#include <algorithm>
#include <iostream>
#include <string>

using namespace tetris;

int main(int argc, char** argv) {
    AppConfig config{};
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--solo") {
            config.mode = Mode::Solo;
            config.maxPlayers = 1;
        } else if (arg == "--local" || arg == "--local2p") {
            config.mode = Mode::Local2P;
            config.maxPlayers = 2;
        } else if (arg == "--host") {
            config.mode = Mode::Host;
        } else if (arg == "--client") {
            config.mode = Mode::Client;
        } else if (arg == "--address" && i + 1 < argc) {
            config.hostAddress = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            config.port = static_cast<std::uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--players" && i + 1 < argc) {
            config.maxPlayers = (std::max)(2, std::stoi(argv[++i]));
        } else if (arg == "--tick" && i + 1 < argc) {
            config.tickRate = static_cast<std::uint32_t>(std::stoi(argv[++i]));
        }
    }

    std::string modeStr = "Menu";
    if (config.mode == Mode::Solo) {
        modeStr = "Solo";
    } else if (config.mode == Mode::Local2P) {
        modeStr = "Local 2P";
    } else if (config.mode == Mode::Host) {
        modeStr = "Host";
    } else if (config.mode == Mode::Client) {
        modeStr = "Client";
    }
    std::cout << "Mode: " << modeStr << "\n";
    std::cout << "Port: " << config.port << "\n";
    if (config.mode == Mode::Client) {
        std::cout << "Host address: " << config.hostAddress << "\n";
    }

    App app(config);
    if (!app.initialize()) {
        std::cerr << "Failed to initialize app\n";
        return 1;
    }
    return app.run();
}
