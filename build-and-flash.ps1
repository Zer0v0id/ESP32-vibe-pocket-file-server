# ESP-IDF build helper
# Run from any PowerShell. Cleans, sets target esp32s3, and builds. Does not flash;
# you are prompted to run the flash command yourself.
# Use -WithDisplay for SSD1306 OLED firmware (build-display).
# If idf.py is not in PATH, launches a new window using Initialize-Idf.ps1.

param(
    [string]$ProjectDir = "",
    [switch]$LaunchIdfFirst = $false,
    [switch]$WithDisplay = $false
)

$ErrorActionPreference = "Stop"

# Defaults (edit these or press Enter at prompts to use them)
$DefaultEspressifRoot   = "C:\Espressif"
$DefaultProjectDir      = "D:\DEV\ESP_File_Server"
# Same as IDE shortcut: Initialize-Idf.ps1 and IdfId (ESP-IDF 5.5.2)
$InitializeIdfScript    = "C:\Espressif\Initialize-Idf.ps1"
$IdfId                  = "esp-idf-b29c58f93b4ca0f49cdfc4c3ef43b562"

function Get-DefaultIdfPath {
    $frameworksDir = Join-Path $DefaultEspressifRoot "frameworks"
    if (-not (Test-Path $frameworksDir)) { return $null }
    $idfDirs = Get-ChildItem -Path $frameworksDir -Directory -Filter "esp-idf-*" -ErrorAction SilentlyContinue | Sort-Object Name -Descending
    if ($idfDirs.Count -gt 0) { return $idfDirs[0].FullName }
    return $null
}

function Ensure-IdfInPath {
    if (Get-Command idf.py -ErrorAction SilentlyContinue) {
        Write-Host "ESP-IDF already in PATH." -ForegroundColor Green
        return $true
    }
    $idfPath = $env:IDF_PATH
    if (-not $idfPath -or -not (Test-Path $idfPath)) {
        $idfPath = Get-DefaultIdfPath
    }
    if (-not $idfPath -or -not (Test-Path $idfPath)) {
        Write-Host "idf.py not found and IDF_PATH not set or invalid." -ForegroundColor Yellow
        $defaultPrompt = if (Get-DefaultIdfPath) { Get-DefaultIdfPath } else { "e.g. C:\Espressif\frameworks\esp-idf-v5.5.2" }
        $idfPath = Read-Host "Enter your ESP-IDF path [$defaultPrompt]"
        if ([string]::IsNullOrWhiteSpace($idfPath)) { $idfPath = Get-DefaultIdfPath }
        if (-not $idfPath -or -not (Test-Path $idfPath)) {
            Write-Host "Invalid path. Run this script from an ESP-IDF PowerShell instead, or set IDF_PATH." -ForegroundColor Red
            exit 1
        }
        $env:IDF_PATH = $idfPath
    }
    $exportScript = Join-Path $idfPath "export.ps1"
    if (-not (Test-Path $exportScript)) {
        Write-Host "export.ps1 not found at $exportScript" -ForegroundColor Red
        exit 1
    }
    Write-Host "Activating ESP-IDF from $idfPath ..." -ForegroundColor Cyan
    & $exportScript -Scope CurrentProcess
    if (-not (Get-Command idf.py -ErrorAction SilentlyContinue)) {
        Write-Host "Activation failed. Run this script from an ESP-IDF PowerShell." -ForegroundColor Red
        exit 1
    }
}

