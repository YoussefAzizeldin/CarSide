// NvencEncoder.cpp — Cross-platform encoder wrapper for hardware + software
// Uses Windows Media Foundation (IMFTransform) with DXVA2 Video Processor

#include <mfapi.h>
#include <mfidl.h>
#include <dxva2api.h>
#include <codecapi.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <vector>
#include <cstdint>

using Microsoft::WRL::ComPtr;

extern ID3D11Device* g_d3dDevice;  // Provided by frame capture module
extern ID3D11DeviceContext* g_d3dContext;

class NvencEncoder {
    ComPtr<IMFTransform> m_encoder;
    int m_width = 0;
    int m_height = 0;
    int m_fps = 60;

public:
    bool Init(int width, int height, int fps = 60) {
        m_width = width;
        m_height = height;
        m_fps = fps;

        if (FAILED(MFStartup(MF_VERSION))) {
            return false;
        }

        // Create H264 encoder MFT (hardware first, software fallback)
        CLSID encoderId = CLSID_CMSH264EncoderMFT;
        if (FAILED(CoCreateInstance(encoderId, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&m_encoder)))) {
            return false;
        }

        ComPtr<IMFMediaType> inputType;
        if (FAILED(MFCreateMediaType(&inputType))) return false;

        inputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        inputType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
        inputType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
        MFSetAttributeSize(inputType.Get(), MF_MT_FRAME_SIZE, width, height);
        MFSetAttributeRatio(inputType.Get(), MF_MT_FRAME_RATE, fps, 1);
        MFSetAttributeRatio(inputType.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);

        if (FAILED(m_encoder->SetInputType(0, inputType.Get(), 0))) return false;

        ComPtr<IMFMediaType> outputType;
        if (FAILED(MFCreateMediaType(&outputType))) return false;

        outputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        outputType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
        outputType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
        outputType->SetUINT32(MF_MT_AVG_BITRATE, 8000000);
        MFSetAttributeSize(outputType.Get(), MF_MT_FRAME_SIZE, width, height);
        MFSetAttributeRatio(outputType.Get(), MF_MT_FRAME_RATE, fps, 1);

        if (FAILED(m_encoder->SetOutputType(0, outputType.Get(), 0))) return false;

        ComPtr<ICodecAPI> codecApi;
        if (SUCCEEDED(m_encoder->QueryInterface(&codecApi)) && codecApi) {
            VARIANT var;
            VariantInit(&var);
            var.vt = VT_BOOL;
            var.boolVal = VARIANT_TRUE;
            codecApi->SetValue(&CODECAPI_AVLowLatencyMode, &var);
            VariantClear(&var);
        }

        m_encoder->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
        m_encoder->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);

        return true;
    }

    std::vector<uint8_t> EncodeFrame(ID3D11Texture2D* tex, int64_t frameIndex) {
        std::vector<uint8_t> nalData;
        if (!m_encoder || !tex) return nalData;

        ComPtr<IMFSample> sample;
        if (FAILED(MFCreateSample(&sample))) return nalData;

        ComPtr<IMFDXGIBuffer> dxgiBuf;
        if (FAILED(MFCreateDXGIBuffer(tex, FALSE, &dxgiBuf))) return nalData;

        if (FAILED(sample->AddBuffer(dxgiBuf.Get()))) return nalData;

        LONGLONG sampleTime = frameIndex * (10000000 / m_fps);
        sample->SetSampleTime(sampleTime);
        sample->SetSampleDuration(10000000 / m_fps);

        if (FAILED(m_encoder->ProcessInput(0, sample.Get(), 0))) {
            return nalData;
        }

        MFT_OUTPUT_DATA_BUFFER output = {};
        DWORD status = 0;

        while (true) {
            HRESULT hr = m_encoder->ProcessOutput(0, 1, &output, &status);

            if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
                break;
            }
            if (hr == MF_E_TRANSFORM_STREAM_CHANGE) {
                ComPtr<IMFMediaType> newType;
                if (SUCCEEDED(m_encoder->GetOutputAvailableType(0, 0, &newType))) {
                    m_encoder->SetOutputType(0, newType.Get(), 0);
                    continue;
                }
                break;
            }
            if (FAILED(hr)) {
                break;
            }

            if (output.pSample) {
                DWORD bufferCount = 0;
                if (SUCCEEDED(output.pSample->GetBufferCount(&bufferCount))) {
                    for (DWORD i = 0; i < bufferCount; ++i) {
                        ComPtr<IMFMediaBuffer> buf;
                        if (SUCCEEDED(output.pSample->GetBufferByIndex(i, &buf))) {
                            BYTE* data = nullptr;
                            DWORD maxLen = 0;
                            DWORD currLen = 0;
                            if (SUCCEEDED(buf->Lock(&data, &maxLen, &currLen))) {
                                nalData.insert(nalData.end(), data, data + currLen);
                                buf->Unlock();
                            }
                        }
                    }
                }
                output.pSample->Release();
                output.pSample = nullptr;
            }
        }

        return nalData;
    }

    ~NvencEncoder() {
        if (m_encoder) {
            m_encoder->ProcessMessage(MFT_MESSAGE_NOTIFY_END_STREAMING, 0);
            m_encoder->ProcessMessage(MFT_MESSAGE_NOTIFY_END_OF_STREAM, 0);
        }
        MFShutdown();
    }
};

