<#
.SYNOPSIS
    Undoes everything Sirenproof can change on this PC. For developers and test machines.

.DESCRIPTION
    Use this when a development build has gone wrong, for example a bug that left a block in place.
    It works even while a lock holds, because it's meant for recovering from bugs, not for everyday use.

    In order, it:
      1. stops the Sirenproof service (ending its process if it won't stop) and removes it;
      2. removes Sirenproof's section from the hosts file, after saving a copy next to it;
      3. clears the DNS cache;
      4. puts back any browser policies Sirenproof changed during a lock;
      5. removes the browser extension's native messaging host registration;
      6. with -RemoveData, deletes %ProgramData%\Sirenproof (rules, logs and key).

    Run it from an administrator PowerShell. Add -WhatIf to see what it would do.

.EXAMPLE
    .\tools\dev-reset.ps1 -WhatIf
.EXAMPLE
    .\tools\dev-reset.ps1 -RemoveData
#>
[CmdletBinding(SupportsShouldProcess = $true, ConfirmImpact = 'High')]
param(
    [switch]$RemoveData
)

$ErrorActionPreference = 'Stop'

$principal = New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw 'Run this from an administrator PowerShell.'
}

# 1. The service
$service = Get-Service -Name 'Sirenproof' -ErrorAction SilentlyContinue
if ($service) {
    if ($PSCmdlet.ShouldProcess('Sirenproof service', 'Stop and remove')) {
        & sc.exe stop Sirenproof | Out-Null
        Start-Sleep -Seconds 2
        Get-Process -Name 'sirenproof-service' -ErrorAction SilentlyContinue | Stop-Process -Force
        & sc.exe delete Sirenproof | Out-Null
        Write-Host 'Removed the Sirenproof service.'
    }
} else {
    Write-Host 'The Sirenproof service is not installed.'
}

# 2. The hosts file: only the lines between the markers go.
$hosts = Join-Path $env:SystemRoot 'System32\drivers\etc\hosts'
$text = [IO.File]::ReadAllText($hosts)
$pattern = '(?ms)^# BEGIN Sirenproof[^\r\n]*\r?\n.*?^# END Sirenproof\r?\n?'
if ($text -match $pattern) {
    if ($PSCmdlet.ShouldProcess($hosts, "Remove Sirenproof's section")) {
        Copy-Item -Path $hosts -Destination "$hosts.sirenproof-backup" -Force
        [IO.File]::WriteAllText($hosts, ($text -replace $pattern, ''))
        Write-Host "Removed Sirenproof's section from the hosts file (copy saved as hosts.sirenproof-backup)."
    }
} else {
    Write-Host 'The hosts file has no Sirenproof section.'
}

# 3. DNS cache
if ($PSCmdlet.ShouldProcess('DNS cache', 'Clear')) {
    & ipconfig.exe /flushdns | Out-Null
}

# 4. Browser policies Sirenproof set during locks, put back from its ledger
$ledger = Join-Path $env:ProgramData 'Sirenproof\policies.json'
if (Test-Path $ledger) {
    if ($PSCmdlet.ShouldProcess('browser policies', 'Restore what was there before Sirenproof')) {
        foreach ($entry in (Get-Content $ledger -Raw | ConvertFrom-Json).values) {
            $key = 'HKLM:\' + $entry.key
            if ($null -eq $entry.previous) {
                Remove-ItemProperty -Path $key -Name $entry.name -ErrorAction SilentlyContinue
                continue
            }
            $hex = [string]$entry.previous.data
            $bytes = [byte[]]::new($hex.Length / 2)
            for ($i = 0; $i -lt $bytes.Length; $i++) { $bytes[$i] = [Convert]::ToByte($hex.Substring($i * 2, 2), 16) }
            if ($entry.previous.type -eq 4) {
                Set-ItemProperty -Path $key -Name $entry.name -Type DWord -Value ([BitConverter]::ToInt32($bytes, 0))
            } else {
                Set-ItemProperty -Path $key -Name $entry.name -Type String -Value ([Text.Encoding]::Unicode.GetString($bytes).TrimEnd([char]0))
            }
        }
        Remove-Item -LiteralPath $ledger -Force
        Write-Host 'Restored the browser policies.'
    }
}

# 5. The native messaging host registration, for every browser and both scopes
foreach ($root in 'HKLM:', 'HKCU:') {
    foreach ($browser in 'Google\Chrome', 'Microsoft\Edge', 'BraveSoftware\Brave-Browser') {
        $key = "$root\Software\$browser\NativeMessagingHosts\com.sirenproof.host"
        if ((Test-Path $key) -and $PSCmdlet.ShouldProcess($key, 'Remove')) { Remove-Item -Path $key -Force }
    }
}

# 6. Data
$data = Join-Path $env:ProgramData 'Sirenproof'
if ($RemoveData -and (Test-Path $data)) {
    if ($PSCmdlet.ShouldProcess($data, 'Delete')) {
        Remove-Item -LiteralPath $data -Recurse -Force
        Write-Host "Deleted $data."
    }
}

Write-Host 'Done.'
