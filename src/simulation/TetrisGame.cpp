#include "simulation/TetrisGame.hpp"
#include "simulation/GarbageSystem.hpp"

#include <algorithm>
#include <future>

namespace tetris {

namespace {
constexpr int GravityFrames = 15;

int baseGarbageFromLines(int lc) noexcept {
    switch (lc) {
        case 1:
            return 0;
        case 2:
            return 1;
        case 3:
            return 2;
        case 4:
            return 4;
        default:
            return 0;
    }
}
constexpr int PieceShapes[7][4][4][4] = {
    { // I
        {{0, 0, 0, 0}, {1, 1, 1, 1}, {0, 0, 0, 0}, {0, 0, 0, 0}},
        {{0, 0, 1, 0}, {0, 0, 1, 0}, {0, 0, 1, 0}, {0, 0, 1, 0}},
        {{0, 0, 0, 0}, {1, 1, 1, 1}, {0, 0, 0, 0}, {0, 0, 0, 0}},
        {{0, 1, 0, 0}, {0, 1, 0, 0}, {0, 1, 0, 0}, {0, 1, 0, 0}},
    },
    { // O
        {{0, 1, 1, 0}, {0, 1, 1, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}},
        {{0, 1, 1, 0}, {0, 1, 1, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}},
        {{0, 1, 1, 0}, {0, 1, 1, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}},
        {{0, 1, 1, 0}, {0, 1, 1, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}},
    },
    { // T
        {{0, 1, 0, 0}, {1, 1, 1, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}},
        {{0, 1, 0, 0}, {0, 1, 1, 0}, {0, 1, 0, 0}, {0, 0, 0, 0}},
        {{0, 0, 0, 0}, {1, 1, 1, 0}, {0, 1, 0, 0}, {0, 0, 0, 0}},
        {{0, 1, 0, 0}, {1, 1, 0, 0}, {0, 1, 0, 0}, {0, 0, 0, 0}},
    },
    { // S
        {{0, 1, 1, 0}, {1, 1, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}},
        {{0, 1, 0, 0}, {0, 1, 1, 0}, {0, 0, 1, 0}, {0, 0, 0, 0}},
        {{0, 1, 1, 0}, {1, 1, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}},
        {{0, 1, 0, 0}, {0, 1, 1, 0}, {0, 0, 1, 0}, {0, 0, 0, 0}},
    },
    { // Z
        {{1, 1, 0, 0}, {0, 1, 1, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}},
        {{0, 0, 1, 0}, {0, 1, 1, 0}, {0, 1, 0, 0}, {0, 0, 0, 0}},
        {{1, 1, 0, 0}, {0, 1, 1, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}},
        {{0, 0, 1, 0}, {0, 1, 1, 0}, {0, 1, 0, 0}, {0, 0, 0, 0}},
    },
    { // J
        {{1, 0, 0, 0}, {1, 1, 1, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}},
        {{0, 1, 1, 0}, {0, 1, 0, 0}, {0, 1, 0, 0}, {0, 0, 0, 0}},
        {{0, 0, 0, 0}, {1, 1, 1, 0}, {0, 0, 1, 0}, {0, 0, 0, 0}},
        {{0, 1, 0, 0}, {0, 1, 0, 0}, {1, 1, 0, 0}, {0, 0, 0, 0}},
    },
    { // L
        {{0, 0, 1, 0}, {1, 1, 1, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}},
        {{0, 1, 0, 0}, {0, 1, 0, 0}, {0, 1, 1, 0}, {0, 0, 0, 0}},
        {{0, 0, 0, 0}, {1, 1, 1, 0}, {1, 0, 0, 0}, {0, 0, 0, 0}},
        {{1, 1, 0, 0}, {0, 1, 0, 0}, {0, 1, 0, 0}, {0, 0, 0, 0}},
    },
};
} // namespace

TetrisGame::TetrisGame(std::uint32_t seed) : rng_(seed) {
    reset(seed);
}

void TetrisGame::reset(std::uint32_t seed) {
    for (auto& row : board_) {
        row.fill(0);
    }
    queue_.clear();
    holdType_ = -1;
    holdUsed_ = false;
    rng_.seed(seed);
    gravityCounter_ = 0;
    score_ = 0;
    gameOver_ = false;
    comboChain_ = 0;
    lastClearWasQuad_ = false;
    lastAttack_ = 0;
    refillBag();
    spawnNext();
}

void TetrisGame::refillBag() {
    std::array<int, 7> bag{0, 1, 2, 3, 4, 5, 6};
    std::shuffle(bag.begin(), bag.end(), rng_);
    for (int p : bag) {
        queue_.push_back(p);
    }
}

void TetrisGame::spawnNext() {
    if (queue_.size() < 7) {
        refillBag();
    }
    current_.type = queue_.front();
    queue_.pop_front();
    current_.rot = 0;
    current_.x = 3;
    current_.y = 0;
    holdUsed_ = false;
    if (!canPlace(current_)) {
        gameOver_ = true;
    }
}

bool TetrisGame::canPlace(const PieceState& p) const {
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            if (PieceShapes[p.type][p.rot][y][x] == 0) {
                continue;
            }
            const int bx = p.x + x;
            const int by = p.y + y;
            if (bx < 0 || bx >= BoardW || by >= BoardH) {
                return false;
            }
            if (by >= 0 && board_[by][bx] != 0) {
                return false;
            }
        }
    }
    return true;
}

