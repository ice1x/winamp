# Winamp

## About

Winamp is a multimedia player launched in 1997, iconic for its flexibility and wide compatibility with audio formats. Originally developed by Nullsoft, it gained massive popularity with still millions of users. Its development slowed down, but now, its source code was opened to the community, allowing developers to improve and modernize the playerto meet current user needs.

## Usage

Building of the Winamp desktop client is current based around Visual Studio 2019 (VS2019) and Intel IPP libs (You need to use exactly v6.1.1.035).
There are differnt options of how to build Winamp:

1. Use a build_winampAll_2019.cmd script file that makes 4 versions x86/x64 (Debug and Release). In this case Visual Studio IDE not running.
2. Use a winampAll_2019.sln file to build and debug in Visual Studio IDE.

### Dependencies

#### libvpx
We take libvpx from https://github.com/ShiftMediaProject/libvpx, modify it and pack to archive.
Run unpack_libvpx_v1.8.2_msvc16.cmd to unpack.

#### libmpg123
We take libmpg123 from https://www.mpg123.de/download.shtml, modify it and pack to archive.
Run unpack_libmpg123.cmd to unpack and process dlls.

#### OpenSSL
You need to use openssl-1.0.1u. For that you need to build a static version of these libs.
Run build_vs_2019_openssl_x86.cmd and build_vs_2019_openssl_64.cmd.

To build OpenSSL you need to install

7-Zip, NASM and Perl.

#### DirectX 9 SDK 
We take DirectX 9 SDK (June 2010) from Microsoft, modify it and pack to archive.
Run unpack_microsoft_directx_sdk_2010.cmd to unpack it.

#### Microsoft ATLMFC lib fix
In file C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Tools\MSVC\14.24.28314\atlmfc\include\atltransactionmanager.h

goto line 427 and change from 'return ::DeleteFile((LPTSTR)lpFileName);' to 'return DeleteFile((LPTSTR)lpFileName);'

#### Intel IPP 6.1.1.035
We take Intel IPP 6.1.1.035, modify it and pack to archive.
Run unpack_intel_ipp_6.1.1.035.cmd to unpack it.

---

# Porting to macOS 14.8.3 (23J220), Apple Silicon M1

## Project assessment

The Winamp codebase is a large Windows C++ project (~1.7 GB, ~3138 `.cpp`, ~4811 `.h`,
~1382 `.c`), historically built only with Visual Studio 2019.

Key blockers for running on macOS / ARM64:

- **Windows API everywhere.** `windows.h` is included in ~1004 files, `_WIN32` appears in
  ~1339, and there is an MFC dependency. A compatibility layer or replacement is required.
- **UI on the Wasabi engine** (a custom skinning engine on top of Win32 GDI / DirectX 9) —
  there is no cross-platform implementation; a new native UI layer is needed.
- **Audio output** via DirectSound (`out_ds`) and WASAPI (`out_wasapi`) — macOS needs a
  CoreAudio / AudioUnit backend.
- **Input/decoding** partly via DirectShow (`in_dshow`) — incompatible with macOS.
- **Intel IPP 6.1.1.035 is x86-only** and does not run on Apple Silicon. It is a critical
  DSP/codec dependency and must be replaced (Accelerate/vDSP or a software implementation).
- **Outdated dependencies**: OpenSSL 1.0.1u, DirectX 9 SDK (June 2010).
- **Build system** is a Visual Studio `.sln`; there is no CMake/Xcode build for the whole app.

On the positive side:

- There is a cross-platform core, **`replicant`** (audio interfaces `ifc_*`, decoders, player),
  and a platform layer, **`nx`**, with a `Darwin` Makefile target and `.xcodeproj` files for
  several modules (`Wasabi`, `bfc`, `png`, `alac`, `nde`, `playlist`, `xml`, `tataki`, etc.).
- There is old macOS scaffolding in `bfc/platform` (built on **Carbon** — deprecated and removed
  from modern macOS, so it must be rewritten on Cocoa/POSIX).
- The `nx` target is named `osx-amd64` — an `osx-arm64` target needs to be added.

**Strategy:** work bottom-up — first build the cross-platform core and codecs for arm64 with a
CoreAudio output (a working headless player), then build a native UI and gradually bring back
skins/visualizations.

**Key milestones:**
- **00022** — first audio: a headless CLI player that actually plays a local file.
- **00039** — first installable app: an `.app` bundle with a window you can drop into `/Applications`.
- **00040–00041** — signed/notarized DMG suitable for distribution.

