#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

namespace tetris {

enum class PacketType : std::uint8_t {
    Input = 1,
    MatchStart = 2,
    PlayerJoin = 3,
    PlayerLeave = 4,
    Garbage = 5,
    Heartbeat = 6,
    StateHash = 7,
    MatchEnd = 8,
    HeartbeatEcho = 9,
    PlayerStatus = 10
};

inline constexpr int kMaxHashPlayers = 16;

struct PacketHeader {
    PacketType type{PacketType::Heartbeat};
    std::uint32_t frame{0};
    std::uint8_t playerId{0};
    std::uint64_t timestampMs{0};
};

struct InputPacket {
    PacketHeader header{};
    std::uint8_t inputBits{0};
    std::uint8_t inputPlayerId{0};
};

struct MatchStartPacket {
    PacketHeader header{};
    std::uint32_t seed{0};
    std::uint32_t startFrame{0};
    std::uint8_t playerCount{0};
};

struct HashValidationPacket {
    PacketHeader header{};
    std::uint8_t playerCount{0};
    std::uint8_t reserved[7]{};
    std::uint64_t hashes[kMaxHashPlayers]{};
};

struct GarbagePacket {
    PacketHeader header{};
    std::uint32_t effectiveFrame{0};
    std::uint8_t sourcePlayerId{0};
    std::uint8_t targetPlayerId{0};
    std::uint8_t lines{0};
    std::uint8_t reserved{0};
};

struct MatchEndPacket {
    PacketHeader header{};
    std::int8_t winnerPlayerId{-1};
    std::uint8_t reason{0};
    std::uint8_t padding[6]{};
};

struct HeartbeatPingPacket {
    PacketHeader header{};
};

struct HeartbeatEchoPacket {
    PacketHeader header{};
};

struct PlayerStatusPacket {
    PacketHeader header{};
    std::uint8_t subjectPlayerId{0};
    std::uint8_t alive{1};
    std::uint8_t padding[6]{};
};

template <typename T>
inline std::vector<std::uint8_t> serializePacket(const T& packet) {
    std::vector<std::uint8_t> bytes(sizeof(T));
    std::memcpy(bytes.data(), &packet, sizeof(T));
    return bytes;
}

template <typename T>
inline bool deserializePacket(const std::uint8_t* data, std::size_t size, T& outPacket) {
    if (size != sizeof(T)) {
        return false;
    }
    std::memcpy(&outPacket, data, sizeof(T));
    return true;
}

} // namespace tetris
