$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$src = Join-Path $root "src\hiddify_settings_tool.cpp"
$dist = Join-Path $root "dist"
$out = Join-Path $dist "hiddify_settings_tool.exe"

New-Item -ItemType Directory -Path $dist -Force | Out-Null

$gpp = Get-Command g++ -ErrorAction SilentlyContinue
if (-not $gpp) {
  $winlibs = Join-Path $env:LOCALAPPDATA "Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin\g++.exe"
  if (Test-Path $winlibs) {
    $gpp = $winlibs
  } else {
    throw "g++ was not found. Install WinLibs/MinGW-w64 first."
  }
} else {
  $gpp = $gpp.Source
}

& $gpp -std=c++17 -O2 -static -static-libgcc -static-libstdc++ -finput-charset=UTF-8 -fexec-charset=UTF-8 $src -o $out -lcomdlg32 -lshell32 -lurlmon -lole32

Write-Host "Built $out"
