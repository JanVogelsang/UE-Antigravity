<#
.SYNOPSIS
    Automates cleaning, building, packaging, and zipping of the Antigravity Unreal Engine plugin.
.DESCRIPTION
    This script searches for an installation of Unreal Engine, cleans the plugin's temporary build
    directories, runs the Unreal Automation Tool (UAT) to package the plugin, and compresses the
    output into a ZIP archive suitable for distribution on the Fab marketplace.
.PARAMETER EnginePath
    Explicit path to the Unreal Engine installation (e.g., "C:\Program Files\Epic Games\UE_5.4").
.PARAMETER UEVersion
    Target Unreal Engine version to search for (e.g., "5.4"). If omitted, the script searches for the latest installed UE 5.x version.
.PARAMETER OutputPath
    Directory where the packaged plugin and ZIP will be created. Defaults to "Packaged" in the repository root.
.PARAMETER NoZip
    If specified, skips compressing the packaged plugin into a ZIP file.
.EXAMPLE
    .\build_plugin.ps1 -UEVersion "5.4"
.EXAMPLE
    .\build_plugin.ps1 -EnginePath "C:\Program Files\Epic Games\UE_5.4" -OutputPath "C:\Builds\Antigravity"
#>

param (
    [string]$EnginePath = "",
    [string]$UEVersion = "",
    [string]$OutputPath = "Packaged",
    [switch]$NoZip
)

$ErrorActionPreference = "Stop"

# Get absolute path for repository root
$RepoRoot = Resolve-Path $PSScriptRoot
$PluginDir = Join-Path $RepoRoot "Antigravity"
$UpluginPath = Join-Path $PluginDir "Antigravity.uplugin"

if (-not (Test-Path $UpluginPath)) {
    Write-Error "Could not find Antigravity.uplugin at '$UpluginPath'. Please run this script from the repository root."
}

# 1. Locate Unreal Engine installation
$ResolvedEnginePath = ""
if (-not [string]::IsNullOrEmpty($EnginePath)) {
    $ResolvedEnginePath = Resolve-Path $EnginePath
} else {
    Write-Host "Searching for Unreal Engine installation..." -ForegroundColor Cyan
    
    # List of versions to search for (newest first)
    $SearchVersions = @()
    if (-not [string]::IsNullOrEmpty($UEVersion)) {
        $SearchVersions += $UEVersion
    } else {
        # Try dynamic lookup first
        $ParentPaths = @("C:\Program Files\Epic Games", "C:\Program Files (x86)\Epic Games", "D:\Epic Games")
        foreach ($ParentPath in $ParentPaths) {
            if (Test-Path $ParentPath) {
                Get-ChildItem -Path $ParentPath -Directory -Filter "UE_5.*" | ForEach-Object {
                    if ($_.Name -match "UE_(5\.\d+)") {
                        $SearchVersions += $Matches[1]
                    }
                }
            }
        }
        # Add fallback hardcoded list to be safe, including newer versions
        $SearchVersions += @("5.9", "5.8", "5.7", "5.6", "5.5", "5.4", "5.3", "5.2", "5.1", "5.0")
        $SearchVersions = $SearchVersions | Select-Object -Unique | Sort-Object -Descending
    }

    foreach ($Ver in $SearchVersions) {
        # Standard Epic Games launcher paths
        $PathsToTry = @(
            "C:\Program Files\Epic Games\UE_$Ver",
            "C:\Program Files (x86)\Epic Games\UE_$Ver",
            "D:\Epic Games\UE_$Ver"
        )
        
        # Try registry lookup
        $RegPath = "HKLM:\SOFTWARE\EpicGames\Unreal Engine\$Ver"
        if (Test-Path $RegPath) {
            $InstalledDir = Get-ItemProperty -Path $RegPath -Name "InstalledDirectory" -ErrorAction SilentlyContinue
            if ($InstalledDir -and $InstalledDir.InstalledDirectory) {
                $PathsToTry += $InstalledDir.InstalledDirectory
            }
        }

        foreach ($Path in $PathsToTry) {
            if (Test-Path $Path) {
                $ResolvedEnginePath = Resolve-Path $Path
                Write-Host "Found Unreal Engine $Ver at: $ResolvedEnginePath" -ForegroundColor Green
                break
            }
        }
        if (-not [string]::IsNullOrEmpty($ResolvedEnginePath)) { break }
    }
}