function Invoke-BuildAndFlash {
    param(
        [string]$projectDir,
        [switch]$WithDisplay
    )

    Set-Location $projectDir
    Write-Host "Working in: $(Get-Location)" -ForegroundColor Cyan

    $idfArgs = @()
    $buildDirName = "build"
    if ($WithDisplay) {
        $idfArgs = @("-B", "build-display", "-DWITH_DISPLAY=ON")
        $buildDirName = "build-display"
        Write-Host "Building WITH SSD1306 OLED display." -ForegroundColor Cyan
    } else {
        Write-Host "Building WITHOUT display (default). Use -WithDisplay for OLED firmware." -ForegroundColor Cyan
    }

    $buildDir = Join-Path $projectDir $buildDirName
    Write-Host "Removing $buildDirName folder and contents..." -ForegroundColor Yellow
    if (Test-Path $buildDir) {
        Remove-Item -Path $buildDir -Recurse -Force
        Write-Host "Build folder removed." -ForegroundColor Green
    } else {
        Write-Host "No existing $buildDirName folder." -ForegroundColor Gray
    }

    Write-Host "Running idf.py fullclean..." -ForegroundColor Cyan
    idf.py @idfArgs fullclean
    if ($LASTEXITCODE -ne 0) {
        Write-Host "fullclean failed." -ForegroundColor Red
        exit $LASTEXITCODE
    }

    Write-Host "Running idf.py set-target esp32s3..." -ForegroundColor Cyan
    idf.py @idfArgs set-target esp32s3
    if ($LASTEXITCODE -ne 0) {
        Write-Host "set-target failed. Fix the error above (e.g. Python/ESP-IDF env) then run again." -ForegroundColor Red
        exit $LASTEXITCODE
    }

    Write-Host "Running idf.py build..." -ForegroundColor Cyan
    idf.py @idfArgs build
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Build failed." -ForegroundColor Red
        exit $LASTEXITCODE
    }

    Write-Host "Build completed successfully." -ForegroundColor Green
    Write-Host ""
    Write-Host "To flash the device, run:" -ForegroundColor Cyan
    if ($WithDisplay) {
        Write-Host "  idf.py -B build-display -DWITH_DISPLAY=ON flash" -ForegroundColor White
        Write-Host "or, to specify a port:" -ForegroundColor Cyan
        Write-Host "  idf.py -B build-display -DWITH_DISPLAY=ON -p COMx flash" -ForegroundColor White
    } else {
        Write-Host "  idf.py flash" -ForegroundColor White
        Write-Host "or, to specify a port:" -ForegroundColor Cyan
        Write-Host "  idf.py -p COMx flash" -ForegroundColor White
    }
    Write-Host ""
}

# --- Main ---
if ($LaunchIdfFirst) {
    # Launched as child: run Initialize-Idf.ps1 then build (ProjectDir passed in)
    if (-not (Test-Path $InitializeIdfScript)) {
        Write-Host "Initialize-Idf.ps1 not found at $InitializeIdfScript" -ForegroundColor Red
        exit 1
    }
    Write-Host "Running Initialize-Idf.ps1 -IdfId $IdfId ..." -ForegroundColor Cyan
    & $InitializeIdfScript -IdfId $IdfId
    if (-not (Get-Command idf.py -ErrorAction SilentlyContinue)) {
        Write-Host "Initialize-Idf.ps1 did not set up idf.py." -ForegroundColor Red
        exit 1
    }
    Invoke-BuildAndFlash -projectDir $ProjectDir -WithDisplay:$WithDisplay
    exit 0
}

# Normal run: prompt for dir, then either run here or launch IDE-style window
$projectDir = $ProjectDir
if ([string]::IsNullOrWhiteSpace($projectDir)) {
    $projectDir = Read-Host "Enter project directory [$DefaultProjectDir]"
    if ([string]::IsNullOrWhiteSpace($projectDir.Trim())) { $projectDir = $DefaultProjectDir } else { $projectDir = $projectDir.Trim() }
}
if (-not (Test-Path $projectDir)) {
    Write-Host "Directory does not exist: $projectDir" -ForegroundColor Red
    exit 1
}

if (Get-Command idf.py -ErrorAction SilentlyContinue) {
    Invoke-BuildAndFlash -projectDir $projectDir -WithDisplay:$WithDisplay
    exit 0
}

# idf.py not in PATH: launch new PowerShell with Initialize-Idf.ps1 (same as IDE shortcut) then run steps
if (-not (Test-Path $InitializeIdfScript)) {
    Write-Host "idf.py not in PATH and Initialize-Idf.ps1 not found at $InitializeIdfScript" -ForegroundColor Red
    Write-Host "Run this script from an ESP-IDF PowerShell, or use export.ps1 from your IDF path." -ForegroundColor Yellow
    exit 1
}

$scriptPath = $MyInvocation.MyCommand.Path
$argList = @("-ExecutionPolicy", "Bypass", "-NoExit", "-File", $scriptPath, "-LaunchIdfFirst", "-ProjectDir", $projectDir)
if ($WithDisplay) { $argList += "-WithDisplay" }
Write-Host "Launching ESP-IDF window (Initialize-Idf.ps1) and running build there..." -ForegroundColor Cyan
Start-Process -FilePath "powershell.exe" -ArgumentList $argList
