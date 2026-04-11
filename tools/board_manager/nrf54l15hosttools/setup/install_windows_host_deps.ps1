$ErrorActionPreference = "Stop"

if (Get-Command py -ErrorAction SilentlyContinue) {
  & py -3 -m pip install --user --upgrade pip -r "$PSScriptRoot\\..\\requirements-pyocd.txt"
} elseif (Get-Command python -ErrorAction SilentlyContinue) {
  & python -m pip install --user --upgrade pip -r "$PSScriptRoot\\..\\requirements-pyocd.txt"
} else {
  throw "Python 3 is required to install pyOCD."
}

Write-Host "Host upload dependencies are ready."
Write-Host "Restart the Arduino IDE if it was already open."
