// NvencEncoder.cpp — using NVIDIA Video Codec SDK
#include "nvEncodeAPI.h"
#include <d3d11.h>

extern ID3D11Device* g_d3dDevice; // Provided by frame capture module

class NvencEncoder {
    NV_ENCODE_API_FUNCTION_LIST m_nvenc = {};
    void*  m_encoder  = nullptr;
    void*  m_inputBuf = nullptr;
    void*  m_outputBuf = nullptr;

public:
    bool Init(int width, int height, int fps = 60) {
        NvEncodeAPICreateInstance(&m_nvenc);

        NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS sp = {NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS_VER};
        sp.deviceType = NV_ENC_DEVICE_TYPE_DIRECTX;
        sp.device     = g_d3dDevice; // your ID3D11Device*
        sp.apiVersion = NVENCAPI_VERSION;
        m_nvenc.nvEncOpenEncodeSessionEx(&sp, &m_encoder);

        // Configure H.264 low-latency preset
        NV_ENC_INITIALIZE_PARAMS initParams  = {NV_ENC_INITIALIZE_PARAMS_VER};
        NV_ENC_CONFIG             encConfig  = {NV_ENC_CONFIG_VER};
        initParams.encodeConfig              = &encConfig;
        initParams.encodeGUID                = NV_ENC_CODEC_H264_GUID;
        initParams.presetGUID                = NV_ENC_PRESET_P1_GUID; // lowest latency
        initParams.encodeWidth               = width;
        initParams.encodeHeight              = height;
        initParams.frameRateNum              = fps;
        initParams.frameRateDen              = 1;
        initParams.enablePTD                 = 1;

        encConfig.gopLength                              = NVENC_INFINITE_GOPLENGTH;
        encConfig.frameIntervalP                         = 1;
        encConfig.encodeCodecConfig.h264Config.idrPeriod = NVENC_INFINITE_GOPLENGTH;

        m_nvenc.nvEncInitializeEncoder(m_encoder, &initParams);
        // Allocate I/O buffers ...
        return true;
    }

    // Pass a D3D11 texture pointer — zero CPU copy
    std::vector<uint8_t> EncodeFrame(ID3D11Texture2D* tex) {
        NV_ENC_MAP_INPUT_RESOURCE mapParams = {NV_ENC_MAP_INPUT_RESOURCE_VER};
        // Register & map tex as NV_ENC input ...

        NV_ENC_PIC_PARAMS picParams = {NV_ENC_PIC_PARAMS_VER};
        picParams.inputBuffer       = m_inputBuf;
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