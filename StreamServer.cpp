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
#include <iostream>

#include "InputEvent.h"

class StreamServer {
    SOCKET m_listenSock = INVALID_SOCKET;
    SOCKET m_clientSock = INVALID_SOCKET;
    std::atomic<bool> m_running{false};
    std::thread m_acceptThread;
    std::thread m_recvThread;
    DNSServiceRef m_bonjourRef = nullptr;
    std::function<void(const InputEvent&)> m_inputCallback;

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

    void AcceptLoop() {
        printf("[StreamServer] Waiting for client connection...\n");

        while (m_running) {
            m_clientSock = accept(m_listenSock, nullptr, nullptr);
            if (m_clientSock != INVALID_SOCKET) {
                printf("[StreamServer] Client connected\n");
                m_recvThread = std::thread([this]() { this->RecvInputLoop(); });
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
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
            WSACleanup();
            return false;
        }

        int reuseAddr = 1;
        setsockopt(m_listenSock, SOL_SOCKET, SO_REUSEADDR, (char*)&reuseAddr, sizeof(reuseAddr));

        sockaddr_in addr = {};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = INADDR_ANY;

        if (bind(m_listenSock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
            printf("[StreamServer] Bind failed on port %d\n", port);
            closesocket(m_listenSock);
            WSACleanup();
            return false;
        }

        if (listen(m_listenSock, 1) == SOCKET_ERROR) {
            printf("[StreamServer] Listen failed\n");
            closesocket(m_listenSock);
            WSACleanup();
            return false;
        }

        int flag = 1;
        setsockopt(m_listenSock, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(flag));
        setsockopt(m_listenSock, SOL_SOCKET, SO_KEEPALIVE, (char*)&flag, sizeof(flag));

        m_running = true;
        m_acceptThread = std::thread([this]() { this->AcceptLoop(); });

        if (!RegisterBonjourService(port)) {
            printf("[StreamServer] Bonjour registration failed\n");
        }

        printf("[StreamServer] Started on port %d\n", port);
        return true;
    }

    bool RegisterBonjourService(uint16_t port) {
        DNSServiceErrorType err = DNSServiceRegister(
            &m_bonjourRef,
            0,
            0,
            "CarSide",
            "_winextend._tcp",
            "local.",
            nullptr,
            htons(port),
            0,
            nullptr,
            BonjourCallback,
            this);

        if (err == kDNSServiceErr_NoError) {
            printf("[Bonjour] Service registered as 'CarSide._winextend._tcp.local.:%d'\n", port);
            return true;
        }

        printf("[Bonjour] Registration failed with error: %d\n", err);
        return false;
    }

    bool SendFrame(const std::vector<uint8_t>& nalData) {
        if (m_clientSock == INVALID_SOCKET) return false;

        uint32_t len = htonl((uint32_t)nalData.size());
        int totalSent = 0;

        // send length
        while (totalSent < 4) {
            int sent = send(m_clientSock, ((char*)&len) + totalSent, 4 - totalSent, 0);
            if (sent == SOCKET_ERROR) {
                m_clientSock = INVALID_SOCKET;
                return false;
            }
            totalSent += sent;
        }

        // send payload
        totalSent = 0;
        while (totalSent < (int)nalData.size()) {
            int sent = send(m_clientSock, (char*)nalData.data() + totalSent, (int)nalData.size() - totalSent, 0);
            if (sent == SOCKET_ERROR) {
                m_clientSock = INVALID_SOCKET;
                return false;
            }
            totalSent += sent;
        }

        return true;
    }

    void RecvInputLoop() {
        printf("[StreamServer] Input receiver started\n");

        while (m_running && m_clientSock != INVALID_SOCKET) {
            uint32_t len = 0;
            int nRecv = recv(m_clientSock, (char*)&len, sizeof(len), MSG_WAITALL);

            if (nRecv != sizeof(len)) {
                printf("[StreamServer] Client disconnected or incomplete length\n");
                break;
            }

            len = ntohl(len);
            if (len == 0 || len > 65536) {
                printf("[StreamServer] Invalid input length: %u\n", len);
                break;
            }

            std::vector<uint8_t> buf(len);
            nRecv = recv(m_clientSock, (char*)buf.data(), len, MSG_WAITALL);
            if (nRecv != (int)len) {
                printf("[StreamServer] Client disconnected or incomplete payload\n");
                break;
            }

            if (m_inputCallback) {
                // Deserialize byte stream to InputEvent safely.
                // For now, enforce fixed size struct.
                if (len >= sizeof(InputEvent)) {
                    InputEvent evt;
                    memcpy(&evt, buf.data(), sizeof(InputEvent));
                    m_inputCallback(evt);
                }
            }
        }

        printf("[StreamServer] Input receiver stopped\n");
    }

    void SetInputCallback(std::function<void(const InputEvent&)> cb) {
        m_inputCallback = cb;
    }

    bool IsConnected() const {
        return m_clientSock != INVALID_SOCKET;
    }

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
