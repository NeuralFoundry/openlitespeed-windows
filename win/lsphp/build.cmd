@echo off
setlocal
rem usage: build.cmd <patched-litespeed-src> <php-devel-pack-dir> <php8ts.lib or php8.lib> <out-dir>
if "%~4"=="" (
    echo usage: build.cmd ^<src-dir^> ^<php-devel-pack-dir^> ^<php import lib^> ^<out-dir^>
    exit /b 2
)
set W=%~dp0
set SRC=%~1
set P=%~2
set PL=%~3
set O=%~4
call "%~dp0..\vsenv.cmd" || exit /b 1
set VSLANG=1033
set ZTS=
echo %~nx3 | findstr /i "php8ts" >nul && set ZTS=/DZTS=1
if not exist "%O%" mkdir "%O%"
set INC=/I"%SRC%" /I"%W%include" /I"%W%." /I"%P%\include" /I"%P%\include\main" /I"%P%\include\Zend" /I"%P%\include\TSRM" /I"%P%\include\ext"
set DEF=/D_WINDOWS /DWINDOWS=1 /DZEND_WIN32=1 /DPHP_WIN32=1 /DWIN32 /D_MBCS /D_USE_MATH_DEFINES %ZTS% /DZEND_DEBUG=0 /DNDEBUG /DENABLE_INTSAFE_SIGNED_FUNCTIONS /D_CRT_SECURE_NO_WARNINGS /D_WINSOCK_DEPRECATED_NO_WARNINGS
set CF=/nologo /c /MD /O2 /Zi /W3 /Zc:inline /guard:cf /wd4996 /wd4244 /wd4267 /wd4018 /wd4146 %DEF% %INC%
pushd "%O%"
cl %CF% /DLSPHP_IO_SHIM /FI"%W%lsphp_win.h" "%SRC%\lsapilib.c" || goto fail
cl %CF% /FI"%W%lsphp_win.h" "%SRC%\lsapi_main.c" || goto fail
cl %CF% "%W%lsphp_win.c" || goto fail
link /nologo /DEBUG /OPT:REF /GUARD:CF /OUT:lsphp.exe lsapilib.obj lsapi_main.obj lsphp_win.obj "%PL%" ws2_32.lib advapi32.lib || goto fail
popd
echo built %O%\lsphp.exe
exit /b 0
:fail
popd
exit /b 1
