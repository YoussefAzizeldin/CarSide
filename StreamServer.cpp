// StreamServer.cpp — async Winsock2 TCP server with Bonjour registration
#include <winsock2.h>
#include <ws2tcpip.h>
#include <dns_sd.h>
#include <cstdint>
#include <vector>
#include <functional>
#include <thread>
#include <atomic>
#include <chrono>

#include "InputEvent.h"

class StreamServer {
    SOCKET m_listenSock = INVALID_SOCKET;
    SOCKET m_clientSock = INVALID_SOCKET;
    std::atomic<bool> m_running{false};
    std::thread m_acceptThread;
    std::thread m_recvThread;
    DNSServiceRef m_bonjourRef = nullptr;
    std::function<void(const InputEvent&)> m_inputCallback;

    // Bonjour callback
    static void DNSSD_API BonjourCallback(DNSServiceRef sdRef, DNSServiceFlags flags,
                                          uint32_t interfaceIndex, DNSServiceErrorType error,
                                          const char* serviceName, const char* regType,
                                          const char* domain, void* context) {
        if (error == kDNSServiceErr_NoError) {
            printf("[Bonjour] Service '%s' registered successfully\n", serviceName);
        } else {
            printf("[Bonjour] Registration error: %d\n", error);
        }
    }

public:
    bool Start(uint16_t port = 7878) {
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
            printf("[StreamServer] WSAStartup failed\n");
            return false;
        }

        m_listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (m_listenSock == INVALID_SOCKET) {
            printf("[StreamServer] Socket creation failed\n");
            return false;
        }

        // Enable socket reuse to avoid TIME_WAIT issues
        int reuseAddr = 1;
        if (setsockopt(m_listenSock, SOL_SOCKET, SO_REUSEADDR,
                       (char*)&reuseAddr, sizeof(reuseAddr)) < 0) {
            printf("[StreamServer] setsockopt SO_REUSEADDR failed\n");
        }

        // Bind to all interfaces
        sockaddr_in addr = {};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = INADDR_ANY;

        if (bind(m_listenSock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
            printf("[StreamServer] Bind failed on port %d\n", port);
            closesocket(m_listenSock);
            return false;
        }

        if (listen(m_listenSock, 1) == SOCKET_ERROR) {
            printf("[StreamServer] Listen failed\n");
            return false;
        }

        // TCP_NODELAY for minimum latency
        int flag = 1;
        setsockopt(m_listenSock, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(flag));

        // SO_KEEPALIVE to detect disconnections
        setsockopt(m_listenSock, SOL_SOCKET, SO_KEEPALIVE, (char*)&flag, sizeof(flag));

        m_running = true;

        // Start accept thread
        m_acceptThread = std::thread([this]() { this->AcceptLoop(); });

        // Register Bonjour service
        if (!RegisterBonjourService(port)) {
            printf("[StreamServer] Bonjour registration failed\n");
        }

        printf("[StreamServer] Started on port %d\n", port);
        return true;
    }

    // Advertise the service via Bonjour (mDNS)
    bool RegisterBonjourService(uint16_t port) {
        DNSServiceErrorType err = DNSServiceRegister(
            &m_bonjourRef,                       // serviceRef
            0,                                    // flags: default
            0,                                    // interfaceIndex: all
            "CarSide",                            // name
            "_winextend._tcp",                    // regType
            "local.",                             // domain
            nullptr,                              // host (nullptr = auto)
            htons(port),                          // port
            0,                                    // txtLen
            nullptr,                              // txtRecord
            BonjourCallback,                      // callback
            this                                  // context
        );

        if (err == kDNSServiceErr_NoError) {
            printf("[Bonjour] Service registered as 'CarSide._winextend._tcp.local.:%d'\n", port);
            return true;
        } else {
            printf("[Bonjour] Registration failed with error: %d\n", err);
            return false;
        }
    }

