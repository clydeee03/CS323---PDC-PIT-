#include "core/App.hpp"

#include "network/Packets.hpp"

#include <array>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <unordered_map>

namespace tetris {

namespace {
constexpr int WindowH = 720;
constexpr int CellSizeLocal = 26;
constexpr int BoardOffsetXLocal = 36;
constexpr int BoardOffsetYLocal = 56;

using Glyph = std::array<std::uint8_t, 7>;

const std::unordered_map<char, Glyph> kFont = {
    {' ', {0, 0, 0, 0, 0, 0, 0}},
    {':', {0, 4, 0, 0, 4, 0, 0}},
    {'0', {14, 17, 17, 17, 17, 17, 14}},
    {'1', {4, 12, 4, 4, 4, 4, 14}},
    {'2', {14, 17, 1, 2, 4, 8, 31}},
    {'3', {30, 1, 1, 14, 1, 1, 30}},
    {'4', {2, 6, 10, 18, 31, 2, 2}},
    {'5', {31, 16, 16, 30, 1, 1, 30}},
    {'6', {14, 16, 16, 30, 17, 17, 14}},
    {'7', {31, 1, 2, 4, 8, 8, 8}},
    {'8', {14, 17, 17, 14, 17, 17, 14}},
    {'9', {14, 17, 17, 15, 1, 1, 14}},
    {'A', {14, 17, 17, 31, 17, 17, 17}},
    {'C', {14, 17, 16, 16, 16, 17, 14}},
    {'D', {30, 17, 17, 17, 17, 17, 30}},
    {'E', {31, 16, 16, 30, 16, 16, 31}},
    {'F', {31, 16, 16, 30, 16, 16, 16}},
    {'M', {17, 27, 21, 17, 17, 17, 17}},
    {'N', {17, 25, 21, 19, 17, 17, 17}},
    {'O', {14, 17, 17, 17, 17, 17, 14}},
    {'P', {30, 17, 17, 30, 16, 16, 16}},
    {'R', {30, 17, 17, 30, 20, 18, 17}},
    {'S', {15, 16, 16, 14, 1, 1, 30}},
    {'T', {31, 4, 4, 4, 4, 4, 4}},
    {'X', {17, 17, 10, 4, 10, 17, 17}},
    {'B', {31, 17, 17, 14, 17, 17, 31}},
    {'G', {14, 17, 16, 16, 22, 17, 14}},
    {'I', {14, 4, 4, 4, 4, 4, 14}},
    {'J', {15, 18, 18, 6, 2, 18, 15}},
    {'K', {17, 18, 20, 24, 20, 18, 17}},
    {'L', {17, 17, 17, 17, 17, 17, 31}},
    {'P', {30, 17, 17, 30, 16, 16, 16}},
    {'U', {17, 17, 17, 17, 17, 17, 14}},
    {'V', {17, 17, 17, 17, 17, 10, 4}},
    {'W', {17, 17, 17, 21, 21, 21, 10}},
    {'Y', {17, 17, 10, 4, 4, 4, 4}},
    {'Z', {31, 1, 2, 4, 8, 16, 31}},
    {'-', {0, 0, 0, 31, 0, 0, 0}},
    {'%', {24, 18, 23, 4, 25, 9, 6}},
    {'.', {0, 0, 0, 0, 0, 0, 12}},
};

void drawText(SDL_Renderer* renderer, int x, int y, int scale, const std::string& text, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    int cursorX = x;
    for (char c : text) {
        const auto it = kFont.find(c);
        const Glyph& g = (it != kFont.end()) ? it->second : kFont.at(' ');
        for (int row = 0; row < 7; ++row) {
            for (int col = 0; col < 5; ++col) {
                if ((g[row] & (1 << (4 - col))) == 0) {
                    continue;
                }
                SDL_Rect px{cursorX + col * scale, y + row * scale, scale, scale};
                SDL_RenderFillRect(renderer, &px);
            }
        }
        cursorX += 6 * scale;
    }
}

void drawTextCentered(SDL_Renderer* renderer, int centerX, int y, int scale, const std::string& text, SDL_Color color) {
    const int textW = static_cast<int>(text.size()) * 6 * scale;
    drawText(renderer, centerX - textW / 2, y, scale, text, color);
}

void drawFullscreenDim(SDL_Renderer* renderer, int ww, int wh) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 8, 8, 14, 200);
    SDL_Rect bg{0, 0, ww, wh};
    SDL_RenderFillRect(renderer, &bg);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

void setCellFillColor(SDL_Renderer* renderer, int cellValue, int tintType) {
    if (cellValue == 0) {
        SDL_SetRenderDrawColor(renderer, 42, 44, 56, 255);
        return;
    }
    if (cellValue >= TetrisGame::GarbageCell) {
        SDL_SetRenderDrawColor(renderer, 160, 64, 86, 255);
        return;
    }
    const std::uint8_t c = static_cast<std::uint8_t>(20 * cellValue + 60);
    (void)tintType;
    SDL_SetRenderDrawColor(renderer, c, static_cast<std::uint8_t>(205 - c / 2), 218, 255);
}

void drawBoardCells(SDL_Renderer* renderer,
    int ox,
    int oy,
    int cs,
    const std::array<std::array<int, TetrisGame::BoardW>, TetrisGame::BoardH>& board,
    const std::vector<std::pair<int, int>>* active,
    int activeTint) {
    const int inset = cs >= 18 ? 1 : 0;
    const int insetSmall = cs < 18 ? 1 : 0;
    for (int y = 0; y < TetrisGame::BoardH; ++y) {
        for (int x = 0; x < TetrisGame::BoardW; ++x) {
            const int v = board[y][x];
            SDL_Rect r{ox + x * cs, oy + y * cs,
                cs - inset - insetSmall,
                cs - inset - insetSmall};
            if (cs < 18 && r.w > 4) {
                r.w -= 1;
                r.h -= 1;
            }
            setCellFillColor(renderer, v, activeTint);
            SDL_RenderFillRect(renderer, &r);
        }
    }
    if (active != nullptr && !active->empty()) {
        for (const auto& block : *active) {
            const int x = block.first;
            const int y = block.second;
            if (x < 0 || x >= TetrisGame::BoardW || y < 0 || y >= TetrisGame::BoardH) {
                continue;
            }
            SDL_Rect rr{ox + x * cs, oy + y * cs, cs - inset - insetSmall, cs - inset - insetSmall};
            if (cs < 18 && rr.w > 4) {
                rr.w = (std::max)(3, rr.w - 2);
                rr.h = (std::max)(3, rr.h - 2);
            }
            const std::uint8_t c = static_cast<std::uint8_t>(20 * activeTint + 60);
            SDL_SetRenderDrawColor(renderer, static_cast<std::uint8_t>((std::min)(255, static_cast<int>(c) + 35)),
                static_cast<std::uint8_t>(228 - c / 3), 255, 255);
            SDL_RenderFillRect(renderer, &rr);
        }
    }
}

void drawBoardFrame(SDL_Renderer* renderer, int ox, int oy, int cs, SDL_Color border, int pad) {
    const int w = TetrisGame::BoardW * cs + pad * 2;
    const int h = TetrisGame::BoardH * cs + pad * 2;
    SDL_Rect outer{ox - pad, oy - pad, w, h};
    SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
    SDL_RenderDrawRect(renderer, &outer);
    SDL_SetRenderDrawColor(renderer, static_cast<std::uint8_t>(border.r / 3 + 20),
        static_cast<std::uint8_t>(border.g / 3 + 20), static_cast<std::uint8_t>(border.b / 3 + 20),
        border.a);
    SDL_Rect inner{ox - pad + 1, oy - pad + 1, w - 2, h - 2};
    SDL_RenderDrawRect(renderer, &inner);
}
} // namespace

