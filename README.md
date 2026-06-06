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

# Портирование на macOS 14.8.3 (23J220), Apple Silicon M1

## Оценка проекта

Кодовая база Winamp — это крупный Windows-проект на C++ (~1.7 GB, ~3138 `.cpp`, ~4811 `.h`,
~1382 `.c`), исторически собираемый только в Visual Studio 2019.

Ключевые препятствия для запуска на macOS / ARM64:

- **Windows API везде.** `windows.h` подключается в ~1004 файлах, `_WIN32` встречается в ~1339,
  есть зависимость от MFC. Требуется слой совместимости или замена.
- **UI на движке Wasabi** (собственный скин-движок поверх Win32 GDI / DirectX 9) — нет
  кроссплатформенной реализации, нужен новый нативный UI-слой.
- **Вывод звука** через DirectSound (`out_ds`) и WASAPI (`out_wasapi`) — на macOS нужен
  бэкенд на CoreAudio / AudioUnit.
- **Ввод/декодирование** частично через DirectShow (`in_dshow`) — несовместимо с macOS.
- **Intel IPP 6.1.1.035 — только x86**, не работает на Apple Silicon. Критичная зависимость
  DSP/кодеков, требует замены (Accelerate/vDSP или софт-реализация).
- **Устаревшие зависимости**: OpenSSL 1.0.1u, DirectX 9 SDK (June 2010).
- **Система сборки** — `.sln` Visual Studio, нет CMake/Xcode-сборки для приложения целиком.

Положительное:

- Есть кроссплатформенное ядро **`replicant`** (аудио-интерфейсы `ifc_*`, декодеры, плеер) и
  платформенный слой **`nx`** с Makefile-таргетом `Darwin` и `.xcodeproj` для ряда модулей
  (`Wasabi`, `bfc`, `png`, `alac`, `nde`, `playlist`, `xml`, `tataki` и др.).
- Существует старая заготовка под macOS в `bfc/platform` (на **Carbon** — устарела и удалена из
  современной macOS, её придётся переписать на Cocoa/POSIX).
- Таргет `nx` называется `osx-amd64` — нужно завести `osx-arm64`.

**Стратегия:** идти снизу вверх — сначала собрать кроссплатформенное ядро и кодеки под arm64
с выводом на CoreAudio (рабочий headless-плеер), затем построить нативный UI и постепенно
вернуть скины/визуализации.

## Задачи

### Фаза 0 — Инфраструктура и оценка
- [ ] 00001 Провести полный аудит зависимостей от Windows API (windows.h, _WIN32, MFC, COM, ATL) и составить карту модулей по степени портируемости
- [ ] 00002 Установить toolchain на macOS (Xcode Command Line Tools, clang, cmake, ninja, pkg-config, Homebrew-зависимости)
- [ ] 00003 Завести кроссплатформенную систему сборки (CMake) как замену winampAll_2019.sln
- [ ] 00004 Настроить CI-пайплайн для сборки и тестов на arm64 macOS

### Фаза 1 — Платформенный слой (nx / bfc)
- [ ] 00005 Добавить таргет osx-arm64 в nx (Makefile/xcodeproj вместо osx-amd64)
- [ ] 00006 Переписать мёртвый Carbon-код в bfc/platform на Cocoa/POSIX
- [ ] 00007 Реализовать примитивы nx под macOS (потоки, мьютексы, семафоры, условные переменные, файлы, время, сон) через POSIX/GCD
- [ ] 00008 Портировать строковый слой (UTF-16 ↔ UTF-8, wchar_t, OSFNCHAR) под macOS

### Фаза 2 — Замена сторонних зависимостей
- [ ] 00009 Удалить/заменить Intel IPP 6.1.1.035 (x86-only) на arm64-совместимое решение (Accelerate/vDSP или софт)
- [ ] 00010 Заменить OpenSSL 1.0.1u на современный OpenSSL 3 / LibreSSL / SecureTransport
- [ ] 00011 Устранить зависимости от DirectX 9 SDK
- [ ] 00012 Собрать кодек-библиотеки под arm64 (mpg123, libvpx, vorbis, flac, alac, ogg)

### Фаза 3 — Аудио-движок (ядро replicant)
- [ ] 00013 Собрать кроссплатформенное ядро replicant под arm64 (ifc_audio*, decode, player)
- [ ] 00014 Реализовать output-плагин на CoreAudio/AudioUnit (замена out_ds / out_wasapi)
- [ ] 00015 Портировать декодер MP3 (in_mp3 / mpg123)
- [ ] 00016 Портировать декодер AAC/MP4
- [ ] 00017 Портировать декодер FLAC
- [ ] 00018 Портировать декодер Vorbis
- [ ] 00019 Портировать WAV/AIFF/PCM
- [ ] 00020 Портировать ReplayGain analysis
- [ ] 00021 Портировать эквалайзер/DSP без зависимости от IPP
- [ ] 00022 Собрать рабочий headless-плеер (CLI) для проверки сквозного воспроизведения

### Фаза 4 — Файловый, медиа- и сетевой слой
- [ ] 00023 Портировать filereader (file I/O, потоки) на POSIX
- [ ] 00024 Портировать чтение/запись метаданных и тегов (id3v2, apev2, tagz)
- [ ] 00025 Портировать извлечение обложек (album art)
- [ ] 00026 Портировать работу с плейлистами (m3u, pls, xspf)
- [ ] 00027 Портировать NDE (Nullsoft Database Engine) под arm64
- [ ] 00028 Портировать сетевой слой HTTP/jnetlib (стриминг, интернет-радио)

### Фаза 5 — Пользовательский интерфейс
- [ ] 00029 Спроектировать новый UI-слой для macOS (нативный Cocoa/SwiftUI или Qt) и зафиксировать решение
- [ ] 00030 Реализовать главное окно плеера (play/pause/stop/seek/volume/transport)
- [ ] 00031 Реализовать окно плейлиста
- [ ] 00032 Реализовать менеджер медиатеки
- [ ] 00033 Загрузчик классических скинов (.wsz) — разбор архива и ресурсов
- [ ] 00034 Рендер классических bitmap-скинов на CoreGraphics/Metal
- [ ] 00035 Окно эквалайзера (UI)
- [ ] 00036 Визуализации (спектр/осциллограф) на Metal

### Фаза 6 — Интеграция с macOS и упаковка
- [ ] 00037 Поддержка медиа-клавиш и Now Playing (MPRemoteCommandCenter / MPNowPlayingInfoCenter)
- [ ] 00038 Drag&drop, ассоциации файлов и «Открыть с помощью»
- [ ] 00039 Сборка .app-бандла (Info.plist, иконки, ресурсы)
- [ ] 00040 Подпись кода (codesign) и нотаризация для распространения на macOS
- [ ] 00041 Сборка DMG/инсталлятора

### Фаза 7 — Качество и сопровождение
- [ ] 00042 Юнит-тесты аудио-движка
- [ ] 00043 Интеграционные тесты воспроизведения по форматам
- [ ] 00044 Прогон тестов на M1/arm64 в CI
- [ ] 00045 Профилирование и оптимизация под Apple Silicon (NEON/Accelerate)
- [ ] 00046 Обновить документацию сборки для macOS