# WinCap Simple

## 한국어

### 개요

가볍고 빠른 Windows 캡처 도구입니다. 기본 Windows 캡처 도구보다 빠르게 뜨는 것을 목표로, 일반 .NET 버전과 콜드 스타트 우선의 네이티브 Win32 버전을 함께 제공합니다.

가장 빠른 실행이 필요하면 `build\ㅋㅊ.exe`를 사용하세요. 이 버전은 .NET, WinForms, C runtime 시작 경로를 피하고 `/NODEFAULTLIB`와 커스텀 entry point를 사용합니다. 보조 .NET 빌드도 실행 파일 이름은 `ㅋㅊ.exe`로 맞추되, 서로 덮어쓰지 않도록 별도 하위 폴더에 둡니다.

### 시스템 요구사항

- 실행: Windows 10 이상. 일부 기능(Windows 캡처 제외 속성)은 Windows 10 2004 이상에서 활성화됩니다.
- 네이티브 빌드: Visual Studio Build Tools (`Microsoft.VisualStudio.Component.VC.Tools.x86.x64`).
- .NET Framework 빌드: Windows 기본 설치된 .NET Framework 4.x. 별도 SDK 불필요.
- .NET 10 R2R 빌드: .NET 10 SDK.
- MP4/GIF 내보내기: `ffmpeg.exe`가 `PATH`에 있어야 합니다.

### 빠른 시작

네이티브 Win32 버전:

```powershell
.\build-native.ps1
.\build\ㅋㅊ.exe
```

.NET Framework WinForms 버전:

```powershell
.\build.ps1
.\build\netfx\ㅋㅊ.exe
```

.NET SDK가 설치되어 있으면 관리형 앱을 .NET 10 ReadyToRun 형태로도 빌드할 수 있습니다. 네이티브 C 버전만큼 작지는 않지만 JIT 비용을 줄이는 보조 경로입니다.

```powershell
.\build-dotnet.ps1
.\build\dotnet-r2r\ㅋㅊ.exe
```

### 빌드 결과

```text
build\ㅋㅊ.exe             네이티브 Win32 빠른 시작 빌드
build\netfx\ㅋㅊ.exe       .NET Framework WinForms 빌드
build\dotnet-r2r\ㅋㅊ.exe  .NET 10 ReadyToRun WinForms publish
```

### 빌드 비교

| 항목 | Native | .NET Framework | .NET 10 R2R |
|---|---|---|---|
| 콜드 스타트 | 가장 빠름 | 보통 | 빠름 |
| 빌드 SDK 의존성 | VC++ Build Tools | 없음 | .NET 10 SDK |
| 단일 저장 포맷 | BMP | PNG | PNG |
| 연사 프레임 포맷 | BMP | PNG | PNG |
| DXGI Output Duplication | 디스플레이 연사에서 사용 | 미사용 | 미사용 |
| Burst RAM 버퍼링 | ✓ | 부분 (DC/버퍼 재사용) | 부분 |
| Burst 영역 외곽선 | ✓ | — | — |
| Burst MP4/GIF 내보내기 | ffmpeg 사용 | ffmpeg 사용 | ffmpeg 사용 |

### 기능

- 새 캡처 버튼
- 직사각형, 자유형, 창, 디스플레이 선택 캡처
- 지연 캡처: 없음, 1초, 3초, 5초, 10초
- 연사 캡처: 임시 폴더에 프레임 저장 후 보기에서 선택
- 연사 캡처를 MP4 또는 GIF로 내보내기
- 네이티브 연사 중 캡처 영역 외곽선 표시
- Ctrl+C로 현재 캡처 복사, 넓은 붙여넣기 호환성을 위해 bitmap/DIB 포맷 제공
- 캡처된 이미지 회전, 가로 대칭, 세로 대칭
- 마커 도구와 1/3/6/10/16/24 px 굵기 선택
- Layered 캡처 토글: 투명/오버레이 창 포함이 필요할 때만 사용
- 카메라 스타일 앱 아이콘

### 키보드 단축키

| 단축키 | 동작 |
|---|---|
| Ctrl+N | 새 캡처 |
| Ctrl+C | 현재 캡처를 클립보드로 복사 (마커 포함) |
| Esc | 영역 선택 오버레이 취소 |

### 파일 출력

- 단일 캡처는 Save 버튼으로 사용자 지정 위치에 저장합니다.
  - 네이티브 빌드: BMP
  - .NET 빌드: PNG
- 연사 캡처는 자동으로 다음 경로에 저장됩니다.

```text
%TEMP%\WinCapSimple\Bursts\<yyyyMMdd_HHmmss>\frame_NNNN.{bmp|png}
```

- View Burst 버튼은 가장 최근 연사 폴더를 다시 엽니다. 메인 윈도우를 닫아도 디스크에 남아있으므로 같은 폴더를 다시 열 수 있습니다.
- Burst Viewer의 MP4/GIF 버튼은 저장된 프레임을 `ffmpeg`로 인코딩합니다. 프레임 간격은 burst 폴더의 `interval_ms.txt`를 사용합니다.