App::App(AppConfig config) : config_(std::move(config)) {}

App::~App() {
    shutdown();
}

bool App::initialize() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        std::cerr << "SDL init failed\n";
        return false;
    }
    window_ = SDL_CreateWindow(
        "Distributed Multiplayer Tetris",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        1000,
        WindowH,
        SDL_WINDOW_SHOWN);
    if (window_ == nullptr) {
        return false;
    }
    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer_ == nullptr) {
        return false;
    }

    if (config_.mode == Mode::Unset && !chooseStartupMode()) {
        return false;
    }

    if (isMultiplayer()) {
        if (!network_.initialize()) {
            return false;
        }

        bool ok = false;
        if (config_.mode == Mode::Host) {
            ok = network_.startHost(config_.port, config_.maxPlayers);
            localPlayerId_ = 0;
            playerIds_ = {0};
        } else {
            ok = network_.startClient(config_.hostAddress, config_.port);
            localPlayerId_ = 1;
        }
        if (!ok) {
            return false;
        }
    }
    return true;
}

int App::run() {
    running_.store(true);

    if (config_.mode == Mode::Host || config_.mode == Mode::Solo) {
        sharedSeed_ = static_cast<std::uint32_t>(std::random_device{}());
    }

    games_.clear();
    const int gameCount =
        isMultiplayer() ? config_.maxPlayers : (config_.mode == Mode::Local2P ? 2 : 1);
    for (int i = 0; i < gameCount; ++i) {
        games_.emplace_back(sharedSeed_ + static_cast<std::uint32_t>(i));
    }
    playerIds_.clear();
    for (int i = 0; i < gameCount; ++i) {
        playerIds_.push_back(i);
    }
    if (!isMultiplayer()) {
        localPlayerId_ = 0;
        matchStarted_.store(true);
        if (window_ != nullptr && config_.mode == Mode::Solo) {
            SDL_SetWindowSize(window_, 680, WindowH);
        }
    }

    if (isMultiplayer()) {
        networkThread_ = std::thread(&App::networkLoop, this);
    }
    simulationThread_ = std::thread(&App::simulationLoop, this);
    metricsThread_ = std::thread(&App::metricsLoop, this);

    while (running_.load()) {
        processSdlEvents();
        renderFrame();
        SDL_Delay(4);
    }

    if (networkThread_.joinable()) {
        networkThread_.join();
    }
    if (simulationThread_.joinable()) {
        simulationThread_.join();
    }
    if (metricsThread_.joinable()) {
        metricsThread_.join();
    }
    return 0;
}

