# CarSide Windows Server Setup Guide

Complete step-by-step instructions for setting up the CarSide wireless display server on Windows.

## 📋 Prerequisites

- Windows 10 Build 19041 or Windows 11
- Administrator access
- One of:
  - AMD Ryzen 3XXX+ with Radeon Graphics (or discrete AMD GPU)
  - NVIDIA GeForce GTX 750 Ti or newer
  - Intel 10th Gen processor or Arc GPU
  - (No GPU required for CPU software encoding fallback)
- Visual Studio 2019 or 2022 Community Edition
- Internet connection for downloads

## 🚀 Quick Start (5 Minutes)

### 1. Install Visual Studio & Dependencies

```powershell
# Step 1: Download Visual Studio Community 2022
# URL: https://visualstudio.microsoft.com/downloads/

# Step 2: Run installer and select:
#   ✓ Desktop development with C++
#   ✓ Windows 10/11 SDK (Build 19041+)

# Wait for completion (~5-8 GB)
```

### 2. Copy CarSide Project

```powershell
# Extract/clone CarSide to your desired location
# Example:
#   C:\Users\YourName\Source\CarSide\
```

### 3. Run Setup Script (Automated)

```powershell
# Open PowerShell as Administrator
# Navigate to CarSide directory
cd C:\path\to\CarSide

# Run setup (enables drivers, firewall, Bonjour)
powershell -ExecutionPolicy Bypass -File Setup-CarSide.ps1 -All

# Restart when prompted
```

### 4. Build Project

```
# Open CarSide.sln in Visual Studio
# Select: Release | x64
# Build → Build Solution (Ctrl+Shift+B)
# Output: bin\x64\Release\CarSideServer.exe
```

### 5. Run Server

```powershell
C:\path\to\CarSide\bin\x64\Release\CarSideServer.exe

# Should display:
#   [StreamServer] Started on port 7878
#   [Bonjour] Service registered as 'CarSide._winextend._tcp.local.:7878'
#   [Main] Server ready - waiting for iPad client...
```

### 6. Deploy iPad App

```bash
# On Mac with Xcode:
cd /path/to/CarSide/iOS
open CarSide.xcworkspace

# Select iPhone/iPad device
# Product → Run (Cmd+R)
```

✅ **Done!** iPad should auto-detect and connect.

---

## 🔧 Detailed Setup Steps

### Step 0: Install Development Tools

#### Visual Studio 2019+ Community Edition (FREE)

1. Download: https://visualstudio.microsoft.com/downloads/
2. Run `VisualStudioSetup.exe`
3. Click **Continue**
4. Select **Desktop development with C++** checkbox
5. Click the **Installation details** panel on right
6. Expand **SDK, libraries, and frameworks** and ensure:
   - ✓ Windows 10 SDK (19041 or later)
   - ✓ Visual C++ tools for Windows
7. Click **Install** (grab coffee ☕, takes 5-10 minutes)
8. After completion, close installer

#### Windows Driver Kit (WDK) — Optional

For full IDD driver support:

1. Download: https://docs.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk
2. Follow installer steps
3. Add to Visual Studio: Extensions → Add or Remove Extensions

#### Bonjour SDK for Windows — Optional

For automatic network service discovery:

1. Download: https://support.apple.com/downloads/DL888/en_US/BonjourSDKforWindows.zip
2. Extract to `C:\Program Files\Bonjour`
3. In Visual Studio project properties:
   - C/C++ → General → Additional Include Directories:
     - Add: `C:\Program Files\Bonjour\include`
   - Linker → General → Additional Library Directories:
     - Add: `C:\Program Files\Bonjour\lib`

---

### Step 1: Clone/Download CarSide Repository

```powershell
# Option A: Using Git
git clone https://github.com/your-repo/CarSide.git
cd CarSide

# Option B: Manual download
# Download ZIP from GitHub
# Extract to C:\Users\YourName\Source\CarSide
```

---

### Step 2: Enable Test Signing (For Drivers)

This allows unsigned kernel drivers to load (required for IDD virtual display driver).

#### Automated (Recommended):

```powershell
# Open PowerShell as Administrator
powershell -ExecutionPolicy Bypass

cd C:\path\to\CarSide
.\Setup-CarSide.ps1 -EnableTestSigning
```

#### Manual:

```powershell
# Run as Administrator
bcdedit /set testsigning on

# Restart Windows
shutdown /r /t 0
```

**After restart, verify:**
```powershell
bcdedit /enum | findstr testsigning
# Should show: testsigning             Yes
```

---

### Step 3: Build CarSide Server

#### Via Visual Studio (GUI)

1. **Open Visual Studio**
   - File → Open → Project/Solution
   - Select `CarSide.sln`

2. **Select Configuration**
   - Top toolbar: Change "Debug" to **Release**
   - Change "x86" to **x64**

3. **Build Solution**
   - Ctrl+Shift+B
   - Or: Build → Build Solution
   - Watch Output panel for compile messages
   - Should end with: "Build succeeded"

