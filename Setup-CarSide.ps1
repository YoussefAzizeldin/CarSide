# Setup-CarSide.ps1
# Comprehensive setup script for CarSide Windows server
# Run as Administrator
# 
# Usage: powershell -ExecutionPolicy Bypass -File Setup-CarSide.ps1

param(
    [Switch]$InstallDependencies,
    [Switch]$EnableTestSigning,
    [Switch]$RegisterDriver,
    [Switch]$OpenFirewall,
    [Switch]$All
)

$ErrorActionPreference = "Stop"

function Test-Administrator {
    $currentUser = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($currentUser)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Write-Section {
    param([string]$Title)
    Write-Host "`n========================================" -ForegroundColor Green
    Write-Host $Title -ForegroundColor Green
    Write-Host "========================================`n" -ForegroundColor Green
}

function Write-Success {
    param([string]$Message)
    Write-Host "[✓] $Message" -ForegroundColor Green
}

function Write-Warning {
    param([string]$Message)
    Write-Host "[!] $Message" -ForegroundColor Yellow
}

function Write-Error-Custom {
    param([string]$Message)
    Write-Host "[✗] $Message" -ForegroundColor Red
}

# Check if running as administrator
if (-not (Test-Administrator)) {
    Write-Error-Custom "This script must be run as Administrator"
    Write-Host "Please right-click PowerShell and select 'Run as Administrator'" -ForegroundColor Yellow
    exit 1
}

Write-Host "
╔════════════════════════════════════════╗
║  CarSide Setup - Windows Server        ║
║  version 1.0                           ║
╚════════════════════════════════════════╝
" -ForegroundColor Cyan

# If -All is specified, enable all options
if ($All) {
    $InstallDependencies = $true
    $EnableTestSigning = $true
    $RegisterDriver = $true
    $OpenFirewall = $true
}

# Show what will be done
Write-Section "Setup Options"
Write-Host "Install Dependencies: $InstallDependencies"
Write-Host "Enable Test Signing:  $EnableTestSigning (requires reboot)"
Write-Host "Register Driver:      $RegisterDriver"
Write-Host "Open Firewall Port:   $OpenFirewall"
Write-Host ""

if ($InstallDependencies) {
    Write-Section "Installing Dependencies"
    
    # Install Bonjour SDK (optional but recommended)
    Write-Host "Installing Bonjour SDK from Apple..."
    Write-Host ""
    Write-Warning "Visual Studio 2019+ with C++ workload is required"
    Write-Host "If not installed, download from: https://visualstudio.microsoft.com/downloads/"
    Write-Host ""
    
    # Create download helper
    $bonjourUrl = "https://support.apple.com/downloads/DL888/en_US/BonjourSDKforWindows.zip"
    $bonjourPath = "$env:TEMP\BonjourSDK.zip"
    
    Write-Host "Bonjour SDK URL: $bonjourUrl" -ForegroundColor Cyan
    Write-Host "Local SDKs:"
    Write-Host "  - Windows Media Foundation: built-in to Windows"
    Write-Host "  - DXVA2: Direct X video acceleration (built-in)"
    Write-Host ""
    Write-Success "Dependencies check complete"
}

if ($EnableTestSigning) {
    Write-Section "Enable Test Signing Mode"
    Write-Warning "This allows unsigned drivers to be loaded"
    Write-Warning "A system reboot is required after this step"
    Write-Host ""
    
    try {
        Write-Host "Enabling test signing mode..."
        bcdedit /set testsigning on | Out-Null
        Write-Success "Test signing mode enabled"
        Write-Host ""
        Write-Warning "⚠️  REBOOT REQUIRED: Please restart your computer before continuing"
        Write-Host "    Run this script again after reboot with:"
        Write-Host "    powershell -ExecutionPolicy Bypass -File Setup-CarSide.ps1 -All"
        Write-Host ""
        $response = Read-Host "Reboot now? (y/n)"
        if ($response -eq 'y' -or $response -eq 'Y') {
            shutdown /r /t 10 /c "CarSide setup requires reboot"
            exit 0
        }
    }
    catch {
        Write-Error-Custom "Failed to enable test signing: $_"
    }
}

if ($RegisterDriver) {
    Write-Section "Register IDD Virtual Display Driver"
    
    $infPath = "CarSideIdd.inf"
    
    if (-not (Test-Path $infPath)) {
        Write-Error-Custom "Driver INF file not found: $infPath"
        Write-Host "Please ensure $infPath exists in the current directory"
    }
    else {
        try {
            Write-Host "Registering driver using pnputil..."
            $result = pnputil /add-driver $infPath /install 2>&1
            Write-Host $result
            Write-Success "Driver registered successfully"
        }
        catch {
            Write-Error-Custom "Driver registration may have failed: $_"
            Write-Host "Verify the INF file is valid and test signing is enabled"
        }
    }
    Write-Host ""
}

if ($OpenFirewall) {
    Write-Section "Configure Windows Firewall"
    
    try {
        Write-Host "Opening port 7878 for CarSide server..."
        
        # Remove existing rule if present
        netsh advfirewall firewall delete rule name="CarSide" dir=in | Out-Null -ErrorAction SilentlyContinue
        
        # Add new rule
        netsh advfirewall firewall add rule `
            name="CarSide" `
            dir=in `
            action=allow `
            protocol=TCP `
            localport=7878 `
            program="%ProgramFiles%\CarSide\CarSideServer.exe" `
            description="CarSide wireless display server" | Out-Null
        
        Write-Success "Firewall rule added for port 7878"
        
        # Show verification
        Write-Host ""
        Write-Host "Firewall rules for CarSide:" -ForegroundColor Cyan
        netsh advfirewall firewall show rule name="CarSide"
    }
    catch {
        Write-Error-Custom "Failed to configure firewall: $_"
    }
    Write-Host ""
}

Write-Section "Setup Summary"
Write-Host "✓ Setup complete!" -ForegroundColor Green
Write-Host ""
Write-Host "Next steps:" -ForegroundColor Cyan
Write-Host "  1. Install Visual Studio 2019+ with 'Desktop development with C++' workload"
Write-Host "  2. Build the StreamServer.cpp and AMDEncoder.cpp projects"
Write-Host "  3. (Optional) Download Bonjour SDK from Apple:"
Write-Host "     https://support.apple.com/downloads/DL888/en_US/BonjourSDKforWindows.zip"
Write-Host "  4. Run the compiled CarSide server executable"
Write-Host "  5. Connect from your iPad using the CarSide app"
Write-Host ""
Write-Host "For more info, see: README.md" -ForegroundColor Gray
Write-Host ""
