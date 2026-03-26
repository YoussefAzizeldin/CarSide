// InputInjector.cpp — Windows side
// Requires: Windows 10 1607+ (InjectSyntheticPointerInput API)
// Link:     user32.lib
#define _USE_MATH_DEFINES
#include <windows.h>
#include <cmath>
#include <cstdio>
#include "InputEvent.h"

// ─────────────────────────────────────────────────────────────────────────────
// Synthetic pointer device handles
// InitInjector() must be called once at startup before any inject call.
// ─────────────────────────────────────────────────────────────────────────────
static HSYNTHETICPOINTERDEVICE s_penDevice   = nullptr;
static HSYNTHETICPOINTERDEVICE s_touchDevice = nullptr;

// Contact ID slots (we only handle single-pointer for now)
static constexpr UINT32 kTouchId = 0;
static constexpr UINT32 kPenId   = 0;

// ─────────────────────────────────────────────────────────────────────────────
// Startup / shutdown
// ─────────────────────────────────────────────────────────────────────────────

bool InitInjector() {
    // Create a virtual pen digitizer (PT_PEN) — single contact
    s_penDevice = CreateSyntheticPointerDevice(PT_PEN, 1, POINTER_FEEDBACK_DEFAULT);
    if (!s_penDevice) {
        fprintf(stderr, "[Injector] CreateSyntheticPointerDevice(PT_PEN) failed: %lu\n",
                GetLastError());
        return false;
    }

    // Create a virtual touch digitizer (PT_TOUCH) — single contact
    s_touchDevice = CreateSyntheticPointerDevice(PT_TOUCH, 1, POINTER_FEEDBACK_DEFAULT);
    if (!s_touchDevice) {
        fprintf(stderr, "[Injector] CreateSyntheticPointerDevice(PT_TOUCH) failed: %lu\n",
                GetLastError());
        return false;
    }

    return true;
}

void ShutdownInjector() {
    if (s_penDevice)   { DestroySyntheticPointerDevice(s_penDevice);   s_penDevice   = nullptr; }
    if (s_touchDevice) { DestroySyntheticPointerDevice(s_touchDevice); s_touchDevice = nullptr; }
}

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

// Map UITouch phase → Windows POINTER_FLAG bitmask
static UINT32 PhaseToPointerFlags(int phase, bool isDown) {
    UINT32 flags = POINTER_FLAG_NONE;
    switch (phase) {
    case 0: // began
        flags = POINTER_FLAG_DOWN | POINTER_FLAG_INRANGE | POINTER_FLAG_INCONTACT;
        break;
    case 1: // moved
    case 2: // stationary
        flags = POINTER_FLAG_UPDATE | POINTER_FLAG_INRANGE | POINTER_FLAG_INCONTACT;
        break;
    case 3: // ended
        flags = POINTER_FLAG_UP;
        break;
    case 4: // cancelled
        flags = POINTER_FLAG_UP | POINTER_FLAG_CANCELLED;
        break;
    default:
        flags = POINTER_FLAG_UPDATE;
        break;
    }
    return flags;
}

// Convert Apple Pencil altitude (0=flat, π/2=perpendicular) + azimuth
// to Windows tiltX / tiltY (-90..+90 degrees each).
//
// Windows tilt axes:
//   tiltX: positive = pen tip tilted right  (+X screen direction)
//   tiltY: positive = pen tip tilted down   (+Y screen direction)
//
// Apple Pencil altitude = angle from the screen plane (0 = lying flat).
// Apple Pencil azimuth  = direction the tip points in the screen plane,
//                         measured clockwise from the +X axis.
static void AltAzToTilt(float altitude, float azimuth,
                         INT32& outTiltX, INT32& outTiltY) {
    // tilt magnitude from vertical: 90° when flat, 0° when perpendicular
    float tiltMag = (static_cast<float>(M_PI_2) - altitude)
                    * (180.0f / static_cast<float>(M_PI));

    // Decompose along screen axes using azimuth
    // azimuth=0 → pointing right (+X), azimuth=π/2 → pointing down (+Y)
    float tiltX =  tiltMag * cosf(azimuth);
    float tiltY =  tiltMag * sinf(azimuth);

    // Clamp to Windows valid range
    outTiltX = static_cast<INT32>(max(-90.0f, min(90.0f, tiltX)));
    outTiltY = static_cast<INT32>(max(-90.0f, min(90.0f, tiltY)));
}

// ─────────────────────────────────────────────────────────────────────────────
// Touch injection  (finger → mouse-compatible touch contact)
// ─────────────────────────────────────────────────────────────────────────────

