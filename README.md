# WinCap Simple

## 한국어

### 개요

가볍고 빠른 Windows 캡처 도구입니다. 기본 Windows 캡처 도구보다 빠르게 뜨는 것을 목표로, 일반 .NET 버전과 콜드 스타트 우선의 네이티브 Win32 버전을 함께 제공합니다.

가장 빠른 실행이 필요하면 `WinCapNative.exe`를 사용하세요. 이 버전은 .NET, WinForms, C runtime 시작 경로를 피하고 `/NODEFAULTLIB`와 커스텀 entry point를 사용합니다.

### 빠른 시작

네이티브 Win32 버전:

```powershell
.\build-native.ps1
.\build\WinCapNative.exe
```

.NET Framework WinForms 버전:

```powershell
.\build.ps1
.\build\WinCapSimple.exe
```

.NET SDK가 설치되어 있으면 관리형 앱을 .NET 10 ReadyToRun 형태로도 빌드할 수 있습니다. 네이티브 C 버전만큼 작지는 않지만 JIT 비용을 줄이는 보조 경로입니다.

```powershell
.\build-dotnet.ps1
.\build\dotnet-r2r\WinCapSimple.exe
```

### 빌드 결과

```text
build\WinCapNative.exe   네이티브 Win32 빠른 시작 빌드
build\WinCapSimple.exe   .NET Framework WinForms 빌드
build\dotnet-r2r\        .NET 10 ReadyToRun WinForms publish
```

### 기능

- 새 캡처 버튼
- 직사각형, 자유형, 창, 디스플레이 선택 캡처
- 지연 캡처: 없음, 1초, 3초, 5초, 10초
- 연사 캡처: 임시 폴더에 프레임 저장 후 보기에서 선택
- 네이티브 연사 중 캡처 영역 외곽선 표시
- Ctrl+C로 현재 캡처 복사, 넓은 붙여넣기 호환성을 위해 bitmap/DIB 포맷 제공
- 기본 마커 도구
- Layered 캡처 토글: 투명/오버레이 창 포함이 필요할 때만 사용
- 카메라 스타일 앱 아이콘

### 성능 메모

- 네이티브 버전은 콜드 스타트 우선입니다.
- 아이콘은 압축 ICO 리소스로 포함되며 실행 성능에 의미 있는 영향을 주지 않습니다.
- 캡처 직후 자동 클립보드 합성을 하지 않고, Ctrl+C 또는 Copy 시점에만 합성합니다.
- 연사 캡처는 캡처 버퍼와 GDI 리소스를 재사용합니다.
- 네이티브 연사 캡처는 burst 중 1ms 타이머 해상도, 높은 스케줄링 우선순위, 단일 RAM 프레임 블록, 픽셀 재복사 없는 BMP 저장 경로를 사용합니다.
- 네이티브 burst 영역 표시는 캡처 루프와 분리된 click-through overlay로 한 번만 띄우며, 가능한 경우 Windows 캡처 제외 속성을 적용합니다.
- 네이티브 디스플레이 연사는 가능한 경우 DXGI Output Duplication을 사용하고, 실패하면 GDI로 자동 fallback합니다.
- 네이티브 DXGI 경로는 D3D11 device/context/output을 캐시하여 같은 디스플레이에서 두 번째 이후 burst 시작 비용을 줄입니다.
- 기본 캡처는 `CAPTUREBLT`를 끕니다. Layered 토글을 켜면 투명/레이어드 창이 포함될 수 있지만 더 느릴 수 있습니다.
- 자유형 마스크는 픽셀마다 전체 폴리곤을 검사하지 않고 스캔라인 구간 방식으로 처리합니다.
- .NET SDK 빌드는 `PublishReadyToRun`을 사용합니다. Windows Forms는 NativeAOT/트리밍 경로와 맞지 않으므로 이 프로젝트의 초고속 경로는 네이티브 C 빌드입니다.

### 검증

```powershell
.\build\WinCapSimple.exe --self-test
.\build\dotnet-r2r\WinCapSimple.exe --self-test
.\build\WinCapNative.exe --self-test
.\build\WinCapNative.exe --self-test-dxgi
```

DXGI self-test는 같은 프로세스에서 display burst를 두 번 실행해 device cache 재사용 경로도 확인합니다.

### OS 캐시와 백신 영향 줄이기

Windows 캐시와 백신 검사는 완전히 제거할 수는 없습니다. 다만 아래 방법으로 변동을 줄일 수 있습니다.