### 성능 메모

- 네이티브 버전은 콜드 스타트 우선입니다.
- 아이콘은 압축 ICO 리소스로 포함되며 실행 성능에 의미 있는 영향을 주지 않습니다.
- 캡처 직후 자동 클립보드 합성을 하지 않고, Ctrl+C 또는 Copy 시점에만 합성합니다.
- 연사 캡처는 캡처 버퍼와 GDI 리소스를 재사용합니다.
- MP4/GIF 인코딩은 캡처 이후 사용자가 요청할 때만 실행되므로 burst 캡처 성능에는 영향을 주지 않습니다.
- 네이티브 연사 캡처는 burst 중 1ms 타이머 해상도, 높은 스케줄링 우선순위, 단일 RAM 프레임 블록, 픽셀 재복사 없는 BMP 저장 경로를 사용합니다.
- 네이티브 burst 영역 표시는 캡처 루프와 분리된 click-through overlay로 한 번만 띄우며, 가능한 경우 Windows 캡처 제외 속성을 적용합니다.
- 네이티브 디스플레이 연사는 가능한 경우 DXGI Output Duplication을 사용하고, 실패하면 GDI로 자동 fallback합니다.
- 네이티브 DXGI 경로는 D3D11 device/context/output을 캐시하여 같은 디스플레이에서 두 번째 이후 burst 시작 비용을 줄입니다.
- 마커 그리기는 배경 지우기를 피하고 더블버퍼링/부분 무효화를 사용해 드래그 중 깜빡임과 불필요한 재그리기를 줄입니다.
- 기본 캡처는 `CAPTUREBLT`를 끕니다. Layered 토글을 켜면 투명/레이어드 창이 포함될 수 있지만 더 느릴 수 있습니다.
- 자유형 마스크는 픽셀마다 전체 폴리곤을 검사하지 않고 스캔라인 구간 방식으로 처리합니다.
- .NET SDK 빌드는 `PublishReadyToRun`을 사용합니다. Windows Forms는 NativeAOT/트리밍 경로와 맞지 않으므로 이 프로젝트의 초고속 경로는 네이티브 C 빌드입니다.

### 알려진 제한

- 창 모드 캡처는 GDI `BitBlt` 기반이라 다음 케이스에서 의도와 다른 결과가 나올 수 있습니다.
  - 다른 창에 가려진 부분: 위 창의 픽셀이 함께 잡힙니다.
  - 하드웨어 오버레이로 그려지는 비디오 또는 일부 게임: 해당 영역이 검은 사각형으로 잡힐 수 있습니다.
  - 최소화된 창: 의미 있는 컨텐츠를 잡을 수 없습니다.
- DRM 보호 콘텐츠는 어떤 경로로도 캡처되지 않습니다 (OS 정책).
- DXGI Output Duplication은 회전된 디스플레이에서는 사용하지 않고 GDI로 자동 fallback합니다.
- 네이티브 빌드의 디스플레이 연사 DXGI 가속은 `MODE_DISPLAY`에만 적용됩니다. 다른 모드는 GDI 경로를 사용합니다.
- MP4/GIF 내보내기는 `ffmpeg.exe`가 없으면 실행되지 않습니다. MP4는 `libx264` 인코더가 포함된 ffmpeg 빌드가 필요합니다.

### 검증

```powershell
.\build\netfx\ㅋㅊ.exe --self-test
.\build\dotnet-r2r\ㅋㅊ.exe --self-test
.\build\ㅋㅊ.exe --self-test
.\build\ㅋㅊ.exe --self-test-dxgi
```

DXGI self-test는 같은 프로세스에서 display burst를 두 번 실행해 device cache 재사용 경로도 확인합니다.

종료 코드: `0` 통과, `1` 캡처 또는 저장 실패, `2` DXGI 경로가 실행되지 않았거나 두 번째 burst 실패.

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

Use `build\ㅋㅊ.exe` when startup speed matters most. The native version avoids .NET, WinForms, and the C runtime startup path. It links with `/NODEFAULTLIB` and uses a custom entry point. The helper .NET builds also use the `ㅋㅊ.exe` executable name, but are placed in separate subfolders so build outputs do not overwrite each other.

### Requirements

- Runtime: Windows 10 or later. Some features (capture-exclusion attribute) require Windows 10 version 2004 or later.
- Native build: Visual Studio Build Tools (`Microsoft.VisualStudio.Component.VC.Tools.x86.x64`).
- .NET Framework build: the .NET Framework 4.x that ships with Windows. No SDK required.
- .NET 10 R2R build: the .NET 10 SDK.
- MP4/GIF export: `ffmpeg.exe` must be available in `PATH`.

### Quick Start

Native Win32 build:

```powershell
.\build-native.ps1
.\build\ㅋㅊ.exe
```

.NET Framework WinForms build:

```powershell
.\build.ps1
.\build\netfx\ㅋㅊ.exe
```

