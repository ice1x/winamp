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

## MVP scope — classic UI + local music (no network, no extra skins)

A frequent question is *"when can a minimal Winamp run on the Mac — just the classic
UI playing local files, with no internet features and no extra skins?"* This section
defines that MVP precisely against the roadmap so progress can be measured against it.

**In scope (the MVP critical path):**

| Stage | Tasks | Status |
|-------|-------|--------|
| Platform layer | 00005, 00007, 00008, **00023** (filereader / POSIX file I/O) | ~90% — only 00023 left |
| Build system | **00002**, **00003** (CMake to replace the `.sln`), 00006 (Carbon→Cocoa) | not started |
| arm64 dependencies | **00009** (drop x86-only Intel IPP), 00011 (drop DirectX 9), **00012** (mp3 codec for arm64) | not started |
| Audio engine | **00013** (build replicant core), **00014** (CoreAudio output), **00015** (MP3 decoder) | not started |
| → 🎯 **00022 — first sound** | headless CLI plays a local `.mp3` end-to-end | **first real checkpoint** |
| Library/UI glue | 00024 (tags for title/artist), 00026 (m3u/pls playlists) | not started |
| Classic UI | 00029 (UI tech decision), 00030 (main window/transport), 00031 (playlist window), **00033** (`.wsz` classic-skin loader), **00034** (render classic skin) | not started |
| → 🎯 **00039 — installable app** | `.app` bundle = **the MVP described above** | **MVP target** |

