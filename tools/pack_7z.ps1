# pack_7z.ps1 - VoxMine one-click self-extractor using the official 7-Zip SFX module.
# Usage: powershell -File tools\pack_7z.ps1
# Output: releases\VoxMine-Installer.exe = [7z.sfx][config][7z archive]
#
# NOTE: assets/ is NOT packed (Mojang EULA). Players run extract_assets.exe
# (auto-launched by the SFX) to pull textures from their own minecraft.jar.
#
# The 7-Zip SFX module is taken from the bundled 7-Zip\ folder (7z.sfx).
# Config (tools\sfx_config_7z.txt) auto-runs VoxMine\extract_assets.exe after
# extraction.

$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$build = Join-Path $root 'build'
$outDir = Join-Path $root 'releases'
$sevenZip = Join-Path $root '7-Zip'
$sfxModule = Join-Path $sevenZip '7z.sfx'
$config = Join-Path $root 'tools\sfx_config_7z.txt'

if (!(Test-Path $sfxModule)) { throw "7z.sfx not found at $sfxModule" }
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

# 1) Stage payload into build\sfxstage\VoxMine\
$stage = Join-Path $build 'sfxstage'
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
$appDir = Join-Path $stage 'VoxMine'
New-Item -ItemType Directory -Force -Path (Join-Path $appDir 'shaders') | Out-Null
Copy-Item (Join-Path $build 'voxmine.exe') (Join-Path $appDir 'voxmine.exe') -Force
Copy-Item (Join-Path $root 'extract_assets.exe') (Join-Path $appDir 'extract_assets.exe') -Force
Copy-Item (Join-Path $root 'README-INSTALL.txt') (Join-Path $appDir 'README-INSTALL.txt') -Force
Get-ChildItem (Join-Path $build 'shaders') -Filter '*.spv' | ForEach-Object {
    Copy-Item $_.FullName (Join-Path $appDir "shaders\$($_.Name)") -Force
}

# 2) Create the 7z archive (path prefix VoxMine\ so extraction lands in a folder).
$archive = Join-Path $stage 'payload.7z'
Push-Location $stage
try {
    & (Join-Path $sevenZip '7z.exe') a -t7z -mx=9 $archive 'VoxMine' | Out-Null
} finally {
    Pop-Location
}
if ($LASTEXITCODE -ne 0) { throw "7z archive failed" }

# 3) Concatenate 7z.sfx + config + archive -> VoxMine-Installer.exe
$out = Join-Path $outDir 'VoxMine-Installer.exe'
$fs = [System.IO.File]::Create($out)
foreach ($p in @($sfxModule, $config, $archive)) {
    $b = [System.IO.File]::ReadAllBytes($p)
    $fs.Write($b, 0, $b.Length)
}
$fs.Close()

$mb = [math]::Round((Get-Item $out).Length / 1MB, 1)
Write-Host ("OK: {0}  ({1} MB)" -f $out, $mb)
