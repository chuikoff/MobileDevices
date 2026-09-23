$ErrorActionPreference = "Stop"
$root = $PSScriptRoot.TrimEnd("\", "/")
$pluginDir = Join-Path $root "Plugin"
$dist = Join-Path $root "dist"
$inf = Join-Path $root "pluginst.inf"

if (-not (Test-Path $inf)) { throw "pack failed: missing pluginst.inf" }
$verLine = Select-String -Path $inf -Pattern "^version=(.+)$" | Select-Object -First 1
if (-not $verLine) { throw "pack failed: no version= in pluginst.inf" }
$ver = $verLine.Matches[0].Groups[1].Value.Trim()
if (-not $ver) { throw "pack failed: empty version" }

$wfx = Join-Path $pluginDir "mobiledevices.wfx"
$wfx64 = Join-Path $pluginDir "mobiledevices.wfx64"
if (-not (Test-Path $wfx) -and -not (Test-Path $wfx64)) {
	throw "pack failed: missing Plugin\mobiledevices.wfx / .wfx64"
}

$mtx = New-Object System.Threading.Mutex($false, "Global\MobileDevicesPackPlugin")
$taken = $false
try {
	$taken = $mtx.WaitOne(60000)
	if (-not $taken) { throw "pack failed: lock timeout" }

	if (-not (Test-Path $dist)) { New-Item -ItemType Directory -Path $dist | Out-Null }
	$stage = Join-Path $dist "stage"
	if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
	New-Item -ItemType Directory -Path $stage | Out-Null

	if (Test-Path $wfx) { Copy-Item $wfx (Join-Path $stage "mobiledevices.wfx") }
	if (Test-Path $wfx64) { Copy-Item $wfx64 (Join-Path $stage "mobiledevices.wfx64") }
	Copy-Item $inf (Join-Path $stage "pluginst.inf")
	Copy-Item (Join-Path $root "ReadMe.txt") (Join-Path $stage "ReadMe.txt")
	Copy-Item (Join-Path $root "LICENSE.txt") (Join-Path $stage "LICENSE.txt")

	$zip = Join-Path $dist ("MobileDevices-" + $ver + ".zip")
	if (Test-Path $zip) { Remove-Item -Force $zip }

	Add-Type -AssemblyName System.IO.Compression.FileSystem
	[System.IO.Compression.ZipFile]::CreateFromDirectory($stage, $zip, [System.IO.Compression.CompressionLevel]::Optimal, $false)

	Remove-Item -Recurse -Force $stage
	if (-not (Test-Path $zip)) { throw "pack failed: zip not created" }
	$item = Get-Item $zip
	Write-Host ("Packed " + $item.Name + " (" + $item.Length + " bytes)")
}
finally {
	if ($taken) { $mtx.ReleaseMutex() | Out-Null }
	$mtx.Dispose()
}
