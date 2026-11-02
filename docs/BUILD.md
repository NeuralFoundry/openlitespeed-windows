# Building from source

The release installer is built with the steps below. A full build takes about 20 minutes on four cores.

## Requirements

- Windows 10, Windows 11 or Windows Server 2019 or newer, x64
- Visual Studio 2022 (Build Tools, Community or any other edition) with the **Desktop development with C++** workload. CMake and Ninja are included in that workload.
- [Git for Windows](https://git-scm.com/download/win). Its Perl is used to generate part of the QUIC library.
- PowerShell 5.1 or newer and an internet connection for the dependency sources
- About 8 GB of free disk space

## Steps

Run the commands from a normal command prompt. Clone into a short folder such as `C:\src`, because some dependency sources have long paths. The build scripts locate Visual Studio themselves.

```bat
git -c core.autocrlf=false clone --recurse-submodules https://github.com/NeuralFoundry/openlitespeed-windows.git
cd openlitespeed-windows

powershell -ExecutionPolicy Bypass -File win\deps\fetch-deps.ps1
win\deps\build-deps.cmd
win\prepare.cmd
win\build.cmd

powershell -ExecutionPolicy Bypass -File win\lsphp\fetch-php.ps1
powershell -ExecutionPolicy Bypass -File win\lsphp\build-all.ps1

powershell -ExecutionPolicy Bypass -File win\package.ps1
```

| Step | Result |
|---|---|
| `fetch-deps.ps1` | BoringSSL, Brotli, Expat, PCRE2, zlib, udns, libbcrypt, libmaxminddb, the IP2Location C library and LuaJIT at pinned commits in `.deps\`. BoringSSL gets the OpenLiteSpeed TLS patch from `win\deps\patches\`. |
| `build-deps.cmd` | Static libraries in `.deps\prefix\` |
| `prepare.cmd` | Links the QUIC and HPACK sources from the `lsquic` submodule into `src\` |
| `build.cmd` | `build\openlitespeed.exe`, `olstray.exe`, `setup.exe` and `olspack.exe` |
| `fetch-php.ps1` | PHP development packs for PHP 8.2 to 8.5, checked against the SHA-256 sums published by windows.php.net, and the LiteSpeed SAPI sources from the matching php-src tags |
| `build-all.ps1` | `build\lsphp-<version>-<ts\|nts>\lsphp.exe` for every PHP version and build type |
| `package.ps1` | `build\OpenLiteSpeed-<version>-win64-setup.exe` and a zip of the same files |

`core.autocrlf=false` matters: the QUIC library generates code with Perl scripts that fail on Windows line endings.

## Build options

| Variable | Default | Meaning |
|---|---|---|
| `JOBS` | `4` | Parallel compile jobs for `build.cmd` and `build-deps.cmd` |
| `BUILD` | `build` | Output folder for `build.cmd` |

CMake option `-DOLS_POISON_NEW=ON` fills new and freed memory with a pattern, which makes use of uninitialized fields easy to spot during testing.

## Running a development build

```bat
xcopy /e /i build\OpenLiteSpeed-1.9.4-win64 C:\ols-dev
powershell -ExecutionPolicy Bypass -File C:\ols-dev\install.ps1 -Mode standalone -HttpPort 8088 -AdminPort 7081
C:\ols-dev\bin\openlitespeed.exe -d
```

`-d` keeps the server in the foreground. Logs are in `C:\ols-dev\logs\`. A crash writes a symbolized stack trace to `logs\stderr.log` and a minidump.

## Source layout

| Path | Contents |
|---|---|
| `win\compat\` | POSIX compatibility layer, see [ARCHITECTURE.md](ARCHITECTURE.md) |
| `win\lsphp\` | Windows support for the PHP LiteSpeed SAPI and its build scripts |
| `win\setup\` | `setup.exe` installer and `olspack.exe` packer |
| `win\tray\` | Tray application |
| `win\common\` | PHP, Laragon and XAMPP detection shared by the installer and the tray |
| `win\package\` | Default configuration and `install.ps1` |
| `win\deps\` | Dependency fetch and build scripts |
| `src\`, `include\`, `dist\` | OpenLiteSpeed sources with the Windows changes |
