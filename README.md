# WinCap Simple

Lightweight Windows capture utility built with WinForms and .NET Framework.

## Features

- New Capture button with rectangle, freeform, window, and display capture modes.
- Delay options: no delay, 1, 3, 5, and 10 seconds.
- Burst capture saves PNG frames into the system temp folder and opens a viewer to choose a frame.
- Ctrl+C copies the current capture with marker edits.
- Basic marker tool with color and width controls.
- Save current capture as PNG.

## Build

Run from this folder:

```powershell
.\build.ps1
```

The output is:

```text
build\WinCapSimple.exe
```

The build script uses the Windows .NET Framework C# compiler, so it does not require the .NET SDK.

For the fastest cold start on .NET Framework, install a native image after building:

```powershell
.\build.ps1 -NativeImage
```

That step uses Windows `ngen.exe` and may require an elevated PowerShell window.
