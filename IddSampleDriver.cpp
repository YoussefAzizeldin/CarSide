// IddSampleDriver.cpp — core IDD setup (UMDF 2.x)
#include <IddCx.h>
#include <cstdio>

extern class FrameCapturer* g_captureModule;

NTSTATUS IddSampleAdapterInitFinished(IDDCX_ADAPTER adapter, const IDARG_IN_ADAPTER_INIT_FINISHED* pArgs) {
    IDDCX_MONITOR_INFO monInfo = {};
    monInfo.Size = sizeof(monInfo);
    monInfo.MonitorType = DisplayConfigVideoOutputTechnology::DisplayConfigVideoOutputTechnologyHdmi;

    DISPLAYID_DETAILED_TIMING_TYPE_I timing = {};
    timing.PixelClock       = 164000;
    timing.HorizontalActive = 1920;
    timing.VerticalActive   = 1200;
    timing.HorizontalBlanking = 160;
    timing.VerticalBlanking = 60;
    timing.HorizontalSyncOffset = 40;
    timing.HorizontalSyncPulseWidth = 30;
    timing.VerticalSyncOffset = 5;
    timing.VerticalSyncPulseWidth = 5;
    timing.Flags.HorizontalSyncPolarity = 1;
    timing.Flags.VerticalSyncPolarity = 1;

    monInfo.pMonitorDescription = &timing;
    monInfo.MonitorDescription = timing;

    IDDCX_MONITOR monitor = nullptr;
    NTSTATUS status = IddCxMonitorCreate(adapter, &monInfo, &monitor);
    if (!NT_SUCCESS(status)) {
        fprintf(stderr, "[IddSampleDriver] IddCxMonitorCreate failed: 0x%08lx\n", status);
        return status;
    }
    status = IddCxMonitorArrival(monitor);
    if (!NT_SUCCESS(status)) {
        fprintf(stderr, "[IddSampleDriver] IddCxMonitorArrival failed: 0x%08lx\n", status);
    }
    return status;
}

// BUG FIX #27: Correct callback signature must return NTSTATUS
NTSTATUS IddSampleMonitorAssignSwapChain(IDDCX_MONITOR monitor,
                                         const IDARG_IN_SETSWAPCHAIN* pArgs) {
    if (!pArgs || !g_captureModule) {
        return STATUS_INVALID_PARAMETER;
    }
    // BUG FIX #24: Use swap chain directly instead of Desktop Duplication
    // This avoids session 0 limitations when running as UMDF driver
    g_captureModule->SetSwapChain(pArgs->hSwapChain, pArgs->RenderAdapterLuid);
    return STATUS_SUCCESS;
}