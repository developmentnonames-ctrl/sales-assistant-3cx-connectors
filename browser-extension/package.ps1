# Packs the extension for release: a copy in dist\ whose manifest allows only the hosted assistant
# (the localhost entries used for development are removed), zipped for Chrome/Edge/Brave.
#   powershell -ExecutionPolicy Bypass -File package.ps1
$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot

$files = @('manifest.json', 'background.js', '3cx-hook.js', '3cx-bridge.js', 'popup.html', 'popup.js')
$out = Join-Path $PSScriptRoot 'dist\sales-assistant-for-3cx'
if (Test-Path $out) { Remove-Item $out -Recurse -Force }
New-Item -ItemType Directory -Force $out | Out-Null
foreach ($f in $files) { Copy-Item $f $out }

$manifestPath = Join-Path $out 'manifest.json'
$manifest = Get-Content $manifestPath -Raw | ConvertFrom-Json
$manifest.externally_connectable.matches = @($manifest.externally_connectable.matches |
  Where-Object { $_ -like 'https://*' })
$manifest | ConvertTo-Json -Depth 10 | Set-Content $manifestPath -Encoding utf8

$zip = Join-Path $PSScriptRoot ("dist\sales-assistant-for-3cx-" + $manifest.version + ".zip")
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path (Join-Path $out '*') -DestinationPath $zip
Write-Output "release: $zip (allowed pages: $($manifest.externally_connectable.matches -join ', '))"
