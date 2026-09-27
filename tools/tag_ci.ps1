# ---------------------------------------------------------------------------
# 自动打 CI 标签：读 version.txt 的 [current] 版本号，前面加 ci 作为 tag，
# 打在当前提交上并推送到远端。
# 用法：
#   tools\tag_ci.ps1              # 打 tag 并推送
#   tools\tag_ci.ps1 -DryRun      # 只显示将要执行的动作，不实际执行
#   tools\tag_ci.ps1 -Force       # 工作区有未提交修改时仍强行打 tag
# 注意：tag 打在最后一次提交（HEAD）上，未提交的修改不会包含在内。
# ---------------------------------------------------------------------------
param(
    [string]$Remote = "origin",
    [switch]$DryRun,
    [switch]$Force
)

$ErrorActionPreference = "Stop"

# ---- 读版本号 ----
$versionFile = Join-Path $PSScriptRoot "..\version.txt"
$version = $null
foreach ($line in Get-Content -LiteralPath $versionFile) {
    if ($line -match '^\s*\[current\]\s*(\S+)\s*$') { $version = $Matches[1]; break }
}
if (-not $version) {
    Write-Error "version.txt 里没有找到 [current] 版本号"
    exit 1
}

$tag = "ci$version"
$head = (git rev-parse HEAD).Trim()
$branch = (git rev-parse --abbrev-ref HEAD).Trim()

Write-Host "版本号 : $version"
Write-Host "标签   : $tag"
Write-Host "提交   : $(git log -1 --format='%h %s' HEAD)"
Write-Host "分支   : $branch"

# ---- 工作区不干净时提醒：tag 只包含已提交内容 ----
$dirty = @(git status --porcelain)
if ($dirty.Count -gt 0) {
    Write-Warning "工作区有 $($dirty.Count) 处未提交的修改，标签只打在最后一次提交上："
    $dirty | Select-Object -First 10 | ForEach-Object { Write-Host "  $_" }
    if (-not $Force) {
        Write-Error "请先提交这些修改，或加 -Force 仍要继续"
        exit 1
    }
}

# ---- 打标签（已存在且指向同一提交时只补推送）----
$existing = @(git tag -l $tag)
if ($existing.Count -gt 0) {
    $existingSha = (git rev-list -n 1 $tag).Trim()
    if ($existingSha -eq $head) {
        Write-Host "标签已打在当前提交上，只做推送"
    } else {
        Write-Error "标签 $tag 已存在但指向 $existingSha，不是当前提交 $head。请先在 version.txt 里升版本号。"
        exit 1
    }
} else {
    if ($DryRun) {
        Write-Host "[DryRun] git tag $tag"
    } else {
        git tag $tag
        if ($LASTEXITCODE -ne 0) { exit 1 }
        Write-Host "已打标签 $tag"
    }
}

# ---- 推送 ----
if ($DryRun) {
    Write-Host "[DryRun] git push $Remote $tag"
} else {
    git push $Remote $tag
    if ($LASTEXITCODE -ne 0) { exit 1 }
    Write-Host "完成：$tag 已推送到 $Remote"
}
