#pragma once

#include "core/Config.hpp"
#include "network/NetworkNode.hpp"
#include "simulation/TetrisGame.hpp"
#include "utils/ThreadSafeQueue.hpp"

#include <SDL.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

namespace tetris {

class App {
public:
    explicit App(AppConfig config);
    ~App();

    bool initialize();
    int run();
    void shutdown();

private:
    using FrameInputs = std::unordered_map<std::uint32_t, std::unordered_map<int, std::uint8_t>>;

    void networkLoop();
    void simulationLoop();
    void renderFrame();
    void processSdlEvents();
    void handlePacket(const ReceivedPacket& packet);
    bool chooseStartupMode();
    void broadcastStartIfNeeded();
    bool isMultiplayer() const;
    bool isLocalTwoPlayer() const;
    bool hasAllInputsForFrame(std::uint32_t frame) const;
    void submitLocalInput(std::uint32_t frame, int playerId, std::uint8_t bits);
    void sendInputPacket(std::uint32_t frame, std::uint8_t bits);
    void sendGarbagePackets(std::uint32_t effectiveFrame, int sourcePlayerId, int targetPlayerId, int lines);
    void sendHeartbeatPing();
    void metricsLoop();

    AppConfig config_;
    NetworkNode network_;

    SDL_Window* window_{nullptr};
    SDL_Renderer* renderer_{nullptr};

    std::atomic<bool> running_{false};
    std::atomic<bool> matchStarted_{false};
    std::atomic<std::uint8_t> inputBits_{0};
    std::atomic<std::uint8_t> inputBitsP0_{0};
    std::atomic<std::uint8_t> inputBitsP1_{0};

    std::thread networkThread_;
    std::thread simulationThread_;
    std::thread metricsThread_;

    mutable std::mutex stateMutex_;
    FrameInputs frameInputs_;
    std::vector<TetrisGame> games_;
    std::vector<int> playerIds_;
    int localPlayerId_{0};
    std::uint32_t sharedSeed_{1337};
    std::uint32_t currentFrame_{0};
    std::uint32_t startAnnounceTicks_{0};

    std::atomic<std::uint64_t> simulationFramesExecuted_{0};
    std::atomic<int> smoothedSimulationHz_{0};
    std::atomic<int> displayFps_{0};
    std::atomic<int> smoothedPingMs_{0};
    std::atomic<std::uint64_t> desyncEvents_{0};
    std::atomic<std::uint64_t> garbagePacketsSent_{0};
    std::atomic<std::uint64_t> garbagePacketsReceived_{0};

    std::atomic<int> hudCombo_{0};
    std::atomic<int> hudGarbageInbound_{0};
    std::atomic<int> hudGarbageOutbound_{0};
    std::atomic<std::uint64_t> netBytesPerSecApprox_{0};

    std::uint32_t lastHeartbeatSendMs_{0};
    std::uint64_t lastMetricsSimFrames_{0};
    std::uint64_t lastMetricsNetBytes_{0};
};

} // namespace tetris
