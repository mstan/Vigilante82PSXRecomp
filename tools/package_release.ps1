param(
    [string]$BuildDir = 'build-release',
    [string]$RecompilerBuild = 'psxrecomp-v4/recompiler/build-release',
    [string]$CacheDir = 'build-release/cache',
    [string]$MingwBin = 'C:/msys64/mingw64/bin'
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$version = (Get-Content -LiteralPath "$root/packaging/release/VERSION" -Raw).Trim()
$stage = "$root/release-stage/Vigilante82PSXRecomp-$version-windows-x64"
$env:PATH = "$MingwBin;$env:PATH"
& py -3 "$root/tools/package_release.py" --build-dir "$root/$BuildDir" --stage $stage `
    --recompiler-build "$root/$RecompilerBuild" --cache-dir "$root/$CacheDir" `
    --download-cache "$root/build-release/toolchain-cache" --platform win --mingw-bin $MingwBin `
    --zip "$root/dist/Vigilante82PSXRecomp-$version-windows-x64.zip"
if ($LASTEXITCODE -ne 0) { throw 'Release staging failed' }
