# ESP-IDF build helper
# Run from any PowerShell. Cleans, sets target esp32s3, and builds. Does not flash;
# you are prompted to run the flash command yourself. If idf.py is not in PATH,
# launches a new window using Initialize-Idf.ps1 (same as the ESP-IDF PowerShell shortcut).

param(
    [string]$ProjectDir = "",
    [switch]$LaunchIdfFirst = $false
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
    param([string]$projectDir)

    Set-Location $projectDir
    Write-Host "Working in: $(Get-Location)" -ForegroundColor Cyan

    # Order: (1) delete build dir, (2) fullclean, (3) set-target, (4) build, (5) flash
    $buildDir = Join-Path $projectDir "build"
    Write-Host "Removing build folder and contents..." -ForegroundColor Yellow
    if (Test-Path $buildDir) {
        Remove-Item -Path $buildDir -Recurse -Force
        Write-Host "Build folder removed." -ForegroundColor Green
    } else {
        Write-Host "No existing build folder." -ForegroundColor Gray
    }

    Write-Host "Running idf.py fullclean..." -ForegroundColor Cyan
    idf.py fullclean
    if ($LASTEXITCODE -ne 0) {
        Write-Host "fullclean failed." -ForegroundColor Red
        exit $LASTEXITCODE
    }

    Write-Host "Running idf.py set-target esp32s3..." -ForegroundColor Cyan
    idf.py set-target esp32s3
    if ($LASTEXITCODE -ne 0) {
        Write-Host "set-target failed. Fix the error above (e.g. Python/ESP-IDF env) then run again." -ForegroundColor Red
        exit $LASTEXITCODE
    }

    Write-Host "Running idf.py build..." -ForegroundColor Cyan
    idf.py build
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Build failed." -ForegroundColor Red
        exit $LASTEXITCODE
    }

    Write-Host "Build completed successfully." -ForegroundColor Green
    Write-Host ""
    Write-Host "To flash the device, run:" -ForegroundColor Cyan
    Write-Host "  idf.py flash" -ForegroundColor White
    Write-Host "or, to specify a port:" -ForegroundColor Cyan
    Write-Host "  idf.py -p COMx flash" -ForegroundColor White
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
    Invoke-BuildAndFlash -projectDir $ProjectDir
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
    Invoke-BuildAndFlash -projectDir $projectDir
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
Write-Host "Launching ESP-IDF window (Initialize-Idf.ps1) and running build there..." -ForegroundColor Cyan
Start-Process -FilePath "powershell.exe" -ArgumentList $argList
