param(
    [Parameter(Mandatory = $true)]
    [string]$IsoPath
)

$ErrorActionPreference = 'Stop'

$resolvedIsoPath = [System.IO.Path]::GetFullPath($IsoPath)
if (-not (Test-Path -LiteralPath $resolvedIsoPath)) {
    throw "ISO was not found: $resolvedIsoPath"
}

$dolphinPath = 'C:\dev\helworks\emus\dolphin-2603a-x64\Dolphin-x64\Dolphin.exe'
$globalProfileRoot = 'C:\Users\Helena\AppData\Roaming\Dolphin Emulator'
$userDir = 'C:\dev\helworks\helengine-wii\tmp\dolphin-launcher-user'
$sdSyncFolderPath = Join-Path $userDir 'Wii\sd-sync'

if (-not (Test-Path -LiteralPath $dolphinPath)) {
    throw "Dolphin executable was not found: $dolphinPath"
}

if (-not (Test-Path -LiteralPath $globalProfileRoot)) {
    throw "Dolphin profile root was not found: $globalProfileRoot"
}

$globalQtPath = Join-Path $globalProfileRoot 'Config\Qt.ini'
if (-not (Test-Path -LiteralPath $globalQtPath)) {
    throw "Dolphin Qt.ini was not found: $globalQtPath"
}

$globalLoggerPath = Join-Path $globalProfileRoot 'Config\Logger.ini'
if (-not (Test-Path -LiteralPath $globalLoggerPath)) {
    throw "Dolphin Logger.ini was not found: $globalLoggerPath"
}

$existingDolphinProcesses = @(Get-Process -Name 'Dolphin' -ErrorAction SilentlyContinue)
foreach ($process in $existingDolphinProcesses) {
    Stop-Process -Id $process.Id -Force
}

if (Test-Path -LiteralPath $userDir) {
    Remove-Item -LiteralPath $userDir -Recurse -Force
}

New-Item -ItemType Directory -Force -Path $userDir | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $userDir 'Config') | Out-Null
New-Item -ItemType Directory -Force -Path $sdSyncFolderPath | Out-Null

foreach ($directoryName in @('Wii', 'Backup', 'ResourcePacks')) {
    $sourceDirectoryPath = Join-Path $globalProfileRoot $directoryName
    if (Test-Path -LiteralPath $sourceDirectoryPath) {
        Copy-Item -LiteralPath $sourceDirectoryPath -Destination (Join-Path $userDir $directoryName) -Recurse -Force
    }
}

Set-Content -LiteralPath (Join-Path $userDir 'Config\Dolphin.ini') -Value @(
    '[Analytics]'
    'Enabled = False'
    'PermissionAsked = True'
    '[General]'
    'UseDiscordPresence = False'
    ('WiiSDCardSyncFolder = ' + $sdSyncFolderPath.Replace('\', '/'))
    '[Core]'
    'WiiSDCard = True'
    'WiiSDCardAllowWrites = True'
    'WiiSDCardEnableFolderSync = True'
) -Encoding ASCII

$loggerSource = Get-Content -LiteralPath $globalLoggerPath -Raw
$loggerOptionsSection = @'
[Options]
WriteToConsole = True
WriteToFile = True
WriteToWindow = True
Verbosity = 1
'@
if ($loggerSource -match '(?ms)^\[Options\].*?(?=^\[|\z)') {
    $loggerSource = [System.Text.RegularExpressions.Regex]::Replace($loggerSource, '(?ms)^\[Options\].*?(?=^\[|\z)', $loggerOptionsSection + [Environment]::NewLine)
} else {
    $loggerSource = $loggerSource.TrimEnd() + [Environment]::NewLine + [Environment]::NewLine + $loggerOptionsSection + [Environment]::NewLine
}

Set-Content -LiteralPath (Join-Path $userDir 'Config\Logger.ini') -Value $loggerSource -Encoding ASCII

$qtSource = Get-Content -LiteralPath $globalQtPath -Raw
$loggingSection = @'
[logging]
wraplines=false
font=0
logvisible=true
logconfigvisible=true
'@
if ($qtSource -match '(?ms)^\[logging\].*?(?=^\[|\z)') {
    $qtSource = [System.Text.RegularExpressions.Regex]::Replace($qtSource, '(?ms)^\[logging\].*?(?=^\[|\z)', $loggingSection + [Environment]::NewLine)
} else {
    $qtSource = $qtSource.TrimEnd() + [Environment]::NewLine + [Environment]::NewLine + $loggingSection + [Environment]::NewLine
}

Set-Content -LiteralPath (Join-Path $userDir 'Config\Qt.ini') -Value $qtSource -Encoding UTF8

$isoItem = Get-Item -LiteralPath $resolvedIsoPath

Write-Output ("ISO=" + $resolvedIsoPath)
Write-Output ("ISO_LAST_WRITE_TIME=" + $isoItem.LastWriteTime.ToString('O'))
Write-Output ("DOLPHIN=" + $dolphinPath)
Write-Output ("USER_DIR=" + $userDir)
Write-Output ("SD_SYNC_FOLDER=" + $sdSyncFolderPath)
Write-Output ("LOGGER_CONFIG=" + (Join-Path $userDir 'Config\Logger.ini'))
Write-Output ("LOG_WINDOW=enabled")

$process = Start-Process -FilePath $dolphinPath -ArgumentList '-u', $userDir, '-e', $resolvedIsoPath -PassThru
Write-Output ("PROCESS_ID=" + $process.Id)