void App::shutdown() {
    running_.store(false);
    if (networkThread_.joinable()) {
        networkThread_.join();
    }
    if (simulationThread_.joinable()) {
        simulationThread_.join();
    }
    if (metricsThread_.joinable()) {
        metricsThread_.join();
    }
    network_.shutdown();
    if (renderer_ != nullptr) {
        SDL_DestroyRenderer(renderer_);
        renderer_ = nullptr;
    }
    if (window_ != nullptr) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    SDL_Quit();
}

bool App::chooseStartupMode() {
    const SDL_MessageBoxButtonData buttons[] = {
        {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Solo"},
        {0, 2, "Host Multiplayer"},
        {0, 3, "Client Multiplayer"},
        {0, 4, "Local 2P (test)"},
        {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Quit"},
    };
    const SDL_MessageBoxData messageBoxData{
        SDL_MESSAGEBOX_INFORMATION,
        window_,
        "Select Game Mode",
        "Choose how to start Tetris.",
        static_cast<int>(std::size(buttons)),
        buttons,
        nullptr};
    int selectedButton = 0;
    if (SDL_ShowMessageBox(&messageBoxData, &selectedButton) < 0) {
        return false;
    }
    if (selectedButton == 1) {
        config_.mode = Mode::Solo;
        config_.maxPlayers = 1;
        return true;
    }
    if (selectedButton == 4) {
        config_.mode = Mode::Local2P;
        config_.maxPlayers = 2;
        return true;
    }
    if (selectedButton == 2) {
        config_.mode = Mode::Host;
        config_.maxPlayers = (std::max)(2, config_.maxPlayers);
        return true;
    }
    if (selectedButton == 3) {
        config_.mode = Mode::Client;
        config_.maxPlayers = (std::max)(2, config_.maxPlayers);
        return true;
    }
    return false;
}

void App::networkLoop() {
    while (running_.load()) {
        ReceivedPacket packet{};
        if (network_.poll(packet, 10U)) {
            handlePacket(packet);
        }
        if (config_.mode == Mode::Host) {
            broadcastStartIfNeeded();
        }

        const std::uint32_t now = SDL_GetTicks();
        if (isMultiplayer() && matchStarted_.load() &&
            ((lastHeartbeatSendMs_ == 0U || now > lastHeartbeatSendMs_) && now - lastHeartbeatSendMs_ >= 400U)) {
            lastHeartbeatSendMs_ = now;
            sendHeartbeatPing();
        }
    }
}

bool App::isMultiplayer() const {
    return config_.mode == Mode::Host || config_.mode == Mode::Client;
}

bool App::isLocalTwoPlayer() const {
    return config_.mode == Mode::Local2P;
}

void App::broadcastStartIfNeeded() {
    if (matchStarted_.load()) {
        return;
    }
    const int connected = static_cast<int>(network_.connectedPeers().size()) + 1;
    if (connected < config_.maxPlayers) {
        return;
    }
    startAnnounceTicks_ += 1;
    if (startAnnounceTicks_ < 60) {
        return;
    }

    MatchStartPacket start{};
    start.header.type = PacketType::MatchStart;
    start.header.frame = 0;
    start.header.playerId = 0;
    start.seed = sharedSeed_;
    start.startFrame = 0;
    start.playerCount = static_cast<std::uint8_t>(config_.maxPlayers);
    network_.broadcastReliable(serializePacket(start));
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        playerIds_.clear();
        for (int i = 0; i < config_.maxPlayers; ++i) {
            playerIds_.push_back(i);
            games_[i].reset(sharedSeed_ + static_cast<std::uint32_t>(i));
        }
    }
    matchStarted_.store(true);
    std::cout << "Match started\n";
}

void App::handlePacket(const ReceivedPacket& packet) {
    if (packet.payload.empty()) {
        return;
    }
    PacketType type = static_cast<PacketType>(packet.payload[0]);
    if (type == PacketType::MatchStart) {
        MatchStartPacket p{};
        if (!deserializePacket(packet.payload.data(), packet.payload.size(), p)) {
            return;
        }
        sharedSeed_ = p.seed;
        {
            std::lock_guard<std::mutex> lock(stateMutex_);
            playerIds_.clear();
            for (int i = 0; i < p.playerCount; ++i) {
                playerIds_.push_back(i);
                games_[i].reset(sharedSeed_ + static_cast<std::uint32_t>(i));
            }
        }
        matchStarted_.store(true);
        return;
    }

    if (type == PacketType::Input) {
        InputPacket p{};
        if (!deserializePacket(packet.payload.data(), packet.payload.size(), p)) {
            return;
        }
        std::lock_guard<std::mutex> lock(stateMutex_);
        frameInputs_[p.header.frame][p.inputPlayerId] = p.inputBits;
        if (config_.mode == Mode::Host && p.inputPlayerId != 0) {
            network_.broadcastReliable(serializePacket(p));
        }
        return;
    }

    if (type == PacketType::Garbage) {
        GarbagePacket p{};
        if (!deserializePacket(packet.payload.data(), packet.payload.size(), p)) {
            return;
        }
        garbagePacketsReceived_.fetch_add(1, std::memory_order_relaxed);
        if (config_.mode == Mode::Host && packet.peer != nullptr) {
            network_.broadcastReliableExcept(serializePacket(p), packet.peer);
        }
        return;
    }

    if (type == PacketType::Heartbeat) {
        HeartbeatPingPacket p{};
        if (!deserializePacket(packet.payload.data(), packet.payload.size(), p)) {
            return;
        }
        if (config_.mode == Mode::Host && packet.peer != nullptr) {
            HeartbeatEchoPacket echo{};
            echo.header.type = PacketType::HeartbeatEcho;
            echo.header.frame = p.header.frame;
            echo.header.playerId = 0;
            echo.header.timestampMs = p.header.timestampMs;
            network_.sendReliable(packet.peer, serializePacket(echo));
        }
        return;
    }

    if (type == PacketType::HeartbeatEcho) {
        HeartbeatEchoPacket p{};
        if (!deserializePacket(packet.payload.data(), packet.payload.size(), p)) {
            return;
        }
        const std::uint64_t now = SDL_GetTicks();
        if (now > p.header.timestampMs) {
            const int rtt = static_cast<int>(now - p.header.timestampMs);
            int prev = smoothedPingMs_.load(std::memory_order_relaxed);
            if (prev <= 0) {
                smoothedPingMs_.store(rtt, std::memory_order_relaxed);
            } else {
                smoothedPingMs_.store((prev * 3 + rtt) / 4, std::memory_order_relaxed);
            }
        }
        return;
    }

    if (type == PacketType::StateHash) {
        HashValidationPacket p{};
        if (!deserializePacket(packet.payload.data(), packet.payload.size(), p)) {
            return;
        }
        std::lock_guard<std::mutex> lock(stateMutex_);
        const int n = static_cast<int>(p.playerCount);
        for (int i = 0; i < n && i < kMaxHashPlayers && i < static_cast<int>(games_.size()); ++i) {
            if (games_[i].stateHash() != p.hashes[i]) {
                desyncEvents_.fetch_add(1, std::memory_order_relaxed);
                std::cerr << "Desync: player " << i << " hash mismatch at frame " << p.header.frame << "\n";
            }
        }
        return;
    }

    if (type == PacketType::MatchEnd) {
        std::cout << "Match end packet received\n";
        return;
    }
}

void App::simulationLoop() {
    using clock = std::chrono::steady_clock;
    const auto tick = std::chrono::milliseconds(1000 / config_.tickRate);
    auto next = clock::now();

    while (running_.load()) {
        if (!matchStarted_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        next += tick;

        const std::uint32_t frame = currentFrame_;
        if (isLocalTwoPlayer()) {
            const std::uint8_t p0 = inputBitsP0_.exchange(0U);
            const std::uint8_t p1 = inputBitsP1_.exchange(0U);
            submitLocalInput(frame, 0, p0);
            submitLocalInput(frame, 1, p1);
        } else {
            const std::uint8_t local = inputBits_.exchange(0U);
            submitLocalInput(frame, localPlayerId_, local);
            if (isMultiplayer()) {
                sendInputPacket(frame, local);
            }
        }

        if (isMultiplayer()) {
            auto waitStart = clock::now();
            while (running_.load() && !hasAllInputsForFrame(frame)) {
                if (clock::now() - waitStart > std::chrono::milliseconds(500)) {
                    std::cerr << "Lockstep timeout on frame " << frame << "\n";
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }

        std::vector<int> snapshotPlayers;
        std::unordered_map<int, TickResult> stepResults;

        {
            std::lock_guard<std::mutex> lock(stateMutex_);
            if (frameInputs_.count(frame) == 0) {
                continue;
            }

            snapshotPlayers = playerIds_;
            std::sort(snapshotPlayers.begin(), snapshotPlayers.end());

            stepResults.reserve(snapshotPlayers.size());
            for (int pid : snapshotPlayers) {
                const auto it = frameInputs_[frame].find(pid);
                const std::uint8_t bits = (it != frameInputs_[frame].end()) ? it->second : 0U;
                stepResults[pid] = games_[pid].step(bits);
            }

            for (int tgt : snapshotPlayers) {
                for (int atk : snapshotPlayers) {
                    if (atk == tgt) {
                        continue;
                    }
                    const int atkLines = stepResults[atk].attackLinesPerOpponent;
                    if (atkLines <= 0) {
                        continue;
                    }
                    games_[tgt].applyGarbageIncoming(sharedSeed_, frame, atk, tgt, 0, atkLines);
                }
            }

            if (localPlayerId_ >= 0 && localPlayerId_ < static_cast<int>(games_.size())) {
                hudCombo_.store(stepResults[localPlayerId_].comboDisplayed);
                hudGarbageOutbound_.store(stepResults[localPlayerId_].attackLinesPerOpponent);

                int inbound = 0;
                for (int atk : snapshotPlayers) {
                    if (atk != localPlayerId_) {
                        inbound += stepResults[atk].attackLinesPerOpponent;
                    }
                }
                hudGarbageInbound_.store(inbound);
            }

            frameInputs_.erase(frame);
        }

        if (isMultiplayer()) {
            for (int atk : snapshotPlayers) {
                if (atk != localPlayerId_) {
                    continue;
                }
                const int atkLines = stepResults[atk].attackLinesPerOpponent;
                if (atkLines <= 0) {
                    continue;
                }
                for (int tgt : snapshotPlayers) {
                    if (tgt == atk) {
                        continue;
                    }
                    sendGarbagePackets(frame, atk, tgt, atkLines);
                }
            }
        }

        simulationFramesExecuted_.fetch_add(1, std::memory_order_relaxed);

        if (isMultiplayer() && (frame % config_.hashIntervalFrames) == 0U) {
            HashValidationPacket hp{};
            hp.header.type = PacketType::StateHash;
            hp.header.frame = frame;
            hp.header.playerId = static_cast<std::uint8_t>(localPlayerId_);
            {
                std::lock_guard<std::mutex> lock(stateMutex_);
                const int n = (std::min)(static_cast<int>(games_.size()), kMaxHashPlayers);
                hp.playerCount = static_cast<std::uint8_t>(n);
                for (int i = 0; i < n; ++i) {
                    hp.hashes[i] = games_[i].stateHash();
                }
            }
            if (config_.mode == Mode::Host) {
                network_.broadcastReliable(serializePacket(hp));
            } else if (network_.serverPeer() != nullptr) {
                network_.sendReliable(network_.serverPeer(), serializePacket(hp));
            }
        }

        currentFrame_ += 1;
        std::this_thread::sleep_until(next);
    }
}

void App::submitLocalInput(std::uint32_t frame, int playerId, std::uint8_t bits) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    frameInputs_[frame][playerId] = bits;
}

void App::sendInputPacket(std::uint32_t frame, std::uint8_t bits) {
    if (!isMultiplayer()) {
        return;
    }
    InputPacket p{};
    p.header.type = PacketType::Input;
    p.header.frame = frame;
    p.header.playerId = static_cast<std::uint8_t>(localPlayerId_);
    p.inputBits = bits;
    p.inputPlayerId = static_cast<std::uint8_t>(localPlayerId_);

    if (config_.mode == Mode::Host) {
        network_.broadcastReliable(serializePacket(p));
    } else {
        network_.sendReliable(network_.serverPeer(), serializePacket(p));
    }
}

bool App::hasAllInputsForFrame(std::uint32_t frame) const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    const auto fit = frameInputs_.find(frame);
    if (fit == frameInputs_.end()) {
        return false;
    }
    for (int pid : playerIds_) {
        if (fit->second.find(pid) == fit->second.end()) {
            return false;
        }
    }
    return true;
}

void App::processSdlEvents() {
    SDL_Event e{};
    while (SDL_PollEvent(&e) != 0) {
        if (e.type == SDL_QUIT) {
            running_.store(false);
            return;
        }
        if (e.type == SDL_KEYDOWN) {
            const auto sym = e.key.keysym.sym;
            if (isLocalTwoPlayer()) {
                std::uint8_t p0 = inputBitsP0_.load();
                switch (sym) {
                    case SDLK_LEFT:
                        p0 |= MoveLeft;
                        break;
                    case SDLK_RIGHT:
                        p0 |= MoveRight;
                        break;
                    case SDLK_UP:
                    case SDLK_x:
                        p0 |= RotateCW;
                        break;
                    case SDLK_DOWN:
                        p0 |= SoftDrop;
                        break;
                    case SDLK_SPACE:
                        p0 |= HardDrop;
                        break;
                    case SDLK_c:
                        p0 |= Hold;
                        break;
                    default:
                        break;
                }
                inputBitsP0_.store(p0);

                std::uint8_t p1 = inputBitsP1_.load();
                switch (sym) {
                    case SDLK_a:
                        p1 |= MoveLeft;
                        break;
                    case SDLK_d:
                        p1 |= MoveRight;
                        break;
                    case SDLK_s:
                        p1 |= SoftDrop;
                        break;
                    case SDLK_f:
                        p1 |= RotateCW;
                        break;
                    case SDLK_g:
                        p1 |= HardDrop;
                        break;
                    case SDLK_h:
                        p1 |= Hold;
                        break;
                    default:
                        break;
                }
                inputBitsP1_.store(p1);
            } else {
                std::uint8_t b = inputBits_.load();
                switch (sym) {
                    case SDLK_LEFT:
                        b |= MoveLeft;
                        break;
                    case SDLK_RIGHT:
                        b |= MoveRight;
                        break;
                    case SDLK_UP:
                    case SDLK_x:
                        b |= RotateCW;
                        break;
                    case SDLK_DOWN:
                        b |= SoftDrop;
                        break;
                    case SDLK_SPACE:
                        b |= HardDrop;
                        break;
                    case SDLK_c:
                        b |= Hold;
                        break;
                    default:
                        break;
                }
                inputBits_.store(b);
            }
        }
    }
}

void App::renderFrame() {
    static int frames = 0;
    static std::uint32_t lastFpsTick = SDL_GetTicks();
    static int fps = 0;

    const std::uint32_t now = SDL_GetTicks();
    ++frames;
    if (now - lastFpsTick >= 1000U) {
        fps = frames;
        frames = 0;
        lastFpsTick = now;
        displayFps_.store(fps, std::memory_order_relaxed);
    }

    std::array<std::array<int, TetrisGame::BoardW>, TetrisGame::BoardH> localBoard{};
    std::array<std::array<int, TetrisGame::BoardW>, TetrisGame::BoardH> oppBoard{};
    std::vector<std::pair<int, int>> localActive;
    std::vector<std::pair<int, int>> oppActive;
    std::vector<std::pair<int, int>> nextBlocks;
    int localActiveType = 0;
    int oppActiveType = 0;
    int nextType = 0;
    int score = 0;
    int opponentId = -1;
    std::size_t nGamesSnap{0};
    std::array<bool, 16> playerDead{};
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        nGamesSnap = games_.size();
        for (std::size_t i = 0; i < nGamesSnap && i < playerDead.size(); ++i) {
            playerDead[i] = games_[i].gameOver();
        }
        if (localPlayerId_ < static_cast<int>(games_.size())) {
            localBoard = games_[localPlayerId_].board();
            localActive = games_[localPlayerId_].currentBlocks();
            localActiveType = games_[localPlayerId_].currentType() + 1;
            nextBlocks = games_[localPlayerId_].nextBlocksPreview();
            nextType = games_[localPlayerId_].nextType() + 1;
            score = games_[localPlayerId_].score();
        }
        if (static_cast<int>(games_.size()) >= 2) {
            for (int pid : playerIds_) {
                if (pid != localPlayerId_) {
                    opponentId = pid;
                    break;
                }
            }
            if (opponentId >= 0 && opponentId < static_cast<int>(games_.size())) {
                oppBoard = games_[opponentId].board();
                oppActive = games_[opponentId].currentBlocks();
                oppActiveType = games_[opponentId].currentType() + 1;
            }
        }
    }

    const bool showOpponent = opponentId >= 0;

    SDL_SetRenderDrawColor(renderer_, 20, 20, 24, 255);
    SDL_RenderClear(renderer_);

    const int localOx = showOpponent ? 28 : 80;
    const int localOy = BoardOffsetYLocal;
    const int localCs = showOpponent ? 24 : CellSizeLocal;
    drawBoardFrame(renderer_, localOx, localOy, localCs, SDL_Color{82, 200, 255, 255}, 6);
    const std::string leftLabel = isLocalTwoPlayer() ? "PLAYER 1" : "YOU";
    drawText(renderer_, localOx, localOy - 28, 2, leftLabel, SDL_Color{130, 220, 255, 255});
    drawBoardCells(renderer_, localOx, localOy, localCs, localBoard, &localActive, localActiveType);

    if (showOpponent) {
        constexpr int OppOx = 330;
        constexpr int OppOy = 110;
        constexpr int OppCs = 12;
        drawBoardFrame(renderer_, OppOx, OppOy, OppCs, SDL_Color{255, 150, 80, 255}, 5);
        const std::string rightLabel = isLocalTwoPlayer() ? "PLAYER 2" : "OPPONENT";
        drawText(renderer_, OppOx, OppOy - 28, 2, rightLabel, SDL_Color{255, 170, 120, 255});
        drawBoardCells(renderer_, OppOx, OppOy, OppCs, oppBoard, &oppActive, oppActiveType);
    }

    const int panelX = showOpponent ? 700 : 400;
    SDL_Rect panel{panelX, 40, 280, 260};
    SDL_SetRenderDrawColor(renderer_, 28, 28, 36, 255);
    SDL_RenderFillRect(renderer_, &panel);

    for (const auto& block : nextBlocks) {
        SDL_Rect r{
            panelX + 40 + block.first * 22,
            92 + block.second * 22,
            19,
            19};
        const std::uint8_t c = static_cast<std::uint8_t>(20 * nextType + 60);
        SDL_SetRenderDrawColor(renderer_, c, static_cast<std::uint8_t>(220 - c / 3), 240, 255);
        SDL_RenderFillRect(renderer_, &r);
    }

    const std::string modeText =
        (config_.mode == Mode::Solo) ? "MODE: SOLO" :
        (config_.mode == Mode::Local2P) ? "MODE: LOCAL 2P" :
        (config_.mode == Mode::Host) ? "MODE: HOST" :
        (config_.mode == Mode::Client) ? "MODE: CLIENT" : "MODE: MENU";

    const int hudPingNetwork = smoothedPingMs_.load(std::memory_order_relaxed);
    int hudPingBlend = hudPingNetwork;
    if (isMultiplayer()) {
        const std::uint32_t enetRtt = network_.clientRttMs();
        if (enetRtt > 0 && hudPingBlend > 0) {
            hudPingBlend = (hudPingBlend + static_cast<int>(enetRtt)) / 2;
        } else if (enetRtt > 0) {
            hudPingBlend = static_cast<int>(enetRtt);
        }
    }

    const std::uint64_t desyncs = desyncEvents_.load(std::memory_order_relaxed);
    const std::string syncLabel = desyncs > 0 ? "SYNC: CHECK" : "SYNC: OK";

    drawText(renderer_, panelX + 12, 50, 2, "NEXT", SDL_Color{220, 220, 230, 255});
    drawText(renderer_, panelX + 12, 218, 2, "SCORE: " + std::to_string(score), SDL_Color{180, 220, 120, 255});
    drawText(renderer_, panelX + 12, 246, 2, modeText, SDL_Color{120, 190, 250, 255});
    if (isLocalTwoPlayer()) {
        drawText(renderer_, panelX + 12, 262, 2, "P1 ARROWS ROT X SOFT DOWN", SDL_Color{130, 220, 255, 255});
        drawText(renderer_, panelX + 12, 278, 2, "P1 SPACE HARD C HOLD", SDL_Color{130, 220, 255, 255});
        drawText(renderer_, panelX + 12, 294, 2, "P2 A D MOVE S SOFT F ROT", SDL_Color{255, 170, 120, 255});
        drawText(renderer_, panelX + 12, 310, 2, "P2 G HARD H HOLD", SDL_Color{255, 170, 120, 255});
    }
    int yHud = isLocalTwoPlayer() ? 334 : 274;
    drawText(renderer_, panelX + 12, yHud, 2,
        "FPS " + std::to_string(displayFps_.load(std::memory_order_relaxed)), SDL_Color{250, 180, 120, 255});
    yHud += 24;
    drawText(renderer_, panelX + 12, yHud, 2,
        "TICK " + std::to_string(smoothedSimulationHz_.load(std::memory_order_relaxed)), SDL_Color{200, 200, 200, 255});
    yHud += 24;
    drawText(renderer_, panelX + 12, yHud, 2, "PING MS " + std::to_string(hudPingBlend), SDL_Color{255, 200, 140, 255});
    yHud += 24;
    drawText(renderer_, panelX + 12, yHud, 2, syncLabel, desyncs > 0 ? SDL_Color{255, 120, 120, 255} : SDL_Color{140, 255, 160, 255});
    yHud += 24;
    drawText(renderer_, panelX + 12, yHud, 2,
        "NET B/S " + std::to_string(netBytesPerSecApprox_.load(std::memory_order_relaxed)), SDL_Color{180, 180, 255, 255});
    yHud += 24;
    drawText(renderer_, panelX + 12, yHud, 2,
        "COMBO " + std::to_string(hudCombo_.load(std::memory_order_relaxed)), SDL_Color{255, 220, 160, 255});
    yHud += 24;
    drawText(renderer_, panelX + 12, yHud, 2,
        "GARB IN " + std::to_string(hudGarbageInbound_.load(std::memory_order_relaxed)), SDL_Color{255, 140, 140, 255});
    yHud += 24;
    drawText(renderer_, panelX + 12, yHud, 2,
        "GARB OUT " + std::to_string(hudGarbageOutbound_.load(std::memory_order_relaxed)), SDL_Color{200, 160, 255, 255});

    const bool matchActive = matchStarted_.load();
    int windowW = 1000;
    int windowH = WindowH;
    if (window_ != nullptr) {
        SDL_GetWindowSize(window_, &windowW, &windowH);
    }
    const int cx = windowW / 2;

    int survivors = 0;
    int lastSurvivor = -1;
    for (std::size_t i = 0; i < nGamesSnap && i < playerDead.size(); ++i) {
        if (!playerDead[i]) {
            survivors += 1;
            lastSurvivor = static_cast<int>(i);
        }
    }

    const SDL_Color colorWinTitle{255, 220, 120, 255};
    const SDL_Color colorLoseTitle{255, 90, 90, 255};
    const SDL_Color colorSub{220, 220, 235, 255};

    if (matchActive && nGamesSnap >= 1U) {
        if (config_.mode == Mode::Solo && playerDead[0]) {
            drawFullscreenDim(renderer_, windowW, windowH);
            drawTextCentered(renderer_, cx, windowH / 2 - 14, 4, "GAME OVER", colorLoseTitle);
        } else if (nGamesSnap >= 2U) {
            if (survivors == 0) {
                drawFullscreenDim(renderer_, windowW, windowH);
                drawTextCentered(renderer_, cx, windowH / 2 - 14, 4, "GAME OVER", colorLoseTitle);
            } else if (survivors == 1) {
                const int winnerId = lastSurvivor;
                const std::string winLine =
                    std::string("PLAYER ") + (winnerId >= 0 ? std::to_string(winnerId + 1) : std::string("?")) + " WINS";
                drawFullscreenDim(renderer_, windowW, windowH);

                if (isLocalTwoPlayer()) {
                    drawTextCentered(renderer_, cx, windowH / 2 - 48, 4, winLine, colorWinTitle);
                    drawTextCentered(renderer_, cx, windowH / 2 + 20, 4, "GAME OVER", colorLoseTitle);
                } else {
                    const bool iWon = (winnerId == localPlayerId_);
                    if (iWon) {
                        drawTextCentered(renderer_, cx, windowH / 2 - 56, 4, "WINNER", colorWinTitle);
                        const std::string meLine =
                            std::string("PLAYER ") + std::to_string(localPlayerId_ + 1);
                        drawTextCentered(renderer_, cx, windowH / 2 - 4, 3, meLine, colorSub);
                    } else {
                        drawTextCentered(renderer_, cx, windowH / 2 - 56, 4, "GAME OVER", colorLoseTitle);
                        drawTextCentered(renderer_, cx, windowH / 2 - 4, 3, winLine, colorSub);
                    }
                }
            }
        }
    }

    SDL_RenderPresent(renderer_);
}

void App::sendGarbagePackets(std::uint32_t effectiveFrame, int sourcePlayerId, int targetPlayerId, int lines) {
    if (!isMultiplayer() || lines <= 0) {
        return;
    }
    const int clamped = (std::min)(lines, 255);
    GarbagePacket gp{};
    gp.header.type = PacketType::Garbage;
    gp.header.frame = effectiveFrame;
    gp.header.playerId = static_cast<std::uint8_t>(localPlayerId_);
    gp.effectiveFrame = effectiveFrame;
    gp.sourcePlayerId = static_cast<std::uint8_t>(sourcePlayerId);
    gp.targetPlayerId = static_cast<std::uint8_t>(targetPlayerId);
    gp.lines = static_cast<std::uint8_t>(clamped);
    const auto payload = serializePacket(gp);
    garbagePacketsSent_.fetch_add(1, std::memory_order_relaxed);

    if (config_.mode == Mode::Host) {
        network_.broadcastReliable(payload);
    } else if (network_.serverPeer() != nullptr) {
        network_.sendReliable(network_.serverPeer(), payload);
    }
}

void App::sendHeartbeatPing() {
    if (!isMultiplayer() || config_.mode != Mode::Client) {
        return;
    }
    HeartbeatPingPacket p{};
    p.header.type = PacketType::Heartbeat;
    p.header.frame = currentFrame_;
    p.header.playerId = static_cast<std::uint8_t>(localPlayerId_);
    p.header.timestampMs = SDL_GetTicks();
    const auto payload = serializePacket(p);
    if (network_.serverPeer() != nullptr) {
        network_.sendReliable(network_.serverPeer(), payload);
    }
}

void App::metricsLoop() {
    using clock = std::chrono::steady_clock;
    constexpr auto interval = std::chrono::milliseconds(250);
    while (running_.load()) {
        const auto snapshot = simulationFramesExecuted_.load(std::memory_order_relaxed);
        const std::uint64_t bytes =
            static_cast<std::uint64_t>(network_.bytesSent()) + static_cast<std::uint64_t>(network_.bytesReceived());
        std::uint64_t simDelta = snapshot - lastMetricsSimFrames_;
        lastMetricsSimFrames_ = snapshot;
        if (simDelta > 2000ULL) {
            simDelta = 0;
        }
        const double hzApprox = static_cast<double>(simDelta) / 0.25;
        const int hz = static_cast<int>(hzApprox + 0.5);
        if (hz > 0) {
            const int prev = smoothedSimulationHz_.load(std::memory_order_relaxed);
            if (prev <= 0) {
                smoothedSimulationHz_.store(hz, std::memory_order_relaxed);
            } else {
                smoothedSimulationHz_.store((prev * 3 + hz) / 4, std::memory_order_relaxed);
            }
        }

        const std::uint64_t byteDelta = bytes - lastMetricsNetBytes_;
        lastMetricsNetBytes_ = bytes;
        netBytesPerSecApprox_.store(static_cast<std::uint64_t>(static_cast<double>(byteDelta) / 0.25), std::memory_order_relaxed);

        if (isMultiplayer()) {
            const std::uint32_t rtt = network_.clientRttMs();
            if (rtt > 0) {
                int prev = smoothedPingMs_.load(std::memory_order_relaxed);
                if (prev <= 0) {
                    smoothedPingMs_.store(static_cast<int>(rtt), std::memory_order_relaxed);
                } else {
                    smoothedPingMs_.store((prev * 3 + static_cast<int>(rtt)) / 4, std::memory_order_relaxed);
                }
            }
        }

        for (auto end = clock::now() + interval; running_.load() && clock::now() < end;) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }
}

} // namespace tetris