    // Accept incoming client connection
    void AcceptLoop() {
        printf("[StreamServer] Waiting for client connection...\n");
        m_clientSock = accept(m_listenSock, nullptr, nullptr);

        if (m_clientSock != INVALID_SOCKET) {
            printf("[StreamServer] Client connected\n");
            // Start receiving input in separate thread
            m_recvThread = std::thread([this]() { this->RecvInputLoop(); });
        } else {
            printf("[StreamServer] Accept failed\n");
        }
    }

    // Frame packet: [4-byte length][H.264 NAL data]
    bool SendFrame(const std::vector<uint8_t>& nalData) {
        if (m_clientSock == INVALID_SOCKET) {
            return false;  // Not connected
        }

        uint32_t len = htonl((uint32_t)nalData.size());

        // Send length
        if (send(m_clientSock, (char*)&len, 4, 0) == SOCKET_ERROR) {
            printf("[StreamServer] Send length failed, client disconnected\n");
            m_clientSock = INVALID_SOCKET;
            return false;
        }

        // Send data
        int totalSent = 0;
        while (totalSent < (int)nalData.size()) {
            int sent = send(m_clientSock, (char*)(nalData.data() + totalSent),
                           (int)nalData.size() - totalSent, 0);
            if (sent == SOCKET_ERROR) {
                printf("[StreamServer] Send data failed\n");
                m_clientSock = INVALID_SOCKET;
                return false;
            }
            totalSent += sent;
        }

        return true;
    }

    // Receive input events from iPad client
    void RecvInputLoop() {
        printf("[StreamServer] Input receiver started\n");

        while (m_running && m_clientSock != INVALID_SOCKET) {
            uint32_t len = 0;

            // Receive message length
            int nRecv = recv(m_clientSock, (char*)&len, sizeof(len), MSG_WAITALL);
            if (nRecv != sizeof(len)) {
                printf("[StreamServer] Client disconnected during length recv\n");
                break;
            }

            len = ntohl(len);

            if (len > 65536) {  // Sanity check
                printf("[StreamServer] Invalid message length: %lu\n", len);
                break;
            }

            std::vector<uint8_t> buf(len);
            nRecv = recv(m_clientSock, (char*)buf.data(), len, MSG_WAITALL);

            if (nRecv != (int)len) {
                printf("[StreamServer] Incomplete message received\n");
                break;
            }

            // Parse InputEvent from buffer (JSON or binary format)
            if (m_inputCallback) {
                // Deserialize from buf to InputEvent and call callback
                // For now, assume binary format matches InputEvent struct
                if (len >= sizeof(InputEvent)) {
                    const InputEvent& evt = *(InputEvent*)buf.data();
                    m_inputCallback(evt);
                }
            }
        }

        printf("[StreamServer] Input receiver stopped\n");
    }

    // Set input event callback
    void SetInputCallback(std::function<void(const InputEvent&)> cb) {
        m_inputCallback = cb;
    }

    // Check if client is connected
    bool IsConnected() const {
        return m_clientSock != INVALID_SOCKET;
    }

    // Shutdown server
    void Stop() {
        m_running = false;

        if (m_clientSock != INVALID_SOCKET) {
            closesocket(m_clientSock);
            m_clientSock = INVALID_SOCKET;
        }

        if (m_listenSock != INVALID_SOCKET) {
            closesocket(m_listenSock);
            m_listenSock = INVALID_SOCKET;
        }

        // Deregister Bonjour service
        if (m_bonjourRef) {
            DNSServiceRefDeallocate(m_bonjourRef);
            m_bonjourRef = nullptr;
        }

        WSACleanup();

        if (m_acceptThread.joinable()) m_acceptThread.join();
        if (m_recvThread.joinable()) m_recvThread.join();

        printf("[StreamServer] Stopped\n");
    }

    ~StreamServer() {
        Stop();
    }
};
            InputEvent evt = InputEvent::Deserialize(buf);
            cb(evt);
        }
    }
};