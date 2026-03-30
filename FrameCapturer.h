#pragma once
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <vector>

using Microsoft::WRL::ComPtr;

class FrameCapturer {
    ComPtr<IDXGIOutputDuplication> m_duplication;
    ComPtr<ID3D11Device>           m_device;
    ComPtr<ID3D11DeviceContext>    m_context;

public:
    HRESULT Init(UINT adapterIndex = 0, UINT outputIndex = 0);
    ID3D11Texture2D* AcquireFrameGPU(int& width, int& height);
    bool AcquireFrameCPU(std::vector<uint8_t>& outRGBA, int& width, int& height);
    ID3D11Texture2D* CaptureFrame();  // Simple wrapper
    HRESULT SetSwapChain(HANDLE hSwapChain, LUID adapterLuid);
    ~FrameCapturer();

    // Getter for device (used by encoder)
    ID3D11Device* GetDevice();
    ID3D11DeviceContext* GetContext();
};