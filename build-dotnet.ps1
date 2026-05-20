param(
    [switch]$SelfContained
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$project = Join-Path $root "src\WinCapSimple.csproj"
$output = Join-Path $root "build\dotnet-r2r"
$appName = [string]::Concat([char]0x314B, [char]0x314A)

if (-not (Get-Command dotnet -ErrorAction SilentlyContinue)) {
    throw "dotnet was not found. Install the .NET SDK."
}

New-Item -ItemType Directory -Force -Path $output | Out-Null

$selfContainedValue = if ($SelfContained) { "true" } else { "false" }

& dotnet publish $project `
    -c Release `
    -r win-x64 `
    --self-contained $selfContainedValue `
    -o $output `
    -p:PublishReadyToRun=true `
    -p:AssemblyName=$appName `
    -p:DebugType=none `
    -p:DebugSymbols=false `
    -p:PublishSingleFile=false

if ($LASTEXITCODE -ne 0) {
    throw "dotnet publish failed with exit code $LASTEXITCODE."
}

Write-Host "Built build\dotnet-r2r\$appName.exe"