if ([string]::IsNullOrEmpty($ResolvedEnginePath)) {
    Write-Error "Could not locate Unreal Engine installation. Please specify a path using -EnginePath."
}

$RunUAT = Join-Path $ResolvedEnginePath "Engine\Build\BatchFiles\RunUAT.bat"
if (-not (Test-Path $RunUAT)) {
    Write-Error "Could not find RunUAT.bat at expected path: '$RunUAT'"
}

# 2. Clean previous build folders from the plugin
Write-Host "`nCleaning intermediate and build folders in plugin directory..." -ForegroundColor Cyan
$FoldersToClean = @("Binaries", "Intermediate", "Saved")
foreach ($Folder in $FoldersToClean) {
    $TargetFolder = Join-Path $PluginDir $Folder
    if (Test-Path $TargetFolder) {
        Write-Host "Removing folder: $TargetFolder" -ForegroundColor Yellow
        Remove-Item -Path $TargetFolder -Recurse -Force
    }
}

# 3. Prepare output directory
# Ensure absolute output path
if (-not [System.IO.Path]::IsPathRooted($OutputPath)) {
    $AbsoluteOutputPath = Join-Path $RepoRoot $OutputPath
} else {
    $AbsoluteOutputPath = $OutputPath
}

$PackagedPluginDir = Join-Path $AbsoluteOutputPath "Antigravity"
if (Test-Path $AbsoluteOutputPath) {
    Write-Host "`nCleaning existing output directory '$AbsoluteOutputPath'..." -ForegroundColor Cyan
    Remove-Item -Path $AbsoluteOutputPath -Recurse -Force -ErrorAction SilentlyContinue
}
New-Item -ItemType Directory -Path $AbsoluteOutputPath -Force | Out-Null

# 4. Invoke UAT to build and package the plugin
Write-Host "`nStarting BuildPlugin compilation and packaging via RunUAT..." -ForegroundColor Cyan
Write-Host "Command: & '$RunUAT' BuildPlugin -plugin='$UpluginPath' -package='$PackagedPluginDir' -Rocket" -ForegroundColor DarkGray

# We use Start-Process to run RunUAT in order to correctly stream the output and handle exit codes
$ProcessParams = @{
    FilePath     = $RunUAT
    ArgumentList = "BuildPlugin -plugin=`"$UpluginPath`" -package=`"$PackagedPluginDir`" -Rocket"
    NoNewWindow  = $true
    Wait         = $true
    PassThru     = $true
}

$Process = Start-Process @ProcessParams
if ($Process.ExitCode -ne 0) {
    Write-Error "BuildPlugin failed with exit code $($Process.ExitCode)"
}

Write-Host "Build and packaging completed successfully!" -ForegroundColor Green

# 5. Compress the packaged output into a ZIP archive
if (-not $NoZip) {
    $ZipPath = Join-Path $AbsoluteOutputPath "Antigravity.zip"
    Write-Host "`nCompressing packaged plugin into ZIP archive..." -ForegroundColor Cyan
    Write-Host "Zip destination: $ZipPath" -ForegroundColor DarkGray
    
    if (Test-Path $ZipPath) {
        Remove-Item -Path $ZipPath -Force
    }

    # Compress-Archive is built-in to PowerShell
    Compress-Archive -Path $PackagedPluginDir -DestinationPath $ZipPath -Force
    Write-Host "ZIP archive created successfully at '$ZipPath'!" -ForegroundColor Green
}

Write-Host "`nWorkflow successfully completed. Packaged output located in '$AbsoluteOutputPath'." -ForegroundColor Green
