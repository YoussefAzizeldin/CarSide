// AMDEncoder.cpp — cross-platform video encoding for NVIDIA (NVENC), AMD (VCN), and CPU
// Uses Windows Media Foundation (IMFTransform) with DXVA2 hardware acceleration
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

class AMDEncoder {
    ComPtr<IMFTransform> m_encoder;
    ComPtr<IMFSample> m_inputSample;
    ComPtr<IMFMediaBuffer> m_outputBuffer;
    DWORD m_inputStreamId = 0;
    DWORD m_outputStreamId = 0;
    int m_width, m_height, m_fps;

public:
    bool Init(int width, int height, int fps = 60) {
        m_width = width;
        m_height = height;
        m_fps = fps;

        // Initialize Media Foundation
        MFStartup(MF_VERSION);

        // Create H264 encoder (MediaFoundation auto-selects hardware if available)
        CLSID encoderId = CLSID_CMSH264EncoderMFT;  // Hardware H.264 encoder
        if (FAILED(CoCreateInstance(encoderId, nullptr, CLSCTX_INPROC_SERVER, 
                                    IID_PPV_ARGS(&m_encoder)))) {
            // Fallback to software encoder if hardware unavailable
            encoderId = CLSID_CMSH264EncoderMFT;
            if (FAILED(CoCreateInstance(encoderId, nullptr, CLSCTX_INPROC_SERVER,
                                        IID_PPV_ARGS(&m_encoder)))) {
                return false;
            }
        }

        // Set input type (NV12 format for GPU acceleration)
        ComPtr<IMFMediaType> inputType;
        MFCreateMediaType(&inputType);
        inputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        inputType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
        inputType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
        inputType->SetUINT32(MF_MT_MPEG2_FLAGS, 0);
        
        MFSetAttributeSize(inputType.Get(), MF_MT_FRAME_SIZE, width, height);
        MFSetAttributeRatio(inputType.Get(), MF_MT_FRAME_RATE, fps, 1);
        MFSetAttributeRatio(inputType.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);

        m_encoder->SetInputType(0, inputType.Get(), 0);

        // Set output type (H.264 NAL format)
        ComPtr<IMFMediaType> outputType;
        MFCreateMediaType(&outputType);
        outputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        outputType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
        outputType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
        
        // Bitrate & quality settings
        outputType->SetUINT32(MF_MT_AVG_BITRATE, 8000000); // 8 Mbps
        MFSetAttributeSize(outputType.Get(), MF_MT_FRAME_SIZE, width, height);
        MFSetAttributeRatio(outputType.Get(), MF_MT_FRAME_RATE, fps, 1);

        m_encoder->SetOutputType(0, outputType.Get(), 0);

        // Configure for low-latency streaming
        ComPtr<ICodecAPI> codecApi;
        m_encoder->QueryInterface(&codecApi);
        if (codecApi) {
            VARIANT var;
            var.vt = VT_BOOL;
            var.boolVal = VARIANT_TRUE;
            codecApi->SetValue(&CODECAPI_AVLowLatencyMode, &var);
            VariantClear(&var);
        }

        m_encoder->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);
        m_encoder->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, 0);
        return true;
    }

    // Pass a D3D11 texture pointer
    std::vector<uint8_t> EncodeFrame(ID3D11Texture2D* tex) {
        std::vector<uint8_t> nalData;

        // Create input sample from D3D11 texture
        ComPtr<IMFSample> inputSample;
        ComPtr<IMFMediaBuffer> inputBuffer;
        ComPtr<IMFDXGIBuffer> dxgiBuffer;

        MFCreateSample(&inputSample);
        MFCreateDXGIBuffer(tex, TRUE, &dxgiBuffer);  // TRUE = read-only, GPU texture
        inputSample->AddBuffer(dxgiBuffer.Get());

        LONGLONG currentTime = 0;
        inputSample->SetSampleTime(currentTime);
        inputSample->SetSampleDuration(1000000 / m_fps);  // microseconds

        // Process frame
        m_encoder->ProcessInput(0, inputSample.Get(), 0);

        // Get output
        MFT_OUTPUT_DATA_BUFFER outputDataBuffer = {};
        DWORD status = 0;

        while (m_encoder->ProcessOutput(0, 1, &outputDataBuffer, &status) == S_OK) {
            if (outputDataBuffer.pSample) {
                DWORD bufferCount;
                outputDataBuffer.pSample->GetBufferCount(&bufferCount);

                for (DWORD i = 0; i < bufferCount; ++i) {
                    ComPtr<IMFMediaBuffer> buffer;
                    outputDataBuffer.pSample->GetBufferByIndex(i, &buffer);

                    BYTE* data = nullptr;
                    DWORD length = 0;
                    buffer->Lock(&data, nullptr, &length);

                    nalData.insert(nalData.end(), data, data + length);

                    buffer->Unlock();
                }
                outputDataBuffer.pSample->Release();
            }
        }

        return nalData;
    }
        picParams.outputBitstream   = m_outputBuf;
        picParams.inputWidth        = /* width */;
        picParams.inputHeight       = /* height */;
        picParams.pictureStruct     = NV_ENC_PIC_STRUCT_FRAME;
        m_nvenc.nvEncEncodePicture(m_encoder, &picParams);

        // Lock bitstream output and return NAL bytes
        NV_ENC_LOCK_BITSTREAM lockParams = {NV_ENC_LOCK_BITSTREAM_VER};
        lockParams.outputBitstream = m_outputBuf;
        m_nvenc.nvEncLockBitstream(m_encoder, &lockParams);
        std::vector<uint8_t> nal(
            (uint8_t*)lockParams.bitstreamBufferPtr,
            (uint8_t*)lockParams.bitstreamBufferPtr + lockParams.bitstreamSizeInBytes);
        m_nvenc.nvEncUnlockBitstream(m_encoder, m_outputBuf);
        return nal;
    }
};