If the .NET SDK is installed, the managed app can also be published as a .NET 10 ReadyToRun build. It is not as small as the native C build, but it reduces JIT work.

```powershell
.\build-dotnet.ps1
.\build\dotnet-r2r\ㅋㅊ.exe
```

### Build Outputs

```text
build\ㅋㅊ.exe             Native Win32 fast-start build
build\netfx\ㅋㅊ.exe       .NET Framework WinForms build
build\dotnet-r2r\ㅋㅊ.exe  .NET 10 ReadyToRun WinForms publish
```

### Build Comparison

| Aspect | Native | .NET Framework | .NET 10 R2R |
|---|---|---|---|
| Cold start | Fastest | Moderate | Fast |
| Build SDK | VC++ Build Tools | None | .NET 10 SDK |
| Single-save format | BMP | PNG | PNG |
| Burst frame format | BMP | PNG | PNG |
| DXGI Output Duplication | Display burst | — | — |
| Burst RAM buffering | Yes | Partial (DC and buffer reuse) | Partial |
| Burst area outline | Yes | — | — |
| Burst MP4/GIF export | ffmpeg | ffmpeg | ffmpeg |

### Features

- New Capture button
- Rectangle, freeform, window, and display capture
- Delay capture: none, 1s, 3s, 5s, 10s
- Burst capture: saves frames to temp storage and lets you pick one later
- Export burst captures to MP4 or GIF
- Native burst shows a capture-area outline while recording
- Ctrl+C copies the current capture and publishes bitmap/DIB formats for broad paste compatibility
- Rotate captured images and flip them horizontally or vertically
- Marker tool with 1/3/6/10/16/24 px width options
- Layered capture toggle: enable only when transparent or overlay windows must be included
- Camera-style app icon

### Keyboard Shortcuts

| Shortcut | Action |
|---|---|
| Ctrl+N | New capture |
| Ctrl+C | Copy the current capture to clipboard (markers included) |
| Esc | Cancel the selection overlay |

### File Outputs

- Single capture is saved through the Save button to a user-chosen location.
  - Native build: BMP
  - .NET builds: PNG
- Burst capture is written automatically to:

```text
%TEMP%\WinCapSimple\Bursts\<yyyyMMdd_HHmmss>\frame_NNNN.{bmp|png}
```

- The View Burst button reopens the most recent burst folder. Frames persist on disk, so the same folder can be reopened later.
- The MP4/GIF buttons in Burst Viewer encode the saved frames through `ffmpeg`. Frame timing comes from `interval_ms.txt` in the burst folder.

### Performance Notes

- The native build is the preferred cold-start path.
- The icon is embedded as a compressed ICO resource and should not meaningfully affect runtime performance.
- Clipboard flattening runs only on Ctrl+C or Copy, not immediately after capture.
- Burst capture reuses capture buffers and GDI resources.
- MP4/GIF encoding runs only after capture when requested, so it does not affect burst capture performance.
- Native burst capture uses 1ms timer resolution, elevated scheduling priority, one contiguous RAM frame block, and a BMP save path that avoids extra pixel copies.
- The native burst outline is a one-time click-through overlay outside the capture loop, with Windows capture exclusion applied when available.
- Native display burst uses DXGI Output Duplication when available and automatically falls back to GDI.
- The native DXGI path caches the D3D11 device, context, and output to reduce startup cost for later bursts on the same display.
- Marker drawing avoids background erases and uses double buffering/partial invalidation to reduce flicker and unnecessary repainting while dragging.
- Default capture leaves `CAPTUREBLT` off. The Layered toggle may include transparent or layered windows, but can be slower.
- Freeform masking uses scanline spans instead of testing the full polygon per pixel.
- The .NET SDK build uses `PublishReadyToRun`. Windows Forms does not fit the NativeAOT/trimming path, so the native C build remains this project's fastest path.

### Known Limitations

- Window-mode capture uses GDI `BitBlt`, so the result can differ from intent in these cases:
  - Areas obscured by another window: pixels from the foreground window are included.
  - Hardware-overlay video or some games: the area may capture as a black rectangle.
  - Minimized windows: no meaningful content is captured.
- DRM-protected content cannot be captured by any path (OS policy).
- DXGI Output Duplication is skipped on rotated displays and automatically falls back to GDI.
- The native DXGI acceleration applies only to `MODE_DISPLAY` burst. Other modes use the GDI path.
- MP4/GIF export does not run without `ffmpeg.exe`. MP4 requires an ffmpeg build that includes the `libx264` encoder.

### Verification

```powershell
.\build\netfx\ㅋㅊ.exe --self-test
.\build\dotnet-r2r\ㅋㅊ.exe --self-test
.\build\ㅋㅊ.exe --self-test
.\build\ㅋㅊ.exe --self-test-dxgi
```

The DXGI self-test runs display burst twice in one process, which also verifies the device-cache reuse path.

Exit codes: `0` pass, `1` capture or save failed, `2` DXGI path did not run or the second burst failed.

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