4. **Locate Output**
   - Navigate to: `bin\x64\Release\`
   - Find: `CarSideServer.exe`

#### Via Command Line (Advanced)

```powershell
# Using MSBuild
$vsPath = "C:\Program Files\Microsoft Visual Studio\2022\Community"
$msbuild = "$vsPath\MSBuild\Current\Bin\MSBuild.exe"

& $msbuild CarSide.sln /p:Configuration=Release /p:Platform=x64 /m
```

---

### Step 4: Register IDD Driver (If Using Virtual Display)

#### Automated:

```powershell
# Run as Administrator
.\Setup-CarSide.ps1 -RegisterDriver
```

#### Manual:

```powershell
# Run as Administrator (after test signing is enabled)
pnputil /add-driver CarSideIdd.inf /install

# Verify installation
pnputil /enum-devices /drivers | findstr CarSide
```

**To uninstall:**
```powershell
pnputil /delete-driver CarSideIdd.inf
```

---

### Step 5: Configure Windows Firewall

#### Automated:

```powershell
# Run as Administrator
.\Setup-CarSide.ps1 -OpenFirewall
```

#### Manual:

```powershell
# Run as Administrator
netsh advfirewall firewall add rule ^
  name="CarSide" ^
  dir=in ^
  action=allow ^
  protocol=TCP ^
  localport=7878 ^
  program="C:\path\to\CarSideServer.exe" ^
  description="CarSide wireless display server"

# Verify
netsh advfirewall firewall show rule name="CarSide"
```

---

### Step 6: Configure Video Encoding

The server **auto-detects** your GPU and selects the best encoder:

#### Check Your GPU

```powershell
# Run diagnostics
Get-WmiObject Win32_VideoController

# Look for:
#   ✓ AMD Radeon RX 5700 XT (uses VCN encoder)
#   ✓ NVIDIA GeForce RTX 3080 (uses NVENC)  
#   ✓ Intel Arc A770 (uses QuickSync)
#   ✓ Intel Iris Xe Graphics (uses QuickSync)
```

#### Configure NVIDIA (if you have GeForce GPU)

1. Download NVIDIA Video Codec SDK:
   - URL: https://developer.nvidia.com/nvidia-video-codec-sdk
   - Login with NVIDIA account (free)
   - Download latest `nvenc-sdk.zip`

2. Extract to `C:\nvenc-sdk\`

3. In Visual Studio project properties:
   - C/C++ → Additional Include Directories:
     - Add: `C:\nvenc-sdk\Interface`
   - Linker → Additional Library Directories:
     - Add: `C:\nvenc-sdk\Lib`

#### AMD / Intel / CPU

No additional setup required! Windows Media Foundation + DXVA2 handles everything.

---

### Step 7: Run the Server

```powershell
# Navigate to build output directory
cd C:\path\to\CarSide\bin\x64\Release

# Run the server
.\CarSideServer.exe

# You should see:
#   ╔════════════════════════════════════════╗
#   ║  CarSide Server v1.0                   ║
#   ║  Wireless Display Extension for iPad   ║
#   ╚════════════════════════════════════════╝
#   
#   [Config] Port: 7878
#   [Init] Initializing components...
#   [Init] All components initialized successfully
#   
#   [Main] Server ready - waiting for iPad client...
#   [Main] Advertised as: CarSide._winextend._tcp.local.:7878
#   [Main] Press Ctrl+C to stop
```

**Troubleshooting:**
- If port 7878 is in use: `netstat -ano | findstr 7878`
- If Bonjour fails: Ensure Bonjour SDK is installed or server will skip advertising
- If encoder fails: Check GPU driver support (Windows Update may have a newer driver)

---

### Step 8: Build & Deploy iPad App

#### On macOS with Xcode

```bash
# Navigate to iOS project (if separate)
cd /path/to/CarSide/iOS

# Option 1: Using Xcode
open -a Xcode CarSide.xcodeproj

# Option 2: Command line
xcodebuild -scheme CarSide -configuration Release \
  -destination 'generic/platform=iOS' build
```

#### Via Xcode GUI (Easier)

1. **Connect iPad via USB**
   - Plug iPad to Mac with USB cable
   - Unlock iPad
   - Tap **Trust** if prompted

2. **Open Xcode**
   ```bash
   open -a Xcode CarSide.xcodeproj
   ```

3. **Select iPad Device**
   - Top toolbar: Click scheme selector (next to "Run" button)
   - Choose your iPad name

4. **Build & Deploy**
   - Press **Cmd+R** or Product → Run
   - Xcode builds and deploys app
   - App launches automatically on iPad

5. **First Launch on iPad**
   - May ask to trust developer cert (**General** → **VPN & Device Management** → **Trust**)
   - App searches for Windows host (up to 5 seconds)
   - Screen should fill with Windows desktop when found
   - ✅ **Success!**

---

## 🧪 Verification & Testing

### Test 1: Verify Server is Running

On Windows:
```powershell
netstat -ano | findstr 7878
# If running, should show:
#   TCP    0.0.0.0:7878    0.0.0.0:0    LISTENING    <PID>
```

### Test 2: Verify Firewall Port is Open

From another machine on LAN:
```powershell
Test-NetConnection -ComputerName <computer-name> -Port 7878 -ErrorAction SilentlyContinue

