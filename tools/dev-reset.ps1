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
      4. with -RemoveData, deletes %ProgramData%\Sirenproof (rules, logs and key).

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

# 4. Data
$data = Join-Path $env:ProgramData 'Sirenproof'
if ($RemoveData -and (Test-Path $data)) {
    if ($PSCmdlet.ShouldProcess($data, 'Delete')) {
        Remove-Item -LiteralPath $data -Recurse -Force
        Write-Host "Deleted $data."
    }
}

Write-Host 'Done.'
