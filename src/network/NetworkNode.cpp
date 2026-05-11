#include "network/NetworkNode.hpp"

#include <algorithm>
#include <cstring>
#include <iostream>

namespace tetris {

NetworkNode::~NetworkNode() {
    shutdown();
}

bool NetworkNode::initialize() {
    return enet_initialize() == 0;
}

void NetworkNode::shutdown() {
    if (host_ != nullptr) {
        enet_host_destroy(host_);
        host_ = nullptr;
    }
    serverPeer_ = nullptr;
    {
        std::lock_guard<std::mutex> lock(peersMutex_);
        peers_.clear();
    }
    enet_deinitialize();
}

bool NetworkNode::startHost(std::uint16_t port, int maxPlayers) {
    isHost_ = true;
    ENetAddress address{};
    address.host = ENET_HOST_ANY;
    address.port = port;
    host_ = enet_host_create(&address, static_cast<size_t>(maxPlayers), 2, 0, 0);
    return host_ != nullptr;
}

bool NetworkNode::startClient(const std::string& hostAddress, std::uint16_t port) {
    isHost_ = false;
    host_ = enet_host_create(nullptr, 1, 2, 0, 0);
    if (host_ == nullptr) {
        return false;
    }

    ENetAddress address{};
    enet_address_set_host(&address, hostAddress.c_str());
    address.port = port;
    serverPeer_ = enet_host_connect(host_, &address, 2, 0);
    return serverPeer_ != nullptr;
}

bool NetworkNode::poll(ReceivedPacket& outPacket, std::uint32_t timeoutMs) {
    if (host_ == nullptr) {
        return false;
    }

    ENetEvent event{};
    if (enet_host_service(host_, &event, timeoutMs) <= 0) {
        return false;
    }

    switch (event.type) {
        case ENET_EVENT_TYPE_CONNECT: {
            if (isHost_) {
                std::lock_guard<std::mutex> lock(peersMutex_);
                peers_.push_back(event.peer);
                std::cout << "Client connected\n";
            } else {
                serverPeer_ = event.peer;
                std::cout << "Connected to host\n";
            }
            return false;
        }
        case ENET_EVENT_TYPE_DISCONNECT: {
            if (isHost_) {
                std::lock_guard<std::mutex> lock(peersMutex_);
                peers_.erase(
                    std::remove(peers_.begin(), peers_.end(), event.peer),
                    peers_.end());
                std::cout << "Client disconnected\n";
            } else {
                serverPeer_ = nullptr;
                std::cout << "Disconnected from host\n";
            }
            return false;
        }
        case ENET_EVENT_TYPE_RECEIVE: {
            packetsReceived_.fetch_add(1, std::memory_order_relaxed);
            bytesReceived_.fetch_add(static_cast<std::uint32_t>(event.packet->dataLength), std::memory_order_relaxed);
            outPacket.peer = event.peer;
            outPacket.payload.resize(event.packet->dataLength);
            std::memcpy(outPacket.payload.data(), event.packet->data, event.packet->dataLength);
            enet_packet_destroy(event.packet);
            return true;
        }
        default:
            return false;
    }
}

std::vector<ENetPeer*> NetworkNode::connectedPeers() const {
    std::lock_guard<std::mutex> lock(peersMutex_);
    return peers_;
}

void NetworkNode::sendReliable(ENetPeer* peer, const std::vector<std::uint8_t>& bytes) {
    if (host_ == nullptr || peer == nullptr) {
        return;
    }
    ENetPacket* packet = enet_packet_create(bytes.data(), bytes.size(), ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(peer, 0, packet);
    enet_host_flush(host_);
    packetsSent_.fetch_add(1, std::memory_order_relaxed);
    bytesSent_.fetch_add(static_cast<std::uint32_t>(bytes.size()), std::memory_order_relaxed);
}

void NetworkNode::broadcastReliable(const std::vector<std::uint8_t>& bytes) {
    if (host_ == nullptr) {
        return;
    }
    ENetPacket* packet = enet_packet_create(bytes.data(), bytes.size(), ENET_PACKET_FLAG_RELIABLE);
    enet_host_broadcast(host_, 0, packet);
    enet_host_flush(host_);
    packetsSent_.fetch_add(1, std::memory_order_relaxed);
    bytesSent_.fetch_add(static_cast<std::uint32_t>(bytes.size()), std::memory_order_relaxed);
}

void NetworkNode::broadcastReliableExcept(const std::vector<std::uint8_t>& bytes, ENetPeer* exceptPeer) {
    if (host_ == nullptr) {
        return;
    }
    std::vector<ENetPeer*> peers;
    {
        std::lock_guard<std::mutex> lock(peersMutex_);
        peers = peers_;
    }
    std::uint64_t sent = 0;
    std::uint64_t byteTotal = 0;
    for (ENetPeer* peer : peers) {
        if (peer == nullptr || peer == exceptPeer) {
            continue;
        }
        ENetPacket* packet = enet_packet_create(bytes.data(), bytes.size(), ENET_PACKET_FLAG_RELIABLE);
        enet_peer_send(peer, 0, packet);
        ++sent;
        byteTotal += bytes.size();
    }
    enet_host_flush(host_);
    packetsSent_.fetch_add(sent, std::memory_order_relaxed);
    bytesSent_.fetch_add(static_cast<std::uint32_t>(byteTotal), std::memory_order_relaxed);
}

std::uint32_t NetworkNode::clientRttMs() const {
    if (serverPeer_ == nullptr) {
        return 0;
    }
    return static_cast<std::uint32_t>(serverPeer_->roundTripTime);
}

} // namespace tetris
