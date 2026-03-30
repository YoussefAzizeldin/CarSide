#pragma once
#include <vector>
#include <cstdint>

class NvencEncoder {
public:
    bool Init(int width, int height, int fps = 60);
    std::vector<uint8_t> EncodeFrame(ID3D11Texture2D* frame);
    ~NvencEncoder();
};