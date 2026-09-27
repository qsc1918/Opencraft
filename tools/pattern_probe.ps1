# 枚举原版 BlockPattern 的朝向，验证末地门 5x5 方环：哪些布局能匹配、frontTopLeft 落在哪个角
# 目的：确认「框架朝向指向/背向环心」是否可激活，以及原版 EnderEyeItem 的 offset(-3,0,-3) 落点
# 运行: & 'D:\a\e\build\pattern_probe.ps1'

$aisle = @('?vvv?', '>???<', '>???<', '>???<', '?^^^?')
$dirs = @{
  'NORTH' = @(0,0,-1); 'SOUTH' = @(0,0,1); 'WEST' = @(-1,0,0); 'EAST' = @(1,0,0);
  'UP' = @(0,1,0); 'DOWN' = @(0,-1,0)
}
$opp = @{ DOWN='UP'; UP='DOWN'; NORTH='SOUTH'; SOUTH='NORTH'; WEST='EAST'; EAST='WEST' }
function Cross($a, $b) { return @( $a[1]*$b[2]-$a[2]*$b[1], $a[2]*$b[0]-$a[0]*$b[2], $a[0]*$b[1]-$a[1]*$b[0] ) }
function CharFace($ch) {
  switch ($ch) { 'v' { 'NORTH' } '^' { 'SOUTH' } '>' { 'WEST' } '<' { 'EAST' } default { $null } }
}
# 环: x 3..7, z 8..12, 中心 (5,10), y=3
function BuildRing([string]$kind) {
  $ring = @{}
  $sides = @{ N = @(0,-1); S = @(0,1); W = @(-1,0); E = @(1,0) }
  foreach ($side in @('N','S','W','E')) {
    $o = $sides[$side]
    for ($k = -1; $k -le 1; $k++) {
      if ($side -eq 'N' -or $side -eq 'S') { $x = 5 + $k; $z = 10 + $o[1] } else { $x = 5 + $o[0]; $z = 10 + $k }
      switch ($kind) {
        'allIn'      { switch ($side) { 'N' { $f='SOUTH' } 'S' { $f='NORTH' } 'W' { $f='EAST' } 'E' { $f='WEST' } } }
        'allOut'     { switch ($side) { 'N' { $f='NORTH' } 'S' { $f='SOUTH' } 'W' { $f='WEST' } 'E' { $f='EAST' } } }
        'stronghold' { switch ($side) { 'N' { $f='NORTH' } 'S' { $f='SOUTH' } 'W' { $f='EAST' } 'E' { $f='WEST' } } }
        'pinwheel'   { switch ($side) { 'N' { $f='EAST' } 'S' { $f='WEST' } 'W' { $f='NORTH' } 'E' { $f='SOUTH' } } }
      }
      $ring["$x,$z"] = $f
    }
  }
  return $ring
}
function Test-Orient($ring, $tx, $ty, $tz, $fwdName, $upName) {
  $fwd = $dirs[$fwdName]; $up = $dirs[$upName]
  $right = Cross $fwd $up
  for ($r = 0; $r -lt 5; $r++) {
    for ($d = 0; $d -lt 5; $d++) {
      $ch = $aisle[$d][$r]
      if ($ch -eq '?') { continue }
      $x = $tx + $up[0]*(-$d) + $right[0]*$r
      $y = $ty + $up[1]*(-$d) + $right[1]*$r
      $z = $tz + $up[2]*(-$d) + $right[2]*$r
      if ($y -ne $ty) { return $false }
      $key = "$x,$z"
      if (-not $ring.ContainsKey($key)) { return $false }
      if ($ring[$key] -ne (CharFace $ch)) { return $false }
    }
  }
  return $true
}

foreach ($kind in @('allIn','allOut','stronghold','pinwheel')) {
  $ring = BuildRing $kind
  $matches = @()
  for ($tx = 1; $tx -le 9; $tx++) {
    for ($ty = 1; $ty -le 5; $ty++) {
      for ($tz = 6; $tz -le 14; $tz++) {
        foreach ($f in @('DOWN','UP','NORTH','SOUTH','WEST','EAST')) {
          foreach ($u in @('DOWN','UP','NORTH','SOUTH','WEST','EAST')) {
            if ($u -eq $f -or $u -eq $opp[$f]) { continue }
            if (Test-Orient $ring $tx $ty $tz $f $u) {
              $matches += "origin=($tx,$ty,$tz) fwd=$f up=$u"
            }
          }
        }
      }
    }
  }
  $good = @($matches | Where-Object { $_ -like 'origin=(7,3,12)*' })
  Write-Host "=== $kind ===  匹配总朝向/起点数: $($matches.Count) | origin=(7,3,12)(3x3 落点正确) 数: $($good.Count)"
  $matches | Group-Object { ($_ -split ' ')[0] } | Select-Object -First 8 | ForEach-Object { Write-Host ("   " + $_.Name + "   x" + $_.Count) }
  if ($good.Count -gt 0) { $good | Select-Object -First 3 | ForEach-Object { Write-Host "   GOOD: $_" } }
}
