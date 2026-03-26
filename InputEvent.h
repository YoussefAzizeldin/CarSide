#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <nlohmann/json.hpp>

// Matches the Swift InputEvent struct sent over the wire as JSON.
// EventType mirrors the Swift enum string raw values.

enum class EventType : uint8_t {
    Touch      = 0,   // "touch"   — finger, forwarded as mouse
    Pencil     = 1,   // "pencil"  — Apple Pencil in standard mode
    Drawing    = 2,   // "drawing" — Apple Pencil in DrawingPad mode
};

struct InputEvent {
    EventType type;
    float     x;           // Normalised 0–1 (horizontal)
    float     y;           // Normalised 0–1 (vertical)
    int       phase;       // 0=began 1=moved 2=stationary 3=ended 4=cancelled
    float     pressure;    // 0–1
    float     altitude;    // Pencil altitude angle in radians (0=flat, π/2=vertical)
    float     azimuth;     // Pencil azimuth angle in radians
    bool      isDrawingPad;

    static InputEvent Deserialize(const std::vector<uint8_t>& data) {
        std::string jsonStr(data.begin(), data.end());
        auto j = nlohmann::json::parse(jsonStr);

        InputEvent evt;
        std::string typeStr = j["type"];
        if (typeStr == "touch") evt.type = EventType::Touch;
        else if (typeStr == "pencil") evt.type = EventType::Pencil;
        else if (typeStr == "drawing") evt.type = EventType::Drawing;

        evt.x = j["x"];
        evt.y = j["y"];
        evt.phase = j["phase"];
        evt.pressure = j["pressure"];
        evt.altitude = j["altitude"];
        evt.azimuth = j["azimuth"];
        evt.isDrawingPad = j["isDrawingPad"];

        return evt;
    }
};