void TetrisGame::lockPiece() {
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            if (PieceShapes[current_.type][current_.rot][y][x] == 0) {
                continue;
            }
            const int bx = current_.x + x;
            const int by = current_.y + y;
            if (by >= 0 && by < BoardH && bx >= 0 && bx < BoardW) {
                board_[by][bx] = current_.type + 1;
            }
        }
    }
}

int TetrisGame::clearLines() {
    // Parallel row-full scan; compaction remains ordered/deterministic.
    auto scanRange = [this](int startY, int endY) {
        std::array<bool, BoardH> local{};
        local.fill(false);
        for (int y = startY; y < endY; ++y) {
            bool full = true;
            for (int x = 0; x < BoardW; ++x) {
                if (board_[y][x] == 0) {
                    full = false;
                    break;
                }
            }
            local[y] = full;
        }
        return local;
    };

    auto topTask = std::async(std::launch::async, scanRange, 0, BoardH / 2);
    const auto bottomRows = scanRange(BoardH / 2, BoardH);
    const auto topRows = topTask.get();

    std::array<bool, BoardH> fullRows{};
    for (int y = 0; y < BoardH; ++y) {
        fullRows[y] = topRows[y] || bottomRows[y];
    }

    int cleared = 0;
    int writeY = BoardH - 1;
    for (int y = BoardH - 1; y >= 0; --y) {
        if (fullRows[y]) {
            ++cleared;
            continue;
        }
        board_[writeY] = board_[y];
        --writeY;
    }
    while (writeY >= 0) {
        board_[writeY].fill(0);
        --writeY;
    }
    return cleared;
}

