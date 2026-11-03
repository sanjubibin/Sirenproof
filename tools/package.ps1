<#
.SYNOPSIS
    Builds a release of Sirenproof: the programs with everything they need, the installer, and checksums.

.DESCRIPTION
    In order, it:
      1. builds the `release` preset (or -Preset) and runs its tests (skip with -SkipBuild or -SkipTests);
      2. stages the programs in build\package\stage, with the Qt libraries the main window needs
         (windeployqt6) and the compiler's runtime DLLs they all load, found with objdump;
      3. with -Sign, signs every program and DLL it built (signtool, certificate by -CertThumbprint);
      4. compiles installer\sirenproof.iss with Inno Setup (skip with -NoInstaller), signing the
         installer and uninstaller too with -Sign;
      5. zips the staged extension for the browser stores;
      6. writes SHA256SUMS.txt next to everything it made.

    Everything lands in build\package. Nothing is installed and nothing on this PC changes.

    Needs MSYS2 at C:\msys64 (or -Msys) with the packages from the README, Inno Setup 6.3 or later for
    the installer, and the Windows SDK's signtool.exe for -Sign.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File tools\package.ps1
.EXAMPLE
    powershell -ExecutionPolicy Bypass -File tools\package.ps1 -Sign -CertThumbprint 0123abcd...
