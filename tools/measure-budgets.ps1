<#
.SYNOPSIS
    Measures the interface budgets in docs/PLAN.md section 3.7 and prints a pass/fail table.

.DESCRIPTION
    Runs against a throwaway console service (dry run, its own data folder, a stand-in hosts file and a
    private pipe), so it never touches the real hosts file, browsers or an installed service. Nothing
    pops up: the window and the pause are measured invisible and click-through. The tray icon shows
    for the length of the idle measurement.

    Close Sirenproof's own tray first if it's running: only one tray runs per user.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File tools\measure-budgets.ps1
    powershell -ExecutionPolicy Bypass -File tools\measure-budgets.ps1 -Preset debug -IdleSeconds 30
#>
param(
    [string]$Preset = "release",
    [int]$IdleSeconds = 60,
    [string]$QtBin = "C:\msys64\ucrt64\bin"
)

$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot
$bin = Join-Path $repo "build\$Preset\bin"
foreach ($exe in "sirenproof-service.exe", "sirenproof.exe", "sirenproof-ui.exe", "sirenproof-tray.exe", "sleeper.exe") {
    if (-not (Test-Path (Join-Path $bin $exe))) { throw "$exe is missing from $bin; build the '$Preset' preset first" }
}
$env:PATH = "$QtBin;$env:PATH"

$work = Join-Path $env:TEMP ("sirenproof-budgets-" + [guid]::NewGuid().ToString("N").Substring(0, 8))
New-Item -ItemType Directory -Path $work | Out-Null
Set-Content -Path "$work\hosts" -Value "127.0.0.1 localhost" -Encoding ascii
$pipe = "\\.\pipe\SirenproofBudgets-$PID"
$results = New-Object System.Collections.Generic.List[object]

