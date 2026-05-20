$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$native = Join-Path $root "native\wincap_native.c"
$nativeResource = Join-Path $root "native\wincap_native.rc"
$build = Join-Path $root "build"
$appName = [string]::Concat([char]0x314B, [char]0x314A)
$output = Join-Path $build "$appName.exe"
$linkOutput = Join-Path $build "wincap_native_build.exe"
$object = Join-Path $build "wincap_native.obj"
$resource = Join-Path $build "wincap_native.res"
$icon = Join-Path $root "assets\wincap.ico"
$iconTool = Join-Path $root "tools\make-icon.ps1"
$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"

if (-not (Test-Path -LiteralPath $vswhere)) {
    throw "vswhere.exe was not found. Install Visual Studio Build Tools with C++ tools."
}

$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) {
    throw "Visual C++ build tools were not found."
}

$vcvars = Join-Path $vs "VC\Auxiliary\Build\vcvars64.bat"
if (-not (Test-Path -LiteralPath $vcvars)) {
    throw "vcvars64.bat was not found."
}

New-Item -ItemType Directory -Force -Path $build | Out-Null

if (-not (Test-Path -LiteralPath $icon)) {
    & $iconTool -OutPath $icon
}

$command = "`"$vcvars`" >nul && rc /nologo /fo `"$resource`" `"$nativeResource`" && cl /nologo /O1 /Os /GS- /GF /Gw /Gy /Fo`"$object`" /Fe`"$linkOutput`" `"$native`" `"$resource`" kernel32.lib user32.lib gdi32.lib comdlg32.lib d3d11.lib dxgi.lib dxguid.lib /link /NODEFAULTLIB /ENTRY:wWinMainCRTStartup /OPT:REF /OPT:ICF /FILEALIGN:512 /SUBSYSTEM:WINDOWS"
cmd.exe /d /c $command

if ($LASTEXITCODE -ne 0) {
    throw "Native compilation failed with exit code $LASTEXITCODE."
}

Move-Item -LiteralPath $linkOutput -Destination $output -Force
Remove-Item -LiteralPath $object, $resource -Force -ErrorAction SilentlyContinue
Write-Host "Built build\$appName.exe"
