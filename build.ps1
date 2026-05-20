param(
    [switch]$NativeImage
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$src = Join-Path $root "src"
$build = Join-Path $root "build"
$appName = [string]::Concat([char]0x314B, [char]0x314A)
$outputFolder = Join-Path $build "netfx"
$icon = Join-Path $root "assets\wincap.ico"
$iconTool = Join-Path $root "tools\make-icon.ps1"
$csc = Join-Path $env:WINDIR "Microsoft.NET\Framework64\v4.0.30319\csc.exe"

if (-not (Test-Path -LiteralPath $csc)) {
    $csc = Join-Path $env:WINDIR "Microsoft.NET\Framework\v4.0.30319\csc.exe"
}

if (-not (Test-Path -LiteralPath $csc)) {
    throw "csc.exe was not found. Install .NET Framework developer tools or the .NET SDK."
}

New-Item -ItemType Directory -Force -Path $outputFolder | Out-Null

if (-not (Test-Path -LiteralPath $icon)) {
    & $iconTool -OutPath $icon
}

$sources = Get-ChildItem -LiteralPath $src -Filter *.cs | Sort-Object FullName | ForEach-Object { $_.FullName }
$output = Join-Path $outputFolder "$appName.exe"

& $csc /nologo /target:winexe /optimize+ /debug- /filealign:512 /platform:x64 /out:$output /win32icon:$icon `
    /reference:System.dll `
    /reference:System.Drawing.dll `
    /reference:System.Windows.Forms.dll `
    $sources

if ($LASTEXITCODE -ne 0) {
    throw "C# compilation failed with exit code $LASTEXITCODE."
}

Write-Host "Built build\netfx\$appName.exe"

if ($NativeImage) {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($identity)
    if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
        throw "Native image installation requires an elevated PowerShell window."
    }

    $ngen = Join-Path $env:WINDIR "Microsoft.NET\Framework64\v4.0.30319\ngen.exe"
    if (-not (Test-Path -LiteralPath $ngen)) {
        $ngen = Join-Path $env:WINDIR "Microsoft.NET\Framework\v4.0.30319\ngen.exe"
    }

    if (-not (Test-Path -LiteralPath $ngen)) {
        throw "ngen.exe was not found."
    }

    & $ngen install $output /nologo
    if ($LASTEXITCODE -ne 0) {
        throw "NGen failed with exit code $LASTEXITCODE."
    }
}
