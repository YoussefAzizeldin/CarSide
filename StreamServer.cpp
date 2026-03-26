// StreamServer.cpp — async Winsock TCP server
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstdint>
#include <vector>
#include <functional>

#include "InputEvent.h"

class StreamServer {
    SOCKET m_listenSock = INVALID_SOCKET;
    SOCKET m_clientSock = INVALID_SOCKET;

public:
    bool Start(uint16_t port = 7878) {
        WSADATA wsa; WSAStartup(MAKEWORD(2,2), &wsa);
        m_listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

        sockaddr_in addr = {};
        addr.sin_family      = AF_INET;
        addr.sin_port        = htons(port);
        addr.sin_addr.s_addr = INADDR_ANY;

        bind(m_listenSock, (sockaddr*)&addr, sizeof(addr));
        listen(m_listenSock, 1);

        // Set TCP_NODELAY for minimum latency
        int flag = 1;
        setsockopt(m_listenSock, IPPROTO_TCP, TCP_NODELAY,
                   (char*)&flag, sizeof(flag));

        m_clientSock = accept(m_listenSock, nullptr, nullptr);
        return m_clientSock != INVALID_SOCKET;
    }

    // Frame packet: [4-byte length][NAL data]
    void SendFrame(const std::vector<uint8_t>& nal) {
        uint32_t len = htonl((uint32_t)nal.size());
        send(m_clientSock, (char*)&len, 4, 0);
        send(m_clientSock, (char*)nal.data(), (int)nal.size(), 0);
    }

    // Receive input events from iPad
    void RecvInputLoop(std::function<void(const InputEvent&)> cb) {
        while (true) {
            uint32_t len = 0;
            if (recv(m_clientSock, (char*)&len, 4, MSG_WAITALL) != 4) break;
            len = ntohl(len);
            std::vector<uint8_t> buf(len);
            recv(m_clientSock, (char*)buf.data(), len, MSG_WAITALL);
            InputEvent evt = InputEvent::Deserialize(buf);
            cb(evt);
        }
    }
};