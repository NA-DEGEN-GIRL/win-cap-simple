# WinCap Simple

가볍고 빠른 Windows 캡처 도구입니다. 기본 Windows 캡처 도구보다 빠르게 뜨는 것을 목표로, 일반 .NET 버전과 콜드 스타트 우선의 네이티브 Win32 버전을 함께 제공합니다.

Lightweight Windows capture utility focused on fast startup and low capture latency. It ships with both a managed .NET build and a native Win32 fast-start build.

## 빠른 시작 / Quick Start

가장 빠른 콜드 스타트가 필요하면 네이티브 버전을 빌드하세요.

Use the native build when cold-start speed matters most.

```powershell
.\build-native.ps1
.\build\WinCapNative.exe
```

.NET Framework 버전은 SDK 없이 Windows 기본 C# 컴파일러로 빌드됩니다.

The .NET Framework version builds without the .NET SDK by using the Windows C# compiler.

```powershell
.\build.ps1
.\build\WinCapSimple.exe
```

## 기능 / Features

- 새 캡처 버튼 / New Capture button
- 직사각형, 자유형, 창, 디스플레이 선택 캡처 / Rectangle, freeform, window, and display capture
- 지연 캡처: 없음, 1초, 3초, 5초, 10초 / Delay capture: none, 1s, 3s, 5s, 10s
- 연사 캡처: 임시 폴더에 프레임 저장 후 보기에서 선택 / Burst capture: saves frames to temp storage and lets you pick one later
- Ctrl+C로 현재 캡처 복사 / Ctrl+C copies the current capture
- 기본 마커 도구 / Basic marker tool
- Layered 캡처 토글: 투명/오버레이 창 포함이 필요할 때만 사용 / Layered capture toggle: enable only when transparent/overlay windows must be included
- 카메라 스타일 앱 아이콘 / Camera-style app icon

## 빌드 결과 / Build Outputs

```text
build\WinCapNative.exe   Native Win32 fast-start build
build\WinCapSimple.exe   .NET Framework WinForms build
```

네이티브 버전은 .NET, WinForms, C runtime 시작 경로를 피하고 `/NODEFAULTLIB`와 커스텀 entry point를 사용합니다. 캡처는 Win32 GDI `BitBlt` 기반입니다.

The native version avoids .NET, WinForms, and the C runtime startup path. It links with `/NODEFAULTLIB`, uses a custom entry point, and captures through Win32 GDI `BitBlt`.

## 성능 메모 / Performance Notes

- 네이티브 버전은 콜드 스타트 우선입니다. / The native build is the preferred cold-start path.
- 아이콘은 압축 ICO 리소스로 포함되며 실행 성능에 의미 있는 영향을 주지 않습니다. / The icon is embedded as a compressed ICO resource and should not meaningfully affect runtime performance.
- 캡처 직후 자동 클립보드 합성을 하지 않고, Ctrl+C 또는 Copy 시점에만 합성합니다. / Clipboard flattening runs only on Ctrl+C or Copy, not immediately after capture.
- 연사 캡처는 캡처 버퍼와 GDI 리소스를 재사용합니다. / Burst capture reuses capture buffers and GDI resources.
- 네이티브 연사 캡처는 burst 중 1ms 타이머 해상도, 높은 스케줄링 우선순위, 단일 RAM 프레임 블록, 픽셀 재복사 없는 BMP 저장 경로를 사용합니다. / Native burst capture uses 1ms timer resolution, elevated scheduling priority, one contiguous RAM frame block, and a BMP save path that avoids extra pixel copies.
- 기본 캡처는 `CAPTUREBLT`를 끕니다. Layered 토글을 켜면 투명/레이어드 창이 포함될 수 있지만 더 느릴 수 있습니다. / Default capture leaves `CAPTUREBLT` off. The Layered toggle may include transparent/layered windows, but can be slower.
- 자유형 마스크는 픽셀마다 전체 폴리곤을 검사하지 않고 스캔라인 구간 방식으로 처리합니다. / Freeform masking uses scanline spans instead of testing the full polygon per pixel.

## OS 캐시와 백신 영향 줄이기 / Reducing OS Cache and Antivirus Impact

Windows 캐시와 백신 검사는 완전히 제거할 수는 없습니다. 다만 아래 방법으로 변동을 줄일 수 있습니다.

Windows caching and antivirus scanning cannot be fully removed, but these steps can reduce variance.

- 빌드 폴더나 temp 폴더보다 고정 설치 폴더에서 실행하세요. / Run from a stable install folder instead of a build or temp folder.
- 릴리스 exe를 자주 다시 만들지 마세요. 해시가 바뀌면 백신이 다시 검사할 가능성이 큽니다. / Avoid constantly rebuilding the release exe; changed hashes are more likely to be rescanned.
- 배포용이면 코드 서명을 고려하세요. / Consider code signing for distribution.
- Defender 제외는 필요한 경우에만, 신뢰하는 exe 또는 설치 폴더 하나로 좁게 설정하세요. 전체 개발 폴더나 다운로드 폴더 제외는 권장하지 않습니다. / Use Defender exclusions only when necessary, scoped to the trusted exe or one install folder. Avoid broad exclusions for development or download folders.
- 더 빠른 체감이 필요하면 트레이 상주/pre-warm 모드를 추가하는 방식이 가장 확실합니다. / A resident tray/pre-warm mode is the strongest option if instant perceived startup is required.

## .NET Native Image 옵션 / .NET Native Image Option

.NET Framework 버전의 콜드 스타트를 더 줄이고 싶다면 관리자 PowerShell에서 NGen을 사용할 수 있습니다.

For the .NET Framework build, an elevated PowerShell can install a native image with NGen.

```powershell
.\build.ps1 -NativeImage
```

이 단계는 시스템 네이티브 이미지 캐시를 사용하므로 관리자 권한이 필요합니다.

This uses the system native image cache and requires administrator privileges.