function Add-Result($surface, $budget, $measured, $pass) {
    $results.Add([pscustomobject]@{ Surface = $surface; Budget = $budget; Measured = $measured; Result = $(if ($pass) { "pass" } else { "MISS" }) })
}
function Siren { & "$bin\sirenproof.exe" --pipe $pipe @args | Out-Null }
function Read-Json($file) { if (Test-Path $file) { Get-Content $file -Raw | ConvertFrom-Json } else { $null } }
function Run-Window($file, $extra) {
    $p = Start-Process "$bin\sirenproof-ui.exe" -ArgumentList "--pipe $pipe --measure `"$file`" $extra" -PassThru
    if (-not $p.WaitForExit(20000)) { Stop-Process -Id $p.Id -Force }
    Read-Json $file
}
function Median($values) {
    $sorted = @($values | Sort-Object)
    if ($sorted.Count -eq 0) { return $null }
    $sorted[[math]::Floor(($sorted.Count - 1) / 2)]
}

$service = Start-Process "$bin\sirenproof-service.exe" -PassThru -WindowStyle Hidden `
    -ArgumentList "console --data-dir `"$work\data`" --hosts-file `"$work\hosts`" --pipe $pipe --dry-run" `
    -RedirectStandardError "$work\service.log" -RedirectStandardOutput "$work\service.out"
$sleeper = $null
try {
    Start-Sleep -Milliseconds 1500

    # --- Main window: start-up, and drawing only when something changes ---
    Write-Host "Main window: 5 starts..."
    $runs = @(1..5 | ForEach-Object { Run-Window "$work\window$_.json" "" })
    $first = $runs[0]
    $warm = Median (@($runs | Select-Object -Skip 1 | Where-Object { $_ -and $_.first_frame_ms -ge 0 } | ForEach-Object { $_.first_frame_ms }))
    if ($null -eq $warm) {
        Add-Result "Main window" "start <= 400 ms" "no frame drawn" $false
    } else {
        Add-Result "Main window" "start <= 400 ms" ("{0:N0} ms warm (median of 4); first start {1:N0} ms" -f $warm, $first.first_frame_ms) ($warm -le 400)
        $idle = Median (@($runs | Where-Object { $_ } | ForEach-Object { $_.fps }))
        Add-Result "Main window" "draws only on change" ("{0:N1} frames/s while idle ({1})" -f $idle, $first.graphics) ($idle -le 2)
    }

    # --- Main window: animation, with a lock holding (the ring breathes) ---
    Siren block example.com --for 10m --lock
    Write-Host "Main window: animating..."
    $anim = Run-Window "$work\animating.json" "--screen now"
    if ($anim -and $anim.fps) {
        Add-Result "Main window" "60 fps animating" ("{0:N0} fps, longest frame {1:N1} ms" -f $anim.fps, $anim.worst_gap_ms) (($anim.fps -ge 55) -and ($anim.worst_gap_ms -le 34))
    } else {
        Add-Result "Main window" "60 fps animating" "no result" $false
    }

    # --- Tray: idle cost ---
    Write-Host "Tray: idle for $IdleSeconds s..."
    $tray = Start-Process "$bin\sirenproof-tray.exe" -ArgumentList "--pipe $pipe --measure-idle `"$work\tray.json`" --seconds $IdleSeconds" -PassThru
    if (-not $tray.WaitForExit(($IdleSeconds + 20) * 1000)) { Stop-Process -Id $tray.Id -Force }
    $t = Read-Json "$work\tray.json"
    if ($t) {
        Add-Result "Tray" "<= 3 MB" ("{0:N2} MB private ({1:N1} MB working set)" -f $t.private_mb, $t.working_set_mb) ($t.private_mb -le 3)
        Add-Result "Tray" "0% CPU idle" ("{0:N0} ms CPU in {1} s ({2:P3})" -f $t.cpu_ms, $t.seconds, ($t.cpu_ms / ($t.seconds * 1000))) (($t.cpu_ms / ($t.seconds * 1000)) -lt 0.001)
    } else {
        Add-Result "Tray" "<= 3 MB, 0% CPU idle" "no result (is Sirenproof's tray already running?)" $false
    }

    # --- Pause: from the program being caught to the pause on screen ---
    Siren block "app:$bin\sleeper.exe" --nudge --name "Budget check"
    Write-Host "Pause: catching a program..."
    $tray = Start-Process "$bin\sirenproof-tray.exe" -ArgumentList "--pipe $pipe --measure-pause `"$work\pause.json`"" -PassThru
    Start-Sleep -Seconds 2
    $sleeper = Start-Process "$bin\sleeper.exe" -PassThru -WindowStyle Hidden
    $started = [DateTimeOffset]::new($sleeper.StartTime).ToUnixTimeMilliseconds()
    if (-not $tray.WaitForExit(15000)) { Stop-Process -Id $tray.Id -Force }
    $p = Read-Json "$work\pause.json"
    if ($p) {
        Add-Result "Pause" "<= 100 ms after caught" ("{0:N0} ms; drawing surface ready at {1:N0} ms; caught {2:N0} ms after the program started" -f $p.changed_to_frame_ms, $p.steps.render_target, ($p.changed_at_ms - $started)) ($p.changed_to_frame_ms -le 100)
    } else {
        Add-Result "Pause" "<= 100 ms after caught" "no result" $false
    }

    # --- Block page: size, fonts, and only compositor-friendly animation ---
    $ext = Join-Path $repo "extension"
    $files = "block.html", "block.css", "block.js" | ForEach-Object { Join-Path $ext $_ }
    $bytes = ($files | ForEach-Object { (Get-Item $_).Length } | Measure-Object -Sum).Sum
    Add-Result "Block page" "<= 50 KB" ("{0:N1} KB" -f ($bytes / 1KB)) ($bytes -le 50KB)
    $text = ($files | ForEach-Object { Get-Content $_ -Raw }) -join "`n"
    $fonts = [regex]::Matches($text, "@font-face|@import|url\(|fonts\.googleapis").Count
    Add-Result "Block page" "no downloaded fonts" ("{0} font or remote references" -f $fonts) ($fonts -eq 0)
    $css = Get-Content (Join-Path $ext "block.css") -Raw
    $animated = @([regex]::Matches($css, "transition:\s*([^;]+);") | ForEach-Object {
        $_.Groups[1].Value -split "," | ForEach-Object { ($_.Trim() -split "\s+")[0] } })
    $animated += @([regex]::Matches($css, "@keyframes[^{]+\{(.*?\})\s*\}", "Singleline") | ForEach-Object {
        [regex]::Matches($_.Groups[1].Value, "([a-z-]+)\s*:") | ForEach-Object { $_.Groups[1].Value } })
    $others = @($animated | Where-Object { $_ -notin "transform", "opacity", "none" } | Select-Object -Unique)
    Add-Result "Block page" "animates transform/opacity only" $(if ($others.Count) { "also: " + ($others -join ", ") } else { (($animated | Select-Object -Unique) -join ", ") }) ($others.Count -eq 0)
} finally {
    if ($sleeper -and -not $sleeper.HasExited) { Stop-Process -Id $sleeper.Id -Force }
    Get-Process sirenproof-ui, sirenproof-tray -ErrorAction SilentlyContinue |
        Where-Object { $_.Path -like "$bin*" } | Stop-Process -Force
    Stop-Process -Id $service.Id -Force -ErrorAction SilentlyContinue
    Start-Sleep -Milliseconds 300
    Remove-Item -Recurse -Force $work -ErrorAction SilentlyContinue
}

$results | Format-Table -AutoSize -Wrap
if ($results | Where-Object Result -eq "MISS") { exit 1 }
