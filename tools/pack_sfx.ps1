# pack_sfx.ps1 - Build the VoxMine one-click self-extractor (SFX).
# Usage: powershell -File tools\pack_sfx.ps1
# Output: releases\VoxMine-SFX.exe = [sfxstub.exe][file blobs...][file table][u64 table offset]
# NOTE: assets/ is NOT packed (Mojang EULA). Players extract textures with extract_assets.exe.

$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$build = Join-Path $root 'build'
$outDir = Join-Path $root 'releases'
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

# 1) Compile the stub (w64devkit gcc)
$stub = Join-Path $build 'sfxstub.exe'
gcc -O2 -s -mwindows -o $stub (Join-Path $root 'tools\sfxstub.c') -luser32
if ($LASTEXITCODE -ne 0) { throw "gcc stub build failed" }

# 2) Collect payload files (internal SFX paths use '/')
$files = @(
    @{ src = Join-Path $build 'voxmine.exe';        name = 'voxmine.exe' },
    @{ src = Join-Path $root  'extract_assets.exe'; name = 'extract_assets.exe' },
    @{ src = Join-Path $root  'README-INSTALL.txt'; name = 'README-INSTALL.txt' }
)
Get-ChildItem (Join-Path $build 'shaders') -File -Filter '*.spv' | ForEach-Object {
    $files += @{ src = $_.FullName; name = "shaders/$($_.Name)" }
}

# 3) Assemble payload: blobs first, then table, then u64 table offset
$stubBytes = [System.IO.File]::ReadAllBytes($stub)
$ms = New-Object System.IO.MemoryStream
$bw = New-Object System.IO.BinaryWriter($ms)
$off = [uint64]$stubBytes.Length
$entries = @()
foreach ($f in $files) {
    $bytes = [System.IO.File]::ReadAllBytes($f.src)
    $entries += @{ name = [System.Text.Encoding]::UTF8.GetBytes($f.name); size = [uint64]$bytes.Length; off = $off }
    $bw.Write($bytes)
    $off += [uint64]$bytes.Length
}
$tableStart = [uint64]$stubBytes.Length + [uint64]$ms.Position  # absolute offset in SFX file
$bw.Write([uint32]0x58534D56)  # 'VMSX'
$bw.Write([uint32]$entries.Count)
foreach ($e in $entries) {
    $bw.Write([uint32]$e.name.Length)
    $bw.Write([uint64]$e.size)
    $bw.Write([uint64]$e.off)
    $bw.Write($e.name)
}
$bw.Write([uint64]$tableStart)
$bw.Flush()

# 4) Write the SFX: stub + payload
$out = Join-Path $outDir 'VoxMine-SFX.exe'
$fs = [System.IO.File]::Create($out)
$fs.Write($stubBytes, 0, $stubBytes.Length)
$ms.Position = 0; $ms.CopyTo($fs)
$fs.Close(); $bw.Close()

$mb = [math]::Round((Get-Item $out).Length / 1MB, 1)
Write-Host ("OK: {0}  ({1} files, {2} MB)" -f $out, $entries.Count, $mb)
foreach ($e in $entries) { Write-Host ("  - {0}  ({1} KB)" -f [System.Text.Encoding]::UTF8.GetString($e.name), [math]::Round($e.size / 1KB)) }