- 빌드 폴더나 temp 폴더보다 고정 설치 폴더에서 실행하세요.
- 릴리스 exe를 자주 다시 만들지 마세요. 해시가 바뀌면 백신이 다시 검사할 가능성이 큽니다.
- 배포용이면 코드 서명을 고려하세요.
- Defender 제외는 필요한 경우에만, 신뢰하는 exe 또는 설치 폴더 하나로 좁게 설정하세요. 전체 개발 폴더나 다운로드 폴더 제외는 권장하지 않습니다.
- 더 빠른 체감이 필요하면 트레이 상주/pre-warm 모드를 추가하는 방식이 가장 확실합니다.

### .NET Native Image 옵션

.NET Framework 버전의 콜드 스타트를 더 줄이고 싶다면 관리자 PowerShell에서 NGen을 사용할 수 있습니다.

```powershell
.\build.ps1 -NativeImage
```

이 단계는 시스템 네이티브 이미지 캐시를 사용하므로 관리자 권한이 필요합니다.

## English

### Overview

WinCap Simple is a lightweight Windows capture utility focused on fast startup and low capture latency. It ships with both a managed .NET build and a native Win32 fast-start build.

Use `WinCapNative.exe` when startup speed matters most. The native version avoids .NET, WinForms, and the C runtime startup path. It links with `/NODEFAULTLIB` and uses a custom entry point.

### Quick Start

Native Win32 build:

```powershell
.\build-native.ps1
.\build\WinCapNative.exe
```

.NET Framework WinForms build:

```powershell
.\build.ps1
.\build\WinCapSimple.exe
```

If the .NET SDK is installed, the managed app can also be published as a .NET 10 ReadyToRun build. It is not as small as the native C build, but it reduces JIT work.

```powershell
.\build-dotnet.ps1
.\build\dotnet-r2r\WinCapSimple.exe
```

### Build Outputs

```text
build\WinCapNative.exe   Native Win32 fast-start build
build\WinCapSimple.exe   .NET Framework WinForms build
build\dotnet-r2r\        .NET 10 ReadyToRun WinForms publish
```

### Features

- New Capture button
- Rectangle, freeform, window, and display capture
- Delay capture: none, 1s, 3s, 5s, 10s
- Burst capture: saves frames to temp storage and lets you pick one later
- Native burst shows a capture-area outline while recording
- Ctrl+C copies the current capture and publishes bitmap/DIB formats for broad paste compatibility
- Basic marker tool
- Layered capture toggle: enable only when transparent or overlay windows must be included
- Camera-style app icon

### Performance Notes

- The native build is the preferred cold-start path.
- The icon is embedded as a compressed ICO resource and should not meaningfully affect runtime performance.
- Clipboard flattening runs only on Ctrl+C or Copy, not immediately after capture.
- Burst capture reuses capture buffers and GDI resources.
- Native burst capture uses 1ms timer resolution, elevated scheduling priority, one contiguous RAM frame block, and a BMP save path that avoids extra pixel copies.
- The native burst outline is a one-time click-through overlay outside the capture loop, with Windows capture exclusion applied when available.
- Native display burst uses DXGI Output Duplication when available and automatically falls back to GDI.
- The native DXGI path caches the D3D11 device, context, and output to reduce startup cost for later bursts on the same display.
- Default capture leaves `CAPTUREBLT` off. The Layered toggle may include transparent or layered windows, but can be slower.
- Freeform masking uses scanline spans instead of testing the full polygon per pixel.
- The .NET SDK build uses `PublishReadyToRun`. Windows Forms does not fit the NativeAOT/trimming path, so the native C build remains this project's fastest path.

### Verification

```powershell
.\build\WinCapSimple.exe --self-test
.\build\dotnet-r2r\WinCapSimple.exe --self-test
.\build\WinCapNative.exe --self-test
.\build\WinCapNative.exe --self-test-dxgi
```

The DXGI self-test runs display burst twice in one process, which also verifies the device-cache reuse path.

### Reducing OS Cache and Antivirus Impact

Windows caching and antivirus scanning cannot be fully removed, but these steps can reduce variance.

- Run from a stable install folder instead of a build or temp folder.
- Avoid constantly rebuilding the release exe; changed hashes are more likely to be rescanned.
- Consider code signing for distribution.
- Use Defender exclusions only when necessary, scoped to the trusted exe or one install folder. Avoid broad exclusions for development or download folders.
- A resident tray/pre-warm mode is the strongest option if instant perceived startup is required.

### .NET Native Image Option

For the .NET Framework build, an elevated PowerShell can install a native image with NGen.

```powershell
.\build.ps1 -NativeImage
```

This uses the system native image cache and requires administrator privileges.
