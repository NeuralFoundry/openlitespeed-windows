@echo off
setlocal
set ROOT=%~dp0..
for %%L in (liblsquic lshpack) do (
  if not exist "%ROOT%\src\%%L\" (
    del /q "%ROOT%\src\%%L" 2>nul
    mklink /J "%ROOT%\src\%%L" "%ROOT%\lsquic\src\%%L" >nul || exit /b 1
  )
)
for %%H in (lsquic.h lsquic_types.h) do (
  fc /b "%ROOT%\lsquic\include\%%H" "%ROOT%\include\%%H" >nul 2>nul || (
    del /q "%ROOT%\include\%%H" 2>nul
    copy /y "%ROOT%\lsquic\include\%%H" "%ROOT%\include\%%H" >nul || exit /b 1
  )
)
exit /b 0
