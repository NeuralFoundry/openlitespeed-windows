@echo off
setlocal
set ROOT=%~dp0..
call "%~dp0vsenv.cmd" || exit /b 1
set VSLANG=1033
if "%JOBS%"=="" set JOBS=4
if "%BUILD%"=="" set BUILD=%ROOT%\build
if not exist "%BUILD%\build.ninja" cmake -S "%~dp0." -B "%BUILD%" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo || exit /b 1
ninja -C "%BUILD%" -k 0 -j%JOBS% %*