## Tasks

### Phase 0 — Infrastructure and assessment
- [ ] 00001 Perform a full audit of Windows API dependencies (windows.h, _WIN32, MFC, COM, ATL) and map modules by portability
- [ ] 00002 Set up the macOS toolchain (Xcode Command Line Tools, clang, cmake, ninja, pkg-config, Homebrew dependencies)
- [ ] 00003 Introduce a cross-platform build system (CMake) to replace winampAll_2019.sln
- [ ] 00004 Set up a CI pipeline for building and testing on arm64 macOS

### Phase 1 — Platform layer (nx / bfc)
- [ ] 00005 Add an osx-arm64 target to nx (Makefile/xcodeproj instead of osx-amd64)
- [ ] 00006 Rewrite the dead Carbon code in bfc/platform on Cocoa/POSIX
- [ ] 00007 Implement nx primitives on macOS (threads, mutexes, semaphores, condition variables, files, time, sleep) via POSIX/GCD
- [ ] 00008 Port the string layer (UTF-16 ↔ UTF-8, wchar_t, OSFNCHAR) for macOS

### Phase 2 — Replace third-party dependencies
- [ ] 00009 Remove/replace Intel IPP 6.1.1.035 (x86-only) with an arm64-compatible solution (Accelerate/vDSP or software)
- [ ] 00010 Replace OpenSSL 1.0.1u with modern OpenSSL 3 / LibreSSL / SecureTransport
- [ ] 00011 Remove DirectX 9 SDK dependencies
- [ ] 00012 Build codec libraries for arm64 (mpg123, libvpx, vorbis, flac, alac, ogg)

### Phase 3 — Audio engine (replicant core)
- [ ] 00013 Build the cross-platform replicant core for arm64 (ifc_audio*, decode, player)
- [ ] 00014 Implement a CoreAudio/AudioUnit output plugin (replacing out_ds / out_wasapi)
- [ ] 00015 Port the MP3 decoder (in_mp3 / mpg123)
- [ ] 00016 Port the AAC/MP4 decoder
- [ ] 00017 Port the FLAC decoder
- [ ] 00018 Port the Vorbis decoder
- [ ] 00019 Port WAV/AIFF/PCM
- [ ] 00020 Port ReplayGain analysis
- [ ] 00021 Port the equalizer/DSP without depending on IPP
- [ ] 00023 Port filereader (file I/O, streams) to POSIX — required to read local files for playback
- [ ] 00022 🎯 **Milestone: first audio.** Build a working headless player (CLI) to verify end-to-end playback (depends on 00002–00015 and 00023)

### Phase 4 — Media and network layer
- [ ] 00024 Port metadata/tag reading and writing (id3v2, apev2, tagz)
- [ ] 00025 Port album art retrieval
- [ ] 00026 Port playlist handling (m3u, pls, xspf)
- [ ] 00027 Port NDE (Nullsoft Database Engine) for arm64
- [ ] 00028 Port the HTTP/jnetlib network layer (streaming, internet radio)

### Phase 5 — User interface
- [ ] 00029 Design the new macOS UI layer (native Cocoa/SwiftUI or Qt) and commit to a decision
- [ ] 00030 Implement the main player window (play/pause/stop/seek/volume/transport)
- [ ] 00031 Implement the playlist window
- [ ] 00032 Implement the media library manager
- [ ] 00033 Classic skin (.wsz) loader — parse the archive and resources
- [ ] 00034 Render classic bitmap skins on CoreGraphics/Metal
- [ ] 00035 Equalizer window (UI)
- [ ] 00036 Visualizations (spectrum/oscilloscope) on Metal

### Phase 6 — macOS integration and packaging
- [ ] 00037 Media keys and Now Playing support (MPRemoteCommandCenter / MPNowPlayingInfoCenter)
- [ ] 00038 Drag & drop, file associations, and "Open With"
- [ ] 00039 🎯 **Milestone: installable app.** Build the .app bundle (Info.plist, icons, resources) — first version you can drop into /Applications and use with a window
- [ ] 00040 Code signing (codesign) and notarization for macOS distribution
- [ ] 00041 Build a DMG/installer

### Phase 7 — Quality and maintenance
- [ ] 00042 Unit tests for the audio engine
- [ ] 00043 Integration tests for playback across formats
- [ ] 00044 Run tests on M1/arm64 in CI
- [ ] 00045 Profiling and optimization for Apple Silicon (NEON/Accelerate)
- [ ] 00046 Update the build documentation for macOS