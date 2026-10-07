param(
    [string]$BuildDir = 'build-release',
    [string]$RecompilerBuild = 'psxrecomp-v4/recompiler/build-release',
    [string]$CacheDir = 'build-release/cache',
    [string]$MingwBin = 'C:/msys64/mingw64/bin',
    [string]$FrameworkRoot = 'psxrecomp-v4'
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
function Resolve-TitlePath([string]$Path) {
    if ([System.IO.Path]::IsPathRooted($Path)) { return [System.IO.Path]::GetFullPath($Path) }
    return [System.IO.Path]::GetFullPath((Join-Path -Path $root -ChildPath $Path))
}
$version = (Get-Content -LiteralPath "$root/packaging/release/VERSION" -Raw).Trim()
$stage = "$root/release-stage/Vigilante82PSXRecomp-$version-windows-x64"
$env:PATH = "$MingwBin;$env:PATH"
& py -3 "$root/tools/package_release.py" --build-dir (Resolve-TitlePath $BuildDir) --stage $stage `
    --recompiler-build (Resolve-TitlePath $RecompilerBuild) --cache-dir (Resolve-TitlePath $CacheDir) `
    --framework (Resolve-TitlePath $FrameworkRoot) `
    --download-cache "$root/build-release/toolchain-cache" --platform win --mingw-bin $MingwBin `
    --zip "$root/dist/Vigilante82PSXRecomp-$version-windows-x64.zip"
if ($LASTEXITCODE -ne 0) { throw 'Release staging failed' }
