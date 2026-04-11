$ErrorActionPreference = "Stop"

if (Get-Command py -ErrorAction SilentlyContinue) {
  $pythonCmd = @("py", "-3")
} elseif (Get-Command python -ErrorAction SilentlyContinue) {
  $pythonCmd = @("python")
} else {
  throw "Python 3 is required to install pyOCD."
}

$pythonPrefix = @()
if ($pythonCmd.Length -gt 1) {
  $pythonPrefix = $pythonCmd[1..($pythonCmd.Length - 1)]
}

function Invoke-Python {
  param([Parameter(ValueFromRemainingArguments = $true)][string[]]$Args)
  & $pythonCmd[0] @pythonPrefix @Args
}

$pyTag = (Invoke-Python -c "import sys; print(f'cp{sys.version_info.major}{sys.version_info.minor}')").Trim()
$wheelhouse = Join-Path $PSScriptRoot "..\\wheelhouse\\$pyTag"
$installArgs = @("-m", "pip", "install", "--user", "--upgrade")

if (Test-Path $wheelhouse) {
  Write-Host "Using bundled offline wheelhouse: $wheelhouse"
  $installArgs += @("--no-index", "--find-links", $wheelhouse)
}

$installArgs += @("-r", "$PSScriptRoot\\..\\requirements-pyocd.txt")

Invoke-Python @installArgs
if ($LASTEXITCODE -ne 0) {
  if (Test-Path $wheelhouse) {
    Write-Host "Bundled wheelhouse install failed; retrying with online indexes..."
    Invoke-Python -m pip install --user --upgrade -r "$PSScriptRoot\\..\\requirements-pyocd.txt"
    if ($LASTEXITCODE -ne 0) {
      throw "pyOCD installation failed."
    }
  } else {
    throw "pyOCD installation failed."
  }
}

Write-Host "Host upload dependencies are ready."
Write-Host "Restart the Arduino IDE if it was already open."
