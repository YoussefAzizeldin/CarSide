#pragma once
#include <vector>
#include <functional>
#include <cstdint>
#include "InputEvent.h"

class StreamServer {
public:
    bool Start(uint16_t port = 7878);
    // bool RegisterBonjourService(uint16_t port);  // Bonjour - optional
    bool SendFrame(const std::vector<uint8_t>& nalData);
    void SetInputCallback(std::function<void(const InputEvent&)> cb);
    bool IsConnected() const;
    void Stop();
    ~StreamServer();
};