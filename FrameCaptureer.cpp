// FrameCapturer.cpp
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
    HRESULT Init(UINT adapterIndex = 0, UINT outputIndex = 0) {
        ComPtr<IDXGIFactory1> factory;
        HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
        if (FAILED(hr)) {
            fprintf(stderr, "[FrameCapturer] CreateDXGIFactory1 failed: 0x%08lx\n", hr);
            return hr;
        }

        ComPtr<IDXGIAdapter> adapter;
        hr = factory->EnumAdapters(adapterIndex, &adapter);
        if (FAILED(hr)) {
            fprintf(stderr, "[FrameCapturer] EnumAdapters failed: 0x%08lx\n", hr);
            return hr;
        }

        hr = D3D11CreateDevice(adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN,
            nullptr, 0, nullptr, 0, D3D11_SDK_VERSION,
            &m_device, nullptr, &m_context);
        if (FAILED(hr)) {
            fprintf(stderr, "[FrameCapturer] D3D11CreateDevice failed: 0x%08lx\n", hr);
            return hr;
        }

        ComPtr<IDXGIOutput> output;
        hr = adapter->EnumOutputs(outputIndex, &output);
        if (FAILED(hr)) {
            fprintf(stderr, "[FrameCapturer] EnumOutputs failed: 0x%08lx\n", hr);
            return hr;
        }
        ComPtr<IDXGIOutput1> output1;
        hr = output.As(&output1);
        if (FAILED(hr)) {
            fprintf(stderr, "[FrameCapturer] QueryInterface(IDXGIOutput1) failed: 0x%08lx\n", hr);
            return hr;
        }

        return output1->DuplicateOutput(m_device.Get(), &m_duplication);
    }

    ID3D11Texture2D* AcquireFrameGPU(int& width, int& height) {
        DXGI_OUTDUPL_FRAME_INFO frameInfo = {};
        ComPtr<IDXGIResource> resource;

        HRESULT hr = m_duplication->AcquireNextFrame(16 /*ms*/, &frameInfo, &resource);
        if (FAILED(hr)) return nullptr;

        ComPtr<ID3D11Texture2D> tex;
        hr = resource.As(&tex);
        if (FAILED(hr)) {
            fprintf(stderr, "[FrameCapturer] QueryInterface(ID3D11Texture2D) failed: 0x%08lx\n", hr);
            m_duplication->ReleaseFrame();
            return nullptr;
        }

        D3D11_TEXTURE2D_DESC desc = {};
        tex->GetDesc(&desc);
        width = desc.Width;
        height = desc.Height;

        return tex.Detach();
    }

    bool AcquireFrameCPU(std::vector<uint8_t>& outRGBA, int& width, int& height) {
        DXGI_OUTDUPL_FRAME_INFO frameInfo = {};
        ComPtr<IDXGIResource> resource;

        HRESULT hr = m_duplication->AcquireNextFrame(16 /*ms*/, &frameInfo, &resource);
        if (FAILED(hr)) return false;

        ComPtr<ID3D11Texture2D> tex;
        hr = resource.As(&tex);
        if (FAILED(hr)) {
            fprintf(stderr, "[FrameCapturer] QueryInterface failed: 0x%08lx\n", hr);
            m_duplication->ReleaseFrame();
            return false;
        }

        D3D11_TEXTURE2D_DESC desc = {};
        tex->GetDesc(&desc);
        desc.Usage          = D3D11_USAGE_STAGING;
        desc.BindFlags      = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        desc.MiscFlags      = 0;

        ComPtr<ID3D11Texture2D> stagingTex;
        hr = m_device->CreateTexture2D(&desc, nullptr, &stagingTex);
        if (FAILED(hr)) {
            fprintf(stderr, "[FrameCapturer] CreateTexture2D(staging) failed: 0x%08lx\n", hr);
            m_duplication->ReleaseFrame();
            return false;
        }
        m_context->CopyResource(stagingTex.Get(), tex.Get());

        D3D11_MAPPED_SUBRESOURCE mapped = {};
        hr = m_context->Map(stagingTex.Get(), 0, D3D11_MAP_READ, 0, &mapped);
        if (FAILED(hr)) {
            fprintf(stderr, "[FrameCapturer] Map failed: 0x%08lx\n", hr);
            m_duplication->ReleaseFrame();
            return false;
        }

        width  = desc.Width;
        height = desc.Height;
        // BUG FIX #23: Copy row-by-row to handle RowPitch correctly (includes GPU padding)
        outRGBA.resize(width * 4 * height);
        uint8_t* srcData = reinterpret_cast<uint8_t*>(mapped.pData);
        uint8_t* dstData = outRGBA.data();
        for (int y = 0; y < height; ++y) {
            memcpy(dstData + y * width * 4, srcData + y * mapped.RowPitch, width * 4);
        }

        m_context->Unmap(stagingTex.Get(), 0);
        m_duplication->ReleaseFrame();
        return true;
    }

    // BUG FIX #25: Add destructor for proper cleanup
    ~FrameCapturer() {
        if (m_duplication) {
            m_duplication = nullptr;  // ComPtr will release
        }
    }

private:
    // BUG FIX #24: Note about session 0 limitation
    // NOTE: DuplicateOutput requires session 1 (interactive desktop).
    // In UMDF driver context (session 0), use the swap chain from IddSampleMonitorAssignSwapChain instead.
};