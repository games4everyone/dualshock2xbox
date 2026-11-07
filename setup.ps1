# dualshock2xbox setup ; run this once, as Administrator, in PowerShell.
# it installs the ViGEmBus driver, which is what lets us create the fake
# Xbox 360 controller that games see.

Write-Host "dualshock2xbox setup" -ForegroundColor Cyan
Write-Host ""

# installing a driver needs Administrator ; say so up front instead of letting winget
# fail with an error nobody can decode
$principal = New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Host "[!!] this needs Administrator. right-click PowerShell > Run as administrator," -ForegroundColor Yellow
    Write-Host "     then run setup.ps1 again."
    exit 1
}

# work from the folder this script lives in, whatever PowerShell was opened in
Set-Location -Path $PSScriptRoot

# 1. ViGEmBus driver
$installed = Get-ItemProperty 'HKLM:\SYSTEM\CurrentControlSet\Services\ViGEmBus' -ErrorAction SilentlyContinue
if ($installed) {
    Write-Host "[ok] ViGEmBus driver already installed"
} else {
    if (-not (Get-Command winget -ErrorAction SilentlyContinue)) {
        Write-Host "[!!] winget isn't available on this Windows ; download the driver manually from" -ForegroundColor Yellow
        Write-Host "     https://github.com/ViGEm/ViGEmBus/releases"
        exit 1
    }
    Write-Host "installing ViGEmBus driver via winget..."
    winget install --id ViGEm.ViGEmBus -e --accept-source-agreements --accept-package-agreements
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[!!] winget install failed ; download the driver manually from" -ForegroundColor Yellow
        Write-Host "     https://github.com/ViGEm/ViGEmBus/releases"
        exit 1
    }
    Write-Host "[ok] ViGEmBus driver installed"
}

# 2. the translator itself
$exe = @('dualshock2xbox.exe', 'build\Release\dualshock2xbox.exe', 'build\dualshock2xbox.exe') |
       Where-Object { Test-Path $_ } | Select-Object -First 1
if ($exe) {
    Write-Host "[ok] found $exe"
    & $exe --check
} else {
    Write-Host "[!!] dualshock2xbox.exe not found ; build it first (see README.txt, section Build)" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "done ; double-click run.bat to play."