#>
[CmdletBinding()]
param(
    [switch]$SkipBuild,
    [switch]$SkipTests,
    [switch]$NoInstaller,
    [switch]$Sign,
    [string]$CertThumbprint = $env:SIRENPROOF_CERT_THUMBPRINT,
    [string]$TimestampUrl = 'http://timestamp.digicert.com',
    [string]$Msys = 'C:\msys64',
    [string]$Preset = 'release'
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$ucrt = Join-Path $Msys 'ucrt64\bin'
$bin = Join-Path $root "build\$Preset\bin"
$out = Join-Path $root 'build\package'
$stage = Join-Path $out 'stage'
$programs = 'sirenproof.exe', 'sirenproof-service.exe', 'sirenproof-host.exe', 'sirenproof-tray.exe', 'sirenproof-ui.exe'

function Step([string]$text) { Write-Host "`n== $text" -ForegroundColor Cyan }

function Find-Tool([string]$name, [string[]]$candidates) {
    $found = Get-Command $name -ErrorAction SilentlyContinue
    if ($found) { return $found.Source }
    foreach ($pattern in $candidates) {
        $hit = Get-ChildItem -Path $pattern -ErrorAction SilentlyContinue | Sort-Object FullName -Descending | Select-Object -First 1
        if ($hit) { return $hit.FullName }
    }
    return $null
}

function Invoke-Checked([string]$exe, [string[]]$arguments) {
    & $exe @arguments
    if ($LASTEXITCODE -ne 0) { throw "$([IO.Path]::GetFileName($exe)) failed with exit code $LASTEXITCODE" }
}

if (-not (Test-Path (Join-Path $ucrt 'g++.exe'))) { throw "MSYS2's UCRT64 tools aren't at $ucrt (use -Msys)." }
$env:PATH = "$ucrt;$env:PATH"

# The version comes from the top-level CMakeLists.txt, so there's one place to change it.
$project = Get-Content (Join-Path $root 'CMakeLists.txt') -Raw
if ($project -notmatch 'project\(Sirenproof\s+VERSION\s+(\d+\.\d+\.\d+)') { throw "Can't find the version in CMakeLists.txt." }
$version = $Matches[1]
Write-Host "Sirenproof $version"

# ---- 1. Build ----
if (-not $SkipBuild) {
    Step "Building the $Preset preset"
    Push-Location $root
    try {
        Invoke-Checked (Join-Path $ucrt 'cmake.exe') @('--preset', $Preset)
        Invoke-Checked (Join-Path $ucrt 'cmake.exe') @('--build', '--preset', $Preset)
        if (-not $SkipTests) { Invoke-Checked (Join-Path $ucrt 'ctest.exe') @('--preset', $Preset) }
    } finally {
        Pop-Location
    }
}
foreach ($program in $programs) {
    if (-not (Test-Path (Join-Path $bin $program))) { throw "$program isn't built (expected in $bin)." }
}

# ---- 2. Stage ----
Step 'Staging the programs'
if (Test-Path $out) { Remove-Item $out -Recurse -Force }
New-Item -ItemType Directory -Force $stage | Out-Null
foreach ($program in $programs) { Copy-Item (Join-Path $bin $program) $stage }

# The extension, for loading by hand until it's in the stores; its tests stay behind.
$extension = Join-Path $stage 'extension'
Copy-Item (Join-Path $root 'extension') $extension -Recurse
Remove-Item (Join-Path $extension 'tests') -Recurse -Force -ErrorAction SilentlyContinue

Copy-Item (Join-Path $root 'LICENSE') (Join-Path $stage 'LICENSE.txt')
Copy-Item (Join-Path $root 'docs\USER-GUIDE.md') (Join-Path $stage 'USER-GUIDE.md')
Copy-Item (Join-Path $root 'CHANGELOG.md') (Join-Path $stage 'CHANGELOG.md')

Step 'Adding the Qt libraries for the main window'
Invoke-Checked (Join-Path $ucrt 'windeployqt6.exe') @(
    '--release', '--no-translations', '--no-system-d3d-compiler', '--no-compiler-runtime',
    '--qmldir', (Join-Path $root 'ui\qml'), '--dir', $stage, (Join-Path $stage 'sirenproof-ui.exe'))

# Every DLL a staged program or library loads that comes from MSYS2 (libstdc++, libgcc, winpthread,
# and what Qt itself needs), until nothing new turns up. Windows' own DLLs aren't in MSYS2's folder.
Step "Adding the runtime DLLs from $ucrt"
$objdump = Join-Path $ucrt 'objdump.exe'
$seen = @{}
do {
    $added = 0
    $modules = Get-ChildItem $stage -Recurse -Include *.exe, *.dll
    foreach ($module in $modules) {
        if ($seen.ContainsKey($module.FullName)) { continue }
        $seen[$module.FullName] = $true
        foreach ($line in (& $objdump -p $module.FullName)) {
            if ($line -notmatch 'DLL Name:\s*(\S+)') { continue }
            $dll = $Matches[1]
            $source = Join-Path $ucrt $dll
            $target = Join-Path $stage $dll
            if ((Test-Path $source) -and -not (Test-Path $target)) {
                Copy-Item $source $target
                $added++
            }
        }
    }
} while ($added -gt 0)
Write-Host "Staged $((Get-ChildItem $stage -Recurse -File).Count) files."

# ---- 3. Sign ----
$signtool = $null
if ($Sign) {
    if (-not $CertThumbprint) { throw 'Signing needs -CertThumbprint (or SIRENPROOF_CERT_THUMBPRINT).' }
    $signtool = Find-Tool 'signtool.exe' @("${env:ProgramFiles(x86)}\Windows Kits\10\bin\*\x64\signtool.exe")
    if (-not $signtool) { throw "Can't find signtool.exe; install the Windows SDK." }
    Step 'Signing the programs'
    # Only what this project built: Qt's and MSYS2's DLLs belong to their makers.
    $ours = $programs | ForEach-Object { Join-Path $stage $_ }
    Invoke-Checked $signtool (@('sign', '/fd', 'SHA256', '/sha1', $CertThumbprint, '/tr', $TimestampUrl, '/td', 'SHA256') + $ours)
}

# ---- 4. Installer ----
$installer = $null
if (-not $NoInstaller) {
    Step 'Compiling the installer'
    $iscc = Find-Tool 'ISCC.exe' @("${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe", "$env:ProgramFiles\Inno Setup 6\ISCC.exe",
                                   "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe")
    if (-not $iscc) { throw "Can't find Inno Setup 6 (ISCC.exe). Install it, or pass -NoInstaller." }
    $arguments = @("/DAppVersion=$version", "/DStageDir=$stage", "/DOutputDir=$out")
    if ($Sign) {
        # Inno Setup's own escapes: $q is a quote (signtool's path has spaces), $f the file to sign.
        $arguments += "/Ssirenproof=`$q$signtool`$q sign /fd SHA256 /sha1 $CertThumbprint /tr $TimestampUrl /td SHA256 `$f"
        $arguments += '/DSignTool=sirenproof'
    }
    Invoke-Checked $iscc ($arguments + (Join-Path $root 'installer\sirenproof.iss'))
    $installer = Join-Path $out "Sirenproof-$version-setup.exe"
}

# ---- 5. The extension for the stores ----
Step 'Zipping the extension for the Chrome Web Store and Edge Add-ons'
$extensionZip = Join-Path $out "Sirenproof-extension-$version.zip"
Compress-Archive -Path (Join-Path $extension '*') -DestinationPath $extensionZip

# ---- 6. Checksums ----
Step 'Writing checksums'
$made = @($extensionZip)
if ($installer) { $made = @($installer) + $made }
$lines = $made | ForEach-Object {
    $hash = (Get-FileHash $_ -Algorithm SHA256).Hash.ToLowerInvariant()
    "$hash  $([IO.Path]::GetFileName($_))"
}
$sums = Join-Path $out 'SHA256SUMS.txt'
[IO.File]::WriteAllText($sums, (($lines -join "`n") + "`n"))
Get-Content $sums

Write-Host "`nDone. Everything is in $out" -ForegroundColor Green