void InjectTouch(const InputEvent& evt, int screenW, int screenH) {
    if (!s_touchDevice) return;

    POINTER_TOUCH_INFO contact = {};
    contact.pointerInfo.pointerType = PT_TOUCH;
    contact.pointerInfo.pointerId   = kTouchId;
    contact.pointerInfo.pointerFlags = PhaseToPointerFlags(evt.phase, true);
    contact.pointerInfo.ptPixelLocation = {
        static_cast<LONG>(evt.x * screenW),
        static_cast<LONG>(evt.y * screenH)
    };
    contact.pointerInfo.ptHimetricLocation = {
        static_cast<LONG>(evt.x * screenW  * 100),
        static_cast<LONG>(evt.y * screenH * 100)
    };

    // Contact area: a small 4×4 pixel ellipse
    contact.touchFlags = TOUCH_FLAG_NONE;
    contact.touchMask  = TOUCH_MASK_CONTACTAREA | TOUCH_MASK_PRESSURE;
    contact.rcContact  = {
        static_cast<LONG>(evt.x * screenW) - 2,
        static_cast<LONG>(evt.y * screenH) - 2,
        static_cast<LONG>(evt.x * screenW) + 2,
        static_cast<LONG>(evt.y * screenH) + 2
    };
    contact.pressure = static_cast<UINT32>(evt.pressure * 1024);

    if (!InjectSyntheticPointerInput(s_touchDevice, &contact, 1)) {
        fprintf(stderr, "[Injector] InjectSyntheticPointerInput(touch) failed: %lu\n",
                GetLastError());
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Pen injection  (Apple Pencil → WM_POINTER pen digitizer)
// Used for both EventType::Pencil and EventType::Drawing.
// ─────────────────────────────────────────────────────────────────────────────

void InjectPen(const InputEvent& evt, int screenW, int screenH) {
    if (!s_penDevice) return;

    INT32 tiltX = 0, tiltY = 0;
    AltAzToTilt(evt.altitude, evt.azimuth, tiltX, tiltY);

    POINTER_PEN_INFO pen = {};
    pen.pointerInfo.pointerType  = PT_PEN;
    pen.pointerInfo.pointerId    = kPenId;
    pen.pointerInfo.pointerFlags = PhaseToPointerFlags(evt.phase, true);
    pen.pointerInfo.ptPixelLocation = {
        static_cast<LONG>(evt.x * screenW),
        static_cast<LONG>(evt.y * screenH)
    };
    pen.pointerInfo.ptHimetricLocation = {
        static_cast<LONG>(evt.x * screenW  * 100),
        static_cast<LONG>(evt.y * screenH * 100)
    };

    pen.penFlags = PEN_FLAG_NONE;
    pen.penMask  = PEN_MASK_PRESSURE | PEN_MASK_TILT_X | PEN_MASK_TILT_Y;
    pen.pressure = static_cast<UINT32>(evt.pressure * 1024); // 0–1024
    pen.tiltX    = tiltX;   // degrees, –90 to +90
    pen.tiltY    = tiltY;

    // In DrawingPad mode the iPad sends .drawing events; barrel button is implicit
    if (evt.isDrawingPad)
        pen.penFlags |= PEN_FLAG_BARREL;   // signals "drawing intent" to apps like OneNote

    if (!InjectSyntheticPointerInput(s_penDevice, &pen, 1)) {
        fprintf(stderr, "[Injector] InjectSyntheticPointerInput(pen) failed: %lu\n",
                GetLastError());
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Scroll injection  (two-finger DrawingPad pan → WM_MOUSEWHEEL / mouse move)
// The Swift side sends phase+normalised delta in x/y when isDrawingPad=true
// and type=touch (two-finger pan gesture).
// ─────────────────────────────────────────────────────────────────────────────

void InjectScroll(const InputEvent& evt, int screenW, int screenH) {
    // Horizontal scroll
    if (fabsf(evt.x) > 0.0001f) {
        INPUT hScroll = {};
        hScroll.type = INPUT_MOUSE;
        hScroll.mi.dwFlags = MOUSEEVENTF_HWHEEL;
        hScroll.mi.mouseData = static_cast<DWORD>(evt.x * -3 * WHEEL_DELTA);
        SendInput(1, &hScroll, sizeof(INPUT));
    }
    // Vertical scroll
    if (fabsf(evt.y) > 0.0001f) {
        INPUT vScroll = {};
        vScroll.type = INPUT_MOUSE;
        vScroll.mi.dwFlags = MOUSEEVENTF_WHEEL;
        vScroll.mi.mouseData = static_cast<DWORD>(evt.y * -3 * WHEEL_DELTA);
        SendInput(1, &vScroll, sizeof(INPUT));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Main dispatch — call this for every InputEvent received from the iPad
// ─────────────────────────────────────────────────────────────────────────────

void DispatchInputEvent(const InputEvent& evt, int screenW, int screenH) {
    switch (evt.type) {

    case EventType::Touch:
        if (evt.isDrawingPad) {
            // Two-finger pan from DrawingPad mode → scroll
            InjectScroll(evt, screenW, screenH);
        } else {
            InjectTouch(evt, screenW, screenH);
        }
        break;

    case EventType::Pencil:
    case EventType::Drawing:
        // Both pencil types map to a WM_POINTER pen contact.
        // DrawingPad mode sets PEN_FLAG_BARREL to hint "inking" intent.
        InjectPen(evt, screenW, screenH);
        break;
    }
}