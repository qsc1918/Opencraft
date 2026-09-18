# pack_7z.ps1 - 用官方 7-Zip SFX 模块打包 Opencraft 一键安装包
# 用法: powershell -File tools\pack_7z.ps1
# 产物: releases\Opencraft-Installer.exe = [7z.sfx][配置][7z 压缩包]
# 注意: 不打包 assets/（遵守 Mojang EULA），玩家用 extract_assets.exe 自己提取贴图

$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$build = Join-Path $root 'build'
$outDir = Join-Path $root 'releases'
$sevenZip = Join-Path $root '7-Zip'
$sfxModule = Join-Path $sevenZip '7z.sfx'
$config = Join-Path $root 'tools\sfx_config_7z.txt'

if (!(Test-Path $sfxModule)) { throw "7z.sfx not found at $sfxModule" }
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

# 1) 准备载荷到 build\sfxstage\Opencraft\
$stage = Join-Path $build 'sfxstage'
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
$appDir = Join-Path $stage 'Opencraft'
New-Item -ItemType Directory -Force -Path (Join-Path $appDir 'shaders') | Out-Null
Copy-Item (Join-Path $build 'opencraft.exe') (Join-Path $appDir 'opencraft.exe') -Force
Copy-Item (Join-Path $root 'extract_assets.exe') (Join-Path $appDir 'extract_assets.exe') -Force
Copy-Item (Join-Path $root 'README-INSTALL.txt') (Join-Path $appDir 'README-INSTALL.txt') -Force
Copy-Item (Join-Path $root 'version.txt') (Join-Path $appDir 'version.txt') -Force
Get-ChildItem (Join-Path $build 'shaders') -Filter '*.spv' | ForEach-Object {
    Copy-Item $_.FullName (Join-Path $appDir "shaders\$($_.Name)") -Force
}

# 2) 打 7z 压缩包（带 Opencraft\ 前缀，解压后自动落进子目录）
$archive = Join-Path $stage 'payload.7z'
Push-Location $stage
try {
    & (Join-Path $sevenZip '7z.exe') a -t7z -mx=9 $archive 'Opencraft' | Out-Null
} finally {
    Pop-Location
}
if ($LASTEXITCODE -ne 0) { throw "7z archive failed" }

# 3) 拼接 7z.sfx + 配置 + 压缩包 -> Opencraft-Installer.exe
$out = Join-Path $outDir 'Opencraft-Installer.exe'
$fs = [System.IO.File]::Create($out)
foreach ($p in @($sfxModule, $config, $archive)) {
    $b = [System.IO.File]::ReadAllBytes($p)
    $fs.Write($b, 0, $b.Length)
}
$fs.Close()

$mb = [math]::Round((Get-Item $out).Length / 1MB, 1)
Write-Host ("OK: {0}  ({1} MB)" -f $out, $mb)
