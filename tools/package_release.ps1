param([Parameter(ValueFromRemainingArguments=$true)][string[]]$Arguments)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
& "$root/psxrecomp-v4/tools/package_title_release.ps1" --root $root @Arguments
exit $LASTEXITCODE