**Explicitly out of MVP scope** (deferred so they don't gate the MVP):

- Network: 00010 (OpenSSL), 00028 (HTTP / jnetlib / internet radio).
- Extra codecs: 00016–00019 (AAC, FLAC, Vorbis, WAV) — MVP ships MP3-only.
- Extra skins / theming beyond the single bundled classic skin.
- DSP/visuals: 00020 (ReplayGain), 00021/00035 (EQ), 00036 (visualizations).
- Library/metadata extras: 00025 (album art), 00027 (NDE database), 00032 (media library).
- OS integration & distribution: 00037 (media keys), 00038 (drag & drop), 00040–00041
  (signing/notarization/DMG) — needed to *ship*, not to *run locally*.

**Honest reading of the timeline.** Two checkpoints matter:

1. **First sound (00022)** — a headless player that actually decodes and plays a local
   MP3. This is the nearest meaningful "it works on the Mac" moment, and the right thing
   to aim for first.
2. **The MVP you described (≈00039 + 00033/00034)** — the classic skinned UI driving local
   playback.

Today (Phase 1 platform layer ~90% done) the work completed so far is the *smallest and most
self-contained* part of the port: thread/sync/string/atomic primitives with standalone unit
tests. The MVP critical path is still **~13–14 substantial tasks**, and the dominant risk/effort
sits in three of them: **00003** (standing up a whole-app CMake build for a ~1.7 GB Windows
codebase), **00009** (excising the x86-only Intel IPP DSP dependency on arm64), and
**00013 + 00033/00034** (getting the replicant core/codecs to actually compile, plus a
from-scratch classic-skin renderer). Those are each days-to-weeks of work, not afternoons like
the primitives done so far — so realistically *first sound* comes well before a *skinned MVP*,
and neither is "next week." The order to drive toward it: finish **00023**, then **00003**,
then the 00009→00012→00013→00014→00015 chain to hit **00022**, then the Phase 5 UI subset to
hit **00039**.

## Tasks

### Phase 0 — Infrastructure and assessment
- [ ] 00001 Perform a full audit of Windows API dependencies (windows.h, _WIN32, MFC, COM, ATL) and map modules by portability
- [ ] 00002 Set up the macOS toolchain (Xcode Command Line Tools, clang, cmake, ninja, pkg-config, Homebrew dependencies)
- [ ] 00003 Introduce a cross-platform build system (CMake) to replace winampAll_2019.sln
- [ ] 00004 Set up a CI pipeline for building and testing on arm64 macOS

### Phase 1 — Platform layer (nx / bfc)
- [x] 00005 Add an osx-arm64 target to nx (Makefile/xcodeproj instead of osx-amd64) — `nx/Makefile` now detects `uname -m` and selects `osx-arm64`; the `foundation/atomics.h`, `foundation/types.h`, `nu/LockFreeLIFO.h` and `nx/nxonce.h` arch dispatchers route Apple Silicon to new `osx-arm64/` sources instead of `#error Port Me!`.
- [ ] 00006 Rewrite the dead Carbon code in bfc/platform on Cocoa/POSIX
- [ ] 00007 Implement nx primitives on macOS (threads, mutexes, semaphores, condition variables, files, time, sleep) via POSIX/GCD — *partial:* arm64 atomics (`foundation/osx-arm64/atomics.h`), `NXOnce` (`nx/osx-arm64/nxonce.c`) and the lock-free LIFO (`nu/osx-arm64/LockFreeLIFO.h`) are implemented and unit-tested. **Threads** (`nx/osx/nxthread.c`, pthread + return-value trampoline), **semaphores** (`nx/osx/nxsemaphore.c`, GCD `dispatch_semaphore` since macOS `sem_init` is a non-functional stub), **condition variables / mutexes** (`nx/osx/nxcondition.c`, `pthread_cond_t` + `pthread_mutex_t`) and **sleep/yield** (`nx/osx/nxsleep.c`, `nanosleep`/`sched_yield`) are now implemented and unit-tested (`Src/replicant/tests/osx-arm64/run_tests.sh`). The `nx_time_unix_64_t` typedef has an `osx/nxtime.h`; **files** (filereader/`nxfile`) remain pending — tracked under task 00023.
- [x] 00008 Port the string layer (UTF-16 ↔ UTF-8, wchar_t, OSFNCHAR) for macOS — `nx/osx/nxstring.{h,c}` and `nx/osx/nxmutablestring.{h,c}`. Win32 `wchar_t` is a 16-bit UTF-16 code unit; macOS `wchar_t` is 32-bit (UTF-32), so the port stores text internally as explicit 16-bit UTF-16 units (`nx_utf16_t`, matching `foundation/osx-arm64/types.h` `ns_char_t`) — keeping the binary `nx_charset_utf16le` contract identical to Windows — and carries self-contained UTF-8 ↔ UTF-16 ↔ UTF-32 transcoders (with surrogate-pair handling) plus ascii/latin1/utf16le/utf16be conversion, replacing `MultiByteToWideChar`/`WideCharToMultiByte`/`CompareString`/`PathCombineW`/the Win32 process heap. The `__APPLE__` dispatchers (`nx/nxstring.h`, `nx/nxmutablestring.h` — typo `__APPLE_` fixed) route Apple Silicon to these sources. Unit-tested in `Src/replicant/tests/osx-arm64/{test_string,test_mutablestring}.c` (10 osx-arm64 tests total pass; both sources compile clean under `-Wall -Wextra -Werror`).

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

## Building & testing the macOS (arm64) platform layer

The arm64 platform primitives ported so far have a standalone unit-test suite
that builds with the Xcode command-line tools (no Visual Studio required):

```sh
cd Src/replicant
sh tests/osx-arm64/run_tests.sh
```

Each test is compiled with `-arch arm64`, which is what drives the macro
dispatch in the `foundation`/`nu`/`nx` headers to select the `osx-arm64`
sources. The suite covers:

- `test_types` — fixed-width integer / colour / `GUID` typedefs and their sizes.
- `test_atomics` — `nx_atomic_*` semantics and atomicity under thread contention.
- `test_lifo` — the `lifo_*` stack (single-threaded ordering + concurrent
  producer/consumer integrity).
- `test_nxonce` — `NXOnce` runs its initializer exactly once across racing
  threads, with acquire/release visibility (the naive flag check that the Win32
  version uses is unsafe on weakly-ordered arm64).
- `test_sleep` — `NXSleep` blocks for at least the requested duration and
  `NXSleepYield` returns without hanging.
- `test_semaphore` — counting-semaphore semantics: concurrent producer/consumer
  permit accounting and that `NXSemaphoreWait` genuinely blocks until a
  `NXSemaphoreRelease`.
- `test_condition` — `NXConditionSignal` wakes a thread blocked in
  `NXConditionWait`, and `NXConditionTimedWait` times out instead of hanging.
- `test_thread` — `NXThreadCreate` passes the parameter through and
  `NXThreadJoin` returns the worker's value, single- and multi-threaded.

> The lock-free LIFO is currently a correct mutex-guarded fallback; a genuinely
> lock-free arm64 implementation is tracked under task 00045.