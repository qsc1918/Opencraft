# pack_sfx.ps1 - 构建 Opencraft 自解压安装包（自带 SFX stub）
# 用法: powershell -File tools\pack_sfx.ps1
# 产物: releases\Opencraft-SFX.exe = [sfxstub.exe][文件数据...][文件表][u64 表偏移]
# 注意: 不打包 assets/（遵守 Mojang EULA），玩家用 extract_assets.exe 自己提取贴图

$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$build = Join-Path $root 'build'
$outDir = Join-Path $root 'releases'
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

# 1) 编译 stub（w64devkit gcc）
$stub = Join-Path $build 'sfxstub.exe'
gcc -O2 -s -mwindows -o $stub (Join-Path $root 'tools\sfxstub.c') -luser32
if ($LASTEXITCODE -ne 0) { throw "gcc stub build failed" }

# 2) 收集载荷文件（包内路径用 '/'）
$files = @(
    @{ src = Join-Path $build 'opencraft.exe';      name = 'opencraft.exe' },
    @{ src = Join-Path $root  'extract_assets.exe'; name = 'extract_assets.exe' },
    @{ src = Join-Path $root  'README-INSTALL.txt'; name = 'README-INSTALL.txt' },
    @{ src = Join-Path $root  'version.txt';        name = 'version.txt' }
)
Get-ChildItem (Join-Path $build 'shaders') -File -Filter '*.spv' | ForEach-Object {
    $files += @{ src = $_.FullName; name = "shaders/$($_.Name)" }
}

# 3) 组装载荷：数据块 + 文件表 + u64 表偏移
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
$tableStart = [uint64]$stubBytes.Length + [uint64]$ms.Position  # 在 SFX 文件中的绝对偏移
$bw.Write([uint32]0x5853434F)  # 'OCSX'
$bw.Write([uint32]$entries.Count)
foreach ($e in $entries) {
    $bw.Write([uint32]$e.name.Length)
    $bw.Write([uint64]$e.size)
    $bw.Write([uint64]$e.off)
    $bw.Write($e.name)
}
$bw.Write([uint64]$tableStart)
$bw.Flush()

# 4) 写出 SFX：stub + 载荷
$out = Join-Path $outDir 'Opencraft-SFX.exe'
$fs = [System.IO.File]::Create($out)
$fs.Write($stubBytes, 0, $stubBytes.Length)
$ms.Position = 0; $ms.CopyTo($fs)
$fs.Close(); $bw.Close()

$mb = [math]::Round((Get-Item $out).Length / 1MB, 1)
Write-Host ("OK: {0}  ({1} files, {2} MB)" -f $out, $entries.Count, $mb)
foreach ($e in $entries) { Write-Host ("  - {0}  ({1} KB)" -f [System.Text.Encoding]::UTF8.GetString($e.name), [math]::Round($e.size / 1KB)) }