TickResult TetrisGame::step(std::uint8_t inputBits) {
    if (gameOver_) {
        return {.linesCleared = 0, .gameOver = true, .attackLinesPerOpponent = 0, .comboDisplayed = 0, .tetrisClear = false};
    }

    auto moved = current_;
    if ((inputBits & MoveLeft) != 0U) {
        moved.x -= 1;
        if (canPlace(moved)) {
            current_ = moved;
        }
        moved = current_;
    }
    if ((inputBits & MoveRight) != 0U) {
        moved.x += 1;
        if (canPlace(moved)) {
            current_ = moved;
        }
        moved = current_;
    }
    if ((inputBits & RotateCW) != 0U) {
        moved.rot = (moved.rot + 1) % 4;
        if (canPlace(moved)) {
            current_ = moved;
        }
        moved = current_;
    }
    if ((inputBits & Hold) != 0U && !holdUsed_) {
        holdUsed_ = true;
        if (holdType_ < 0) {
            holdType_ = current_.type;
            spawnNext();
        } else {
            std::swap(holdType_, current_.type);
            current_.rot = 0;
            current_.x = 3;
            current_.y = 0;
            if (!canPlace(current_)) {
                gameOver_ = true;
            }
        }
    }

    bool forceLock = false;
    if ((inputBits & HardDrop) != 0U) {
        auto drop = current_;
        while (canPlace(drop)) {
            current_ = drop;
            drop.y += 1;
        }
        forceLock = true;
    } else {
        if ((inputBits & SoftDrop) != 0U) {
            gravityCounter_ = GravityFrames;
        }
        gravityCounter_ += 1;
        if (gravityCounter_ >= GravityFrames) {
            gravityCounter_ = 0;
            moved = current_;
            moved.y += 1;
            if (canPlace(moved)) {
                current_ = moved;
            } else {
                forceLock = true;
            }
        }
    }

    TickResult tick{.linesCleared = 0, .gameOver = gameOver_, .attackLinesPerOpponent = 0, .comboDisplayed = 0, .tetrisClear = false};
    if (forceLock) {
        lockPiece();
        tick.linesCleared = clearLines();
        const int lc = tick.linesCleared;

        score_ += lc * 100 + lc * lc * 10;

        if (lc <= 0) {
            comboChain_ = 0;
        }
        tick.comboDisplayed = 0;

        tick.tetrisClear = (lc == 4);
        const bool prevQuad = lastClearWasQuad_;

        int attack = 0;
        if (lc > 0) {
            comboChain_ += 1;
            attack = baseGarbageFromLines(lc);
            if (comboChain_ >= 3) {
                attack += (comboChain_ - 2);
            }
            if (lc == 4 && prevQuad) {
                attack += 2;
            }
            if (attack < 0) {
                attack = 0;
            }
            if (attack > 24) {
                attack = 24;
            }

            tick.attackLinesPerOpponent = attack;
            tick.comboDisplayed = (comboChain_ > 1) ? (comboChain_ - 1) : 0;
        }

        lastClearWasQuad_ = (lc == 4);
        lastAttack_ = tick.attackLinesPerOpponent;

        spawnNext();
        tick.gameOver = gameOver_;
        return tick;
    }

    tick.gameOver = gameOver_;
    return tick;
}

void TetrisGame::applyGarbageIncoming(std::uint32_t matchSeed, std::uint32_t effectiveFrame, int sourcePlayerId, int targetPlayerId,
                                      int batchLineStartIndex, int lines) {
    if (lines <= 0 || gameOver_) {
        return;
    }
    for (int i = 0; i < lines; ++i) {
        for (int y = 0; y < BoardH - 1; ++y) {
            board_[y] = board_[y + 1];
        }
        const int hole = garbageHoleColumn(matchSeed, effectiveFrame, sourcePlayerId, targetPlayerId, batchLineStartIndex + i, BoardW);
        auto& bottom = board_[BoardH - 1];
        for (int x = 0; x < BoardW; ++x) {
            bottom[x] = GarbageCell;
        }
        bottom[hole] = 0;
    }
}

std::uint64_t TetrisGame::stateHash() const {
    constexpr std::uint64_t prime = 1099511628211ULL;
    std::uint64_t h = 1469598103934665603ULL;
    for (const auto& row : board_) {
        for (int v : row) {
            h ^= static_cast<std::uint64_t>(v + 31);
            h *= prime;
        }
    }
    h ^= static_cast<std::uint64_t>(score_);
    h *= prime;
    h ^= static_cast<std::uint64_t>(current_.x + current_.y * 17 + current_.rot * 131 + current_.type * 919);
    h *= prime;
    h ^= static_cast<std::uint64_t>(holdType_ + 511);
    h *= prime;
    h ^= static_cast<std::uint64_t>(gravityCounter_);
    h *= prime;
    h ^= static_cast<std::uint64_t>(comboChain_ & 255);
    h *= prime;
    h ^= static_cast<std::uint64_t>(lastClearWasQuad_ ? 997u : 0u);
    h *= prime;
    for (int v : queue_) {
        h ^= static_cast<std::uint64_t>(v + 3);
        h *= prime;
    }
    return h;
}

std::vector<std::pair<int, int>> TetrisGame::currentBlocks() const {
    std::vector<std::pair<int, int>> blocks;
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            if (PieceShapes[current_.type][current_.rot][y][x] != 0) {
                blocks.emplace_back(current_.x + x, current_.y + y);
            }
        }
    }
    return blocks;
}

std::vector<std::pair<int, int>> TetrisGame::nextBlocksPreview() const {
    const int next = queue_.empty() ? 0 : queue_.front();
    std::vector<std::pair<int, int>> blocks;
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            if (PieceShapes[next][0][y][x] != 0) {
                blocks.emplace_back(x, y);
            }
        }
    }
    return blocks;
}

} // namespace tetris
