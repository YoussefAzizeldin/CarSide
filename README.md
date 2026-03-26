# CarSide: Wireless Display Extension for iPad

> **Extend your Windows desktop to your iPad wirelessly** — a native iOS app that mirrors your Windows display and captures input in real-time, similar to Apple's Sidecar but for Windows.

[![iOS](https://img.shields.io/badge/iOS-13%2B-blue.svg?style=flat-square)](https://www.apple.com/ios/)
[![Swift](https://img.shields.io/badge/Swift-5.5%2B-orange.svg?style=flat-square)](https://swift.org/)
[![License](https://img.shields.io/badge/License-MIT-green.svg?style=flat-square)](LICENSE)

## 📋 Table of Contents

- [Features](#features)
- [System Requirements](#system-requirements)
- [Architecture & Workflow](#architecture--workflow)
- [Quick Start](#quick-start)
- [Installation Guide](#installation-guide)
- [Building on macOS](#building-on-macos)
- [iPad Setup & Launch](#ipad-setup--launch)
- [Usage](#usage)
- [Project Structure](#project-structure)
- [Troubleshooting](#troubleshooting)
- [Advanced](#advanced)

---

## ✨ Features

- 🎬 **H.264 Video Streaming** — Low-latency GPU-accelerated video decoding with Metal rendering
- 🖱️ **Multi-Input Support** — Touch, Apple Pencil, and mouse input pass-through
- 🎨 **Drawing Mode** — Dedicated stylus mode for precision input with local zero-latency ink feedback
- 🔍 **Auto-Discovery** — Bonjour/mDNS automatically finds your Windows host on the network
- 🚀 **High Performance** — Exponential backoff reconnection, frame buffering, thread-safe operations
- 🔒 **Production-Ready** — Comprehensive error handling, memory management, and data race prevention

---

## 📱 System Requirements

### iPad Side
- **iOS**: 13.0 or later
- **Hardware**: iPad with A9 chip or later (supports Metal GPU acceleration)
- **Network**: Wi-Fi connection (same network as Windows host)
- **Storage**: ~50 MB available space

### Windows Host Side
- **OS**: Windows 10 or Windows 11
- **GPU**: NVIDIA GPU (for NVENC encoding) or CPU fallback
- **Network**: Bonjour service advertised on the network
- **C++ Runtime**: Visual Studio redistributable libraries

### macOS Development Machine (Build Only)
- **macOS**: 11.0 or later
- **Xcode**: 12.0 or later with iOS SDK 13.0+
- **Disk Space**: ~3–5 GB for Xcode and dependencies

---

## 🏗️ Architecture & Workflow

### System Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                    Windows Host (Server)                        │
├─────────────────────────────────────────────────────────────────┤
│  ┌─────────────────┐  ┌──────────────┐  ┌─────────────────┐   │
│  │ Frame Capturer  │→ │ NVENC        │→ │ Stream Server   │   │
│  │ (DXGI)          │  │ (H.264 NAL)  │  │ (TCP 7878)      │   │
│  └─────────────────┘  └──────────────┘  └─────────────────┘   │
│                                               ↑                 │
│                                          TCP Stream             │
│                                         + Input Events          │
│                                               ↓                 │
└─────────────────────────────────────────────────────────────────┘
                           Wi-Fi Network (Bonjour)
┌─────────────────────────────────────────────────────────────────┐
│                   iPad (Client - This App)                      │
├─────────────────────────────────────────────────────────────────┤
│  ┌──────────────────┐  ┌──────────────┐  ┌────────────────┐   │
│  │ Bonjour         │→ │ Stream       │→ │ Metal          │   │
│  │ Discovery       │  │ Client       │  │ Renderer       │   │
│  └──────────────────┘  └──────────────┘  └────────────────┘   │
│                           ↓                      ↑              │
│                      ┌──────────────┐           │              │
│                      │ VideoToolbox │           │              │
│                      │ H.264 Decode │           │              │
│                      └──────────────┘           │              │
│                                                 ↓              │
│                                      ┌──────────────────┐     │
│                                      │ Input Capture    │     │
│                                      │ (Touch/Pencil)   │     │
│                                      └──────────────────┘     │
└─────────────────────────────────────────────────────────────────┘
```

### Data Flow

1. **Video Stream**: Windows captures desktop → NVENC encodes H.264 NALs → TCP sends to iPad
2. **Decode**: iPad receives NALs → VideoToolbox decodes → Metal renders to screen
3. **Input**: iPad captures touch/pencil → serializes to JSON → TCP sends back to Windows
4. **Discovery**: Bonjour finds Windows host automatically; app connects on first start

### Threading Model

| Component | Thread | Purpose |
|-----------|--------|---------|
| `BonjourBrowser` | Main (RunLoop) | Service discovery |
| `StreamClient` | Global(userInteractive) | Network I/O |
| `VideoDecoder` | VideoToolbox background | H.264 decoding |
| `MetalRenderer` | CADisplayLink (Main) | GPU rendering |
| `InputCaptureView` | Main | Touch event handling |

---

## 🚀 Quick Start

### Prerequisites Checklist
- [ ] iPad with Wi-Fi enabled
- [ ] Windows host on same Wi-Fi network running the CarSide server
- [ ] Mac with Xcode 12+ installed
- [ ] All source files (*.swift, *.plist)

### 30-Second Overview
1. Clone/download source files to your Mac
2. Open Xcode, create new iOS App project
3. Copy `.swift` files into Xcode project
4. Add `Info.plist` to project
5. Build and run on iPad via Xcode
6. App auto-connects to Windows host

---

## 📦 Installation Guide

### Step 1: Prepare Your Development Environment

#### On macOS:
1. **Install Xcode**
   ```bash
   # Via App Store (easiest)
   # Or via command line
   xcode-select --install
   ```

2. **Verify Installation**
   ```bash
   xcode-select -p
   # Should output: /Applications/Xcode.app/Contents/Developer
   ```

3. **Download Source Files**
   - Save all `.swift` files to a working directory
   - Keep the `Info.plist` file

### Step 2: Create Xcode Project

1. **Launch Xcode**
   ```bash
   open -a Xcode
   ```

2. **Create New Project**
   - File → New → Project
   - Choose **iOS**
   - Select **App**
   - Click **Next**

3. **Configure Project**
   - **Product Name**: `CarSide`
   - **Team**: Select your Apple Team (or create free account)
   - **Organization Identifier**: `com.example.carside`
   - **Interface**: SwiftUI or Storyboard (StoryboardUI recommended)
   - **Language**: Swift
   - Click **Next** → Choose location → **Create**

### Step 3: Add Source Files

1. **Remove Generated Files**
   - In Xcode, delete `ViewController.swift` (if auto-generated)
   - Delete `Main.storyboard` (we provide our own)

2. **Drag-and-Drop Swift Files**
   - In Finder, select all `.swift` files:
     - `AppDelegate.swift`
     - `ViewController.swift`
     - `StreamClient.swift`
     - `VideoDecoder.swift`
     - `InputCapture.swift`
     - `BonjourDiscovery.swift`
   - Drag into Xcode's file navigator
   - Check **"Copy items if needed"** ✓
   - Click **Finish**

3. **Add Info.plist**
   - Drag `Info.plist` into Xcode
   - Select target **CarSide**
   - Xcode will use it as the app configuration

### Step 4: Configure Project Settings

1. **Info Tab** (Targets → General)
   - Minimum iOS Deployment: **13.0**
   - Supported Interface Orientations:
     - ✓ Portrait
     - ✓ Landscape Left
     - ✓ Landscape Right
   - Requires Full Screen: ✓ (checked)

2. **Build Settings Tab**
   - Search for "Swift Language"
   - Ensure **Swift version** is 5.5+

3. **Signing & Capabilities**
   - Select Team: (your Apple account)
   - Bundle Identifier will auto-fill
   - Ensure signing certificate is valid

### Step 5: Verify Build

```bash
# In Xcode, press Cmd+B to build
# Output should show: "Build complete!"
```

---

## 🏗️ Building on macOS

### Via Xcode GUI (Recommended for First-Time)

1. **Connect iPad**
   - Plug iPad via USB cable (or Wi-Fi if already set up)
   - Unlock iPad, tap **Trust** if prompted

2. **Select Device**
   - Top-left of Xcode window: Click scheme selector
   - Choose your iPad (e.g., "iPad (Personal)")

3. **Build & Run**
   - Press **Cmd+R** (or Product → Run)
   - Xcode compiles, deploys, and launches app on iPad
   - Watch console for logs

### Via Command Line (Advanced)

```bash
cd /path/to/CarSide/project

# Build for iPad
xcodebuild -scheme CarSide -destination 'generic/platform=iOS' build

# Build and run directly (requires connected iPad)
xcodebuild -scheme CarSide test -destination 'platform=iOS,name=Your iPad Name'
```

---

## 📱 iPad Setup & Launch

### Initial Setup (First Time Only)

#### On your iPad:

1. **Connect to Wi-Fi**
   - Settings > Wi-Fi
   - Select same network as Windows host
   - Verify "Connected"

2. **Enable Developer Mode** (if using Xcode wirelessly)
   - Settings > Privacy & Security > Developer Mode
   - Toggle **ON**
   - Restart iPad

3. **Trust Mac Developer Certificate**
   - After first app install via Xcode:
     - Device may show security prompt
     - Settings > General > VPN & Device Management
     - Select certificate, tap **Trust**

### Installing via Xcode

**First Time:**
```
1. Connect iPad via USB cable
2. Unlock iPad
3. In Xcode: Product → Run (Cmd+R)
4. Wait for "App installed successfully"
5. App launches automatically
```

**Subsequent Times:**
```
1. iPad can stay on Wi-Fi (if Xcode > Windows host + iPad)
2. Just press Cmd+R in Xcode
3. App redeploys and relaunches
```

### Launching the App on iPad

1. **Home Screen**
   - Find **CarSide** icon
   - Tap to launch

2. **First Launch**
   - App displays **black screen** briefly (normal)
   - Bonjour scans for Windows host (up to 5 seconds)
   - Screen fills with Windows desktop when found
   - ✅ If you see desktop → **Success!**

3. **If Black Screen Persists**
   - See [Troubleshooting section](#troubleshooting)

---

## 🎮 Usage

### Interacting with Desktop

| Input | Action |
|-------|--------|
| **Single Finger** | Move cursor / click |
| **Two Fingers** | Scroll / pan |
| **Apple Pencil** | Precision input + pressure (supported in compatible apps) |
| **Long Press** | Right-click context menu |
| **Pinch** | Zoom (when Drawing Pad mode active) |

### Modes

**Standard Mode** (Default)
- Touch and pencil both control cursor
- Windows receives all input as mouse/pointer events

**Drawing Pad Mode** (Optional)
- Pencil only for input
- Two-finger gestures for canvas navigation
- Top toolbar: Toggle mode, clear canvas, pick colors

### Keyboard Support
- Connect Bluetooth keyboard to iPad
- Type normally — keyboard input forwarded to Windows
- ⌘+Tab, Alt+Tab work as expected

---

## 📂 Project Structure

```
CarSide/
├── AppDelegate.swift           # App lifecycle, window setup
├── ViewController.swift        # Main view controller, component coordination
├── StreamClient.swift          # Network client, H.264 frame receiving
├── VideoDecoder.swift          # H.264 decoding (VideoToolbox) + Metal rendering
├── InputCapture.swift          # Touch/pencil capture, input serialization
├── BonjourDiscovery.swift      # Bonjour mDNS service discovery
├── Info.plist                  # App configuration, permissions
├── InputEvent.h                # Input event struct (iOS ↔ Windows)
├── InputInjector.cpp           # Windows-side input injection
├── FrameCapturer.cpp          # Windows frame capture (DXGI)
├── NvencEncoder.cpp           # NVIDIA video encoding
├── StreamServer.cpp           # Windows TCP server
├── IddSampleDriver.cpp        # Virtual display driver (IDD)
└── README.md                  # This file
```

### Component Responsibilities

| File | Role |
|------|------|
| **AppDelegate** | App lifecycle, root view setup |
| **ViewController** | Scene coordination, error handling |
| **StreamClient** | TCP connection, frame/input I/O |
| **VideoDecoder** | H.264 NAL parsing, VideoToolbox integration |
| **MetalRenderer** | GPU rendering (zero-copy) |
| **InputCaptureView** | Touch event capture, JSON encoding |
| **BonjourBrowser** | Wi-Fi service discovery (mDNS) |

---

## 🔧 Troubleshooting

### Common Issues

#### 1. **App Shows Black Screen**
- **Symptom**: App launches but screen stays black
- **Causes**: 
  - Windows server not running or not on same Wi-Fi
  - Bonjour service not advertised
  - Metal not supported on device
- **Solution**:
  ```
  1. Check Windows host is running CarSide server
  2. Verify iPad and host are on SAME Wi-Fi network
  3. Restart both devices
  4. Check device has A9 chip or later (Settings > General > About)
  ```

#### 2. **Connection Drops After a Few Seconds**
- **Symptom**: Video appears, then connection fails
- **Causes**:
  - Network unstable or interference
  - TCP timeout on Windows side
  - Firewall blocking port 7878
- **Solution**:
  ```
  1. Move closer to Wi-Fi router
  2. Check Windows firewall: allow port 7878
  3. Reduce Wi-Fi interference (change channel in router)
  ```

#### 3. **Touch Input Not Working**
- **Symptom**: See video but touch doesn't respond
- **Causes**:
  - Input injection failing on Windows side
  - Wrong input mode
  - Windows driver issue
- **Solution**:
  ```
  1. Try touch + hold (right-click) to test
  2. Check Windows event logs for errors
  3. Restart CarSide server on Windows
  4. Verify Windows receives input via Settings > Devices > Touch
  ```

#### 4. **Xcode Can't Find Device**
- **Symptom**: iPad doesn't appear in device list
- **Causes**:
  - USB cable not connected (or cable issue)
  - Trust not established
  - Device locked or screen off
- **Solution**:
  ```
  1. Try different USB cable
  2. Unlock iPad, tap "Trust"
  3. Restart Xcode: Cmd+Q, then reopen
  4. Try connecting wirelessly (Settings > General > AirPlay & Handoff > Wirelessly Connected Macs)
  ```

#### 5. **Build Fails: "Metal is not supported"**
- **Symptom**: Alert on iPad saying device not supported
- **Causes**: Device is iPad mini 1-3, iPad 2-4, iPad Air 1
- **Solution**:
  ```
  Use iPad Air 2 or later, iPad Mini 4+, or iPad Pro
  ```

### Enabling Debug Logs

To see detailed logs in Xcode console:

1. In Xcode: Product → Scheme → Edit Scheme
2. Run → Arguments Passed On Launch:
   ```
   -com.carside.debug YES
   ```
3. Run again — console will show detailed logs

---

## Method 1: Swift Playgrounds (No-Mac Quick Test)

If you want to try the app immediately on your iPad without a Mac, this is the fastest way.

1. Install **Swift Playgrounds** from the App Store.
2. Create a new **App** project in Playgrounds.
3. Open `ContentView.swift`.
4. Copy the Swift code from `ViewController.swift`, `StreamClient.swift`, `VideoDecoder.swift`, and `BonjourDiscovery.swift` into the appropriate Playgrounds files:
   - `ContentView.swift` (UI entry) for simple preview code and mock rendering.
   - Add helper files by creating new Swift files in the project for networking and decoding logic.
5. Wire the UI to your data path: replace SwiftUI `onAppear` and button actions with `streamClient.connect(host:port:)` and `inputView` integration.
6. Tap **Play** to run.

**Limitation:** This is for quick local validation only (proof-of-concept).
- runs while Swift Playgrounds is open
- does not have full system permissions (deep system APIs, lock screen overrides, or driver-level access) and can’t install as a released App Store target.

---

## 🔒 Advanced

### Network & Security

- **Firewall Rules**: Ensure port **7878 (TCP)** is open on Windows/iPad firewall
- **Bonjour mDNS**: Requires multicast enabled on network (standard)
- **Encryption**: Currently unencrypted; use VPN/isolated network for sensitive data

### Performance Tuning

**On Windows:**
- Use NVIDIA NVENC for hardware encoding (fastest)
- Ensure Windows is wired to router for lowest latency

**On iPad:**
- Use iPad close to router (Wi-Fi 5/6 recommended)
- Disable background apps (Settings > General > Background App Refresh)

### Building Windows Server Components

See `IddSampleDriver.cpp`, `NvencEncoder.cpp`, `FrameCapturer.cpp` for Windows side.

Requires:
- Windows Driver Kit (WDK)
- NVIDIA Video Codec SDK
- Visual Studio 2019+

---

## 📄 License

This project is provided as-is for educational and personal use.

---

## ❓ FAQ

**Q: Works with other encoders besides NVENC?**
A: Yes, CPU H.264 encoding works as fallback but is slower.

**Q: Can I use over internet (not local Wi-Fi)?**
A: Currently designed for LAN. Remote use requires VPN setup (advanced).

**Q: iPhone support?**
A: Possible but untested; requires smaller UI adaptation.

**Q: Latency?**
A: Typically 30–100ms over Wi-Fi; depends on network quality.

---

## 📞 Support

For issues, check:
1. [Troubleshooting section](#troubleshooting)
2. Xcode console logs
3. Windows Event Viewer for server errors
4. Ensure all dependencies installed correctly

---

**Happy extending!** 🎉 Enjoy your iPad as a wireless display for Windows.