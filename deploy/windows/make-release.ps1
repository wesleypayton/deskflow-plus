# SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
# SPDX-License-Identifier: MIT

<#
.SYNOPSIS
  Makes the Windows release files from a built Release tree.

.DESCRIPTION
  Produces, in <build>\packages:
    deskflow-plus-<version>-win-x64-setup.exe     installs the Visual C++ runtime if needed, then the MSI
    deskflow-plus-<version>-win-x64-portable.exe  self-extracting; unpacks a portable copy to a folder you pick
  along with the MSI and portable ZIP they are made from.

  Configure and build first, for example:
    cmake -S . -B build/release -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF <toolchain options>
    cmake --build build/release

  Needs Qt's bin folder on PATH (for the install step), WiX 5 with the UI, Util, Firewall and Bal
  extensions (dotnet tool install --global wix --version 5.0.2, then wix extension add --global ...),
  the Visual C++ redistributable from a Visual Studio install, and 7-Zip.
#>
param(
  [string]$BuildDir = 'build/release',
  [string]$VcRedist = '',
  [string]$SevenZip = "$env:ProgramFiles\7-Zip"
)

$ErrorActionPreference = 'Stop'
$repo = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$build = Resolve-Path (Join-Path $repo $BuildDir)
$packages = Join-Path $build 'packages'

function Read-CacheValue([string]$file, [string]$pattern) {
  $match = Select-String -Path $file -Pattern $pattern | Select-Object -First 1
  if (-not $match) { throw "couldn't find $pattern in $file" }
  return $match.Matches[0].Groups[1].Value
}

# the MSI and the portable ZIP, from the project's own packaging
$cmake = (Get-Command cmake).Source
$cpack = Join-Path (Split-Path $cmake) 'cpack.exe'
& $cpack --config "$build\CPackConfig.cmake" -G 'ZIP;WIX' -B $packages
if ($LASTEXITCODE -ne 0) { throw 'cpack failed' }

$cpackConfig = "$build\CPackConfig.cmake"
$version = Read-CacheValue $cpackConfig 'set\(CPACK_PACKAGE_VERSION "([^"]+)"\)'
$base = Read-CacheValue $cpackConfig 'set\(CPACK_PACKAGE_FILE_NAME "([^"]+)"\)'
$msi = Join-Path $packages "$base.msi"
$zip = Join-Path $packages "$base-portable.zip"
$runtimeMinor = Read-CacheValue "$build\deploy\wix-custom.h" 'kWindowsRuntimeMinor = (\d+)'

# the setup .exe: the Visual C++ runtime, then the MSI
if (-not $VcRedist) {
  $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
  $vs = & $vswhere -latest -products * -property installationPath
  $VcRedist = Get-ChildItem "$vs\VC\Redist\MSVC" -Recurse -Filter 'vc_redist.x64.exe' |
    Sort-Object FullName -Descending | Select-Object -First 1 -ExpandProperty FullName
}
if (-not (Test-Path $VcRedist)) { throw "couldn't find vc_redist.x64.exe, pass -VcRedist" }

# the Bal extension's DLL isn't named after the extension, so WiX can't find it by name
$bal = Get-ChildItem "$env:USERPROFILE\.wix\extensions\WixToolset.Bal.wixext" -Recurse -Filter '*.dll' |
  Select-Object -First 1 -ExpandProperty FullName
if (-not $bal) { throw 'the WiX Bal extension is missing: wix extension add --global WixToolset.Bal.wixext/5.0.2' }

$setup = Join-Path $packages "$base-setup.exe"
wix build "$PSScriptRoot\bundle.wxs" -arch x64 -ext $bal -ext WixToolset.Util.wixext `
  -d "Version=$version" -d "Msi=$msi" -d "VcRedist=$VcRedist" -d "RuntimeMinor=$runtimeMinor" `
  -d "Icon=$repo\src\apps\res\deskflow.ico" -o $setup
if ($LASTEXITCODE -ne 0) { throw 'wix build failed' }

# the portable .exe: a 7-Zip self-extracting archive of the portable folder
$staging = Join-Path $packages '_portable'
Remove-Item $staging -Recurse -Force -ErrorAction SilentlyContinue
Expand-Archive -Path $zip -DestinationPath $staging
$archive = Join-Path $packages "$base-portable.7z"
Remove-Item $archive -ErrorAction SilentlyContinue
& "$SevenZip\7z.exe" a -t7z -mx=9 $archive "$staging\*" | Out-Null
if ($LASTEXITCODE -ne 0) { throw '7-Zip failed' }

$portable = Join-Path $packages "$base-portable.exe"
$sfx = [IO.File]::ReadAllBytes("$SevenZip\7z.sfx")
$data = [IO.File]::ReadAllBytes($archive)
$out = [IO.File]::Create($portable)
$out.Write($sfx, 0, $sfx.Length)
$out.Write($data, 0, $data.Length)
$out.Close()
Remove-Item $staging -Recurse -Force
Remove-Item $archive

Get-Item $setup, $portable, $msi, $zip | Select-Object Name, @{ n = 'MB'; e = { [math]::Round($_.Length / 1MB, 1) } }
