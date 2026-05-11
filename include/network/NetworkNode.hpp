#pragma once

#include "network/Packets.hpp"

#include <enet/enet.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace tetris {

struct ReceivedPacket {
    ENetPeer* peer{nullptr};
    std::vector<std::uint8_t> payload;
};

class NetworkNode {
public:
    NetworkNode() = default;
    ~NetworkNode();

    bool initialize();
    void shutdown();

    bool startHost(std::uint16_t port, int maxPlayers);
    bool startClient(const std::string& hostAddress, std::uint16_t port);

    bool poll(ReceivedPacket& outPacket, std::uint32_t timeoutMs);
    std::vector<ENetPeer*> connectedPeers() const;

    void sendReliable(ENetPeer* peer, const std::vector<std::uint8_t>& bytes);
    void broadcastReliable(const std::vector<std::uint8_t>& bytes);
    void broadcastReliableExcept(const std::vector<std::uint8_t>& bytes, ENetPeer* exceptPeer);

    ENetPeer* serverPeer() const { return serverPeer_; }
    bool isHost() const { return isHost_; }

    std::uint64_t packetsSent() const { return packetsSent_.load(std::memory_order_relaxed); }
    std::uint64_t packetsReceived() const { return packetsReceived_.load(std::memory_order_relaxed); }
    std::uint32_t bytesSent() const { return bytesSent_.load(std::memory_order_relaxed); }
    std::uint32_t bytesReceived() const { return bytesReceived_.load(std::memory_order_relaxed); }

    std::uint32_t clientRttMs() const;

private:
    ENetHost* host_{nullptr};
    ENetPeer* serverPeer_{nullptr};
    bool isHost_{false};
    mutable std::mutex peersMutex_;
    std::vector<ENetPeer*> peers_;
    std::atomic<std::uint64_t> packetsSent_{0};
    std::atomic<std::uint64_t> packetsReceived_{0};
    std::atomic<std::uint32_t> bytesSent_{0};
    std::atomic<std::uint32_t> bytesReceived_{0};
};

} // namespace tetris
