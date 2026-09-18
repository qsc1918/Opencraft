# pack_7z.ps1 - 用官方 7-Zip SFX 模块打包 Opencraft 一键安装包
# 用法: powershell -File tools\pack_7z.ps1 [-BuildDir build] [-OutDir releases] [-Optional]
# 产物: releases\Opencraft-Installer.exe = [7z.sfx][配置][7z 压缩包]
# 注意: 不打包 assets/（遵守 Mojang EULA），玩家用 extract_assets.exe 自己提取贴图
#       -Optional 供自动打包使用：缺 7-Zip 或构建产物时只警告，不让构建失败

param(
    [string]$BuildDir = '',
    [string]$OutDir = '',
    [switch]$Optional
)

$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
if ($BuildDir -eq '') { $BuildDir = Join-Path $root 'build' }
if ($OutDir -eq '') { $OutDir = Join-Path $root 'releases' }
$build = $BuildDir
$sevenZip = Join-Path $root '7-Zip'
$sevenZipExe = Join-Path $sevenZip '7z.exe'
$sfxModule = Join-Path $sevenZip '7z.sfx'
$config = Join-Path $root 'tools\sfx_config_7z.txt'

# 取一个必需文件；-Optional 时只警告并正常退出
function Require-File([string]$path, [string]$hint) {
    if (Test-Path $path) { return $path }
    $msg = "缺少 {0}：{1}" -f $path, $hint
    if ($Optional) { Write-Warning $msg; Write-Host "跳过打包。"; exit 0 }
    throw $msg
}

$null = Require-File $sfxModule '把 7-Zip 解压到仓库根目录（需要 7z.sfx 和 7z.exe）'
$null = Require-File $sevenZipExe '把 7-Zip 解压到仓库根目录'
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

# 资源提取工具：优先 build/，兼容放在仓库根目录的旧产物
$extract = Join-Path $build 'extract_assets.exe'
if (!(Test-Path $extract)) { $extract = Join-Path $root 'extract_assets.exe' }

# 1) 准备载荷到 build\sfxstage\Opencraft\
$stage = Join-Path $build 'sfxstage'
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
$appDir = Join-Path $stage 'Opencraft'
New-Item -ItemType Directory -Force -Path (Join-Path $appDir 'shaders') | Out-Null
Copy-Item (Require-File (Join-Path $build 'opencraft.exe') '先执行 cmake --build build') (Join-Path $appDir 'opencraft.exe') -Force
Copy-Item (Require-File $extract '先执行 cmake --build build（会一并构建 extract_assets.exe）') (Join-Path $appDir 'extract_assets.exe') -Force
Copy-Item (Join-Path $root 'README-INSTALL.txt') (Join-Path $appDir 'README-INSTALL.txt') -Force
Copy-Item (Join-Path $root 'version.txt') (Join-Path $appDir 'version.txt') -Force
Get-ChildItem (Join-Path $build 'shaders') -Filter '*.spv' | ForEach-Object {
    Copy-Item $_.FullName (Join-Path $appDir "shaders\$($_.Name)") -Force
}

# 2) 打 7z 压缩包（带 Opencraft\ 前缀，解压后自动落进子目录）
$archive = Join-Path $stage 'payload.7z'
Push-Location $stage
try {
    & $sevenZipExe a -t7z -mx=9 $archive 'Opencraft' | Out-Null
} finally {
    Pop-Location
}
if ($LASTEXITCODE -ne 0) { throw "7z archive failed" }

# 3) 拼接 7z.sfx + 配置 + 压缩包 -> Opencraft-Installer.exe
$out = Join-Path $OutDir 'Opencraft-Installer.exe'
$fs = [System.IO.File]::Create($out)
foreach ($p in @($sfxModule, $config, $archive)) {
    $b = [System.IO.File]::ReadAllBytes($p)
    $fs.Write($b, 0, $b.Length)
}
$fs.Close()

$mb = [math]::Round((Get-Item $out).Length / 1MB, 1)
Write-Host ("OK: {0}  ({1} MB)" -f $out, $mb)