# Should show:
#   ComputerName     : <computer-name>
#   RemoteAddress    : 192.168.x.x
#   RemotePort       : 7878
#   TcpTestSucceeded : True
```

### Test 3: Verify Bonjour Advertisement

On iPad or another Mac:
```bash
# On Mac
dns-sd -B _winextend._tcp local

# Should list:
#   CarSide._winextend._tcp local.
```

### Test 4: Verify GPU Encoding

Check the server console output:
```
[Init] Video encoder initialized: AMD (Windows Media Foundation)
[StreamingLoop] Encoding @ 60 FPS, ~8 Mbps
```

Monitor GPU usage:
```powershell
# Windows Task Manager
# Performance tab → GPU
# Should see 30-60% GPU usage (video encoder at work)
```

---

## 📱 Autostart & Deployment

### Add to Windows Startup

```powershell
# Create shortcut in Startup folder
$start = "C:\ProgramData\Microsoft\Windows\Start Menu\Programs\Startup"
$target = "C:\path\to\CarSide\bin\x64\Release\CarSideServer.exe"
$shortcut = (New-Object -ComObject WScript.Shell).CreateShortcut("$start\CarSideServer.lnk")
$shortcut.TargetPath = $target
$shortcut.Save()
```

### Or Use Task Scheduler

1. Open **Task Scheduler**
2. Right-click **Task Scheduler Library** → **Create Basic Task...**
3. Name: `CarSide Server`
4. **Trigger**: `At Startup`
5. **Action**: `Start a Program`
   - Program: `C:\path\to\CarSide\bin\x64\Release\CarSideServer.exe`
6. Check: ✓ **Run with highest privileges**
7. Click **OK**

---

## 🆘 Troubleshooting

### Issue: "Build failed with errors"

**Check:**
1. Visual Studio C++ workload installed
2. Windows SDK 19041+ installed
3. No other Visual Studio instances have the solution open
4. Project file paths are correct

**Solution:**
```powershell
# Clean and rebuild
msbuild CarSide.sln /p:Configuration=Release /p:Platform=x64 /t:Clean
msbuild CarSide.sln /p:Configuration=Release /p:Platform=x64 /t:Build
```

### Issue: "Server starts but iPad can't connect"

**Check:**
1. Both on same Wi-Fi network: `ipconfig` on Windows, Settings on iPad
2. Firewall port open: `netstat -ano | findstr 7878`
3. Bonjour running: Check Output for `[Bonjour] Service registered`
4. iPad app can see host: Launch CarSide app, watch for service discovery

**Solution:**
```powershell
# Restart server
# Restart iPad
# Temporarily disable Windows Defender firewall for testing:
netsh advfirewall set allprofiles state off
# Re-enable after:
netsh advfirewall set allprofiles state on
```

### Issue: "GPU Encoding not working"

**Check:**
1. GPU driver is up-to-date: Windows Update or vendor site
2. Correct GPU is being used: Task Manager → Performance

**Solution:**
```powershell
# Force CPU fallback (for testing)
# Edit: ServerInitializer.cpp line ~60 to use software codec
# Rebuild project
```

### Issue: "Bonjour service not found on iPad"

**Check:**
1. Bonjour SDK installed (optional but recommended)
2. Service advertised: Check console output

**Solution:**
1. Install Bonjour SDK from Apple
2. Or manually specify server IP/port in iPad app (if app supports it)

---

## 📞 Support & Resources

- **GitHub Issues**: https://github.com/your-repo/CarSide/issues
- **Windows Drivers**: https://docs.microsoft.com/en-us/windows-hardware/drivers/
- **Bonjour**: https://support.apple.com/downloads/DL888/en_US/BonjourSDKforWindows.zip
- **Visual Studio**: https://visualstudio.microsoft.com/support/

---

## ✅ Success Checklist

- [ ] Visual Studio 2019+ installed
- [ ] Windows SDK 19041+ installed  
- [ ] CarSide project cloned/downloaded
- [ ] Setup script ran successfully
- [ ] Project builds without errors (Release | x64)
- [ ] CarSideServer.exe runs
- [ ] Port 7878 visible in `netstat`
- [ ] Firewall rule created
- [ ] iPad app deployed to device
- [ ] iPad detects Windows host
- [ ] Desktop visible on iPad screen

---

## Next Steps

1. ✅ **Server Running**: CarSideServer.exe broadcasting Bonjour service
2. ✅ **iPad Connected**: Displaying Windows desktop
3. 📝 **Configure iPad**: Customize touch modes, keyboard shortcuts
4. 🎨 **Advanced**: Set up Drawing Pad mode, multi-display
5. 🚀 **Deploy**: Package and distribute (advanced usage)

Enjoy your wireless desktop extension! 🎉

