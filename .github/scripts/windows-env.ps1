$ErrorActionPreference = 'Stop'
$vswhere = "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
$installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $installation) { throw 'Visual Studio C++ tools were not found' }
Import-Module "$installation/Common7/Tools/Microsoft.VisualStudio.DevShell.dll"
Enter-VsDevShell -VsInstallPath $installation -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64'
$llvmCandidates = @(
    "$installation/VC/Tools/Llvm/x64/bin"
    "C:/Program Files/LLVM/bin"
) | Where-Object { Test-Path "$_/clang-cl.exe" }
$selectedLlvm = $llvmCandidates | ForEach-Object {
    $out = (& "$_/clang-cl.exe" --version | Select-Object -First 1) -join ''
    $ver = if ($out -match 'clang version (\d+\.\d+\.\d+)') { [version]$Matches[1] } else { [version]'0.0.0' }
    Write-Host "Found clang-cl $ver in $_"
    [pscustomobject]@{ Path = $_; Version = $ver }
} | Sort-Object Version -Descending | Select-Object -First 1
if (-not $selectedLlvm) { throw 'clang-cl.exe was not found' }
$env:PATH = "$($selectedLlvm.Path);$env:PATH"
foreach ($name in @('PATH', 'INCLUDE', 'LIB', 'LIBPATH', 'VSINSTALLDIR', 'VCToolsInstallDir', 'WindowsSdkDir', 'WindowsSDKVersion')) {
    $value = [Environment]::GetEnvironmentVariable($name)
    if ($value) { "$name=$value" | Out-File -FilePath $env:GITHUB_ENV -Append -Encoding utf8 }
}
"VCPKG_ROOT=$env:GITHUB_WORKSPACE/.deps/vcpkg" | Out-File -FilePath $env:GITHUB_ENV -Append -Encoding utf8
"VCPKG_BINARY_SOURCES=clear;files,$env:GITHUB_WORKSPACE/.deps/binary-cache,readwrite" | Out-File -FilePath $env:GITHUB_ENV -Append -Encoding utf8
New-Item -ItemType Directory -Force "$env:GITHUB_WORKSPACE/.deps/binary-cache" | Out-Null
clang-cl --version
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
