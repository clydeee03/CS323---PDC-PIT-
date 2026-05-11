#pragma once

#include <array>
#include <cstdint>
#include <deque>
#include <utility>
#include <random>
#include <vector>

namespace tetris {

enum InputBits : std::uint8_t {
    MoveLeft = 1 << 0,
    MoveRight = 1 << 1,
    RotateCW = 1 << 2,
    SoftDrop = 1 << 3,
    HardDrop = 1 << 4,
    Hold = 1 << 5
};

struct TickResult {
    int linesCleared{0};
    bool gameOver{false};
    /// Garbage lines sent to each opponent after this simulated tick (0 = none).
    int attackLinesPerOpponent{0};
    int comboDisplayed{0};
    bool tetrisClear{false};
};

class TetrisGame {
public:
    static constexpr int BoardW = 10;
    static constexpr int BoardH = 20;
    static constexpr int GarbageCell = 20;

    explicit TetrisGame(std::uint32_t seed = 1337);

    void reset(std::uint32_t seed);
    TickResult step(std::uint8_t inputBits);

    /// Apply incoming garbage with deterministic synchronized holes (see GarbageSystem.hpp).
    void applyGarbageIncoming(std::uint32_t matchSeed, std::uint32_t effectiveFrame, int sourcePlayerId, int targetPlayerId,
                              int batchLineStartIndex, int lines);

    std::uint64_t stateHash() const;
    std::vector<std::pair<int, int>> currentBlocks() const;
    std::vector<std::pair<int, int>> nextBlocksPreview() const;
    int currentType() const { return current_.type; }
    int nextType() const { return queue_.empty() ? 0 : queue_.front(); }

    const std::array<std::array<int, BoardW>, BoardH>& board() const { return board_; }
    int score() const { return score_; }
    bool gameOver() const { return gameOver_; }
    int comboChain() const { return comboChain_; }
    int lastAttackLinesPerOpponent() const { return lastAttack_; }
    bool lastClearWasTetris() const { return lastClearWasQuad_; }

private:
    struct PieceState {
        int type{0};
        int rot{0};
        int x{3};
        int y{0};
    };

    bool canPlace(const PieceState& p) const;
    void lockPiece();
    int clearLines();
    void spawnNext();
    void refillBag();

    std::array<std::array<int, BoardW>, BoardH> board_{};
    std::deque<int> queue_;
    int holdType_{-1};
    bool holdUsed_{false};
    PieceState current_{};
    std::mt19937 rng_;
    int gravityCounter_{0};
    int score_{0};
    bool gameOver_{false};

    /// Consecutive clears (any line clear counts); resets on a lock without lines.
    int comboChain_{0};
    bool lastClearWasQuad_{false};
    int lastAttack_{0};
};

} // namespace tetris
