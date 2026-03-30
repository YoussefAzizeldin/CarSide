// ServerInitializer.cpp — Main entry point for CarSide Windows Server
// Coordinates frame capture, video encoding, and network streaming

#include <windows.h>
#include <stdio.h>
#include <thread>
#include <chrono>
#include <iostream>
#include <vector>
#include <objbase.h>

// Class headers
#include "FrameCapturer.h"
#include "NvencEncoder.h"
#include "StreamServer.h"

// Global components
FrameCapturer*  g_capturer  = nullptr;
NvencEncoder*     g_encoder   = nullptr;
StreamServer*   g_server    = nullptr;

// Global D3D11 device (used by encoder and capturer)
ID3D11Device*       g_d3dDevice = nullptr;
ID3D11DeviceContext* g_d3dContext = nullptr;

// Signal for graceful shutdown
HANDLE g_shutdownEvent = nullptr;

BOOL WINAPI ConsoleCtrlHandler(DWORD dwCtrlType) {
    if (dwCtrlType == CTRL_C_EVENT || dwCtrlType == CTRL_BREAK_EVENT) {
        printf("[Main] Shutdown signal received (Ctrl+C)\n");
        SetEvent(g_shutdownEvent);
        Sleep(1000);  // Give threads time to clean up
        return TRUE;
    }
    return FALSE;
}

void StreamingLoop() {
    printf("[StreamingLoop] Started (60 FPS target)\n");

    DWORD lastFrameTime = GetTickCount();
    const DWORD FRAME_TIME_MS = 1000 / 60;  // ~16.67 ms for 60 FPS

    while (WaitForSingleObject(g_shutdownEvent, 0) == WAIT_TIMEOUT) {
        DWORD currentTime = GetTickCount();
        DWORD elapsedMs = currentTime - lastFrameTime;

        if (elapsedMs < FRAME_TIME_MS) {
            Sleep(FRAME_TIME_MS - elapsedMs);
            continue;
        }

        // Capture frame
        if (!g_capturer) {
            Sleep(10);
            continue;
        }

        ID3D11Texture2D* frame = g_capturer->CaptureFrame();
        if (!frame) {
            printf("[StreamingLoop] Frame capture failed\n");
            Sleep(10);
            continue;
        }

        // Encode frame to H.264 NALs
        if (!g_encoder) {
            frame->Release();
            Sleep(10);
            continue;
        }

        std::vector<uint8_t> nalData = g_encoder->EncodeFrame(frame);
        frame->Release();

        // Send over network
        if (!g_server || !g_server->IsConnected()) {
            // No client connected yet, continue encoding but don't send
            continue;
        }

        if (!g_server->SendFrame(nalData)) {
            printf("[StreamingLoop] Frame send failed, client disconnected\n");
            continue;
        }

        lastFrameTime = GetTickCount();
    }

    printf("[StreamingLoop] Stopped\n");
}

int main(int argc, char* argv[]) {
    printf(
        "\n"
        "╔════════════════════════════════════════╗\n"
        "║  CarSide Server v1.0                   ║\n"
        "║  Wireless Display Extension for iPad   ║\n"
        "╚════════════════════════════════════════╝\n"
        "\n"
    );

    // Parse command line
    uint16_t port = 7878;
    bool debugMode = false;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            port = (uint16_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--debug") == 0) {
            debugMode = true;
        } else if (strcmp(argv[i], "--help") == 0) {
            printf(
                "Usage: CarSideServer [options]\n"
                "\n"
                "Options:\n"
                "  --port <number>   Listen port (default: 7878)\n"
                "  --debug            Enable debug output\n"
                "  --help             Show this message\n"
            );
            return 0;
        }
    }

    if (debugMode) {
        printf("[Config] Debug mode: ON\n");
    }

    printf("[Config] Port: %d\n\n", port);

    // Create shutdown event
    g_shutdownEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);

    // Initialize COM for Windows
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    printf("[Init] Initializing components...\n");

    // TODO: Initialize Frame Capturer
    g_capturer = new FrameCapturer();
    if (FAILED(g_capturer->Init())) {
        printf("[Error] Frame capturer initialization failed\n");
        return 1;
    }
    // Set global D3D11 device for encoder
    g_d3dDevice = g_capturer->GetDevice();
    g_d3dContext = g_capturer->GetContext();
    printf("[Init] Frame capturer initialized\n");

    // Initialize Video Encoder (AMD/NVIDIA/Intel/CPU)
    g_encoder = new NvencEncoder();
    if (!g_encoder->Init(1920, 1080, 60)) {
        printf("[Error] Video encoder initialization failed\n");
        return 1;
    }

    printf("[Init] Video encoder initialized\n");

    // Initialize Network Server
    g_server = new StreamServer();
    if (!g_server->Start(port)) {
        printf("[Error] Server initialization failed\n");
        return 1;
    }

    printf("[Init] All components initialized successfully\n\n");

    // Start streaming thread
    std::thread streamThread(StreamingLoop);

    printf("[Main] Server ready - waiting for iPad client...\n");
    printf("[Main] Advertised as: CarSide._winextend._tcp.local.:%d\n", port);
    printf("[Main] Press Ctrl+C to stop\n\n");

    // Wait for shutdown signal
    WaitForSingleObject(g_shutdownEvent, INFINITE);

    printf("[Main] Shutting down...\n");

    // Stop streaming
    streamThread.join();

    // Cleanup
    if (g_server) {
        g_server->Stop();
        delete g_server;
    }

    if (g_encoder) {
        delete g_encoder;
    }

    if (g_capturer) {
        delete g_capturer;
    }

    CloseHandle(g_shutdownEvent);
    CoUninitialize();

    printf("[Main] Shutdown complete\n");
    return 0;
}
