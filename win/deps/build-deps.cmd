@echo off
setlocal
set ROOT=%~dp0..\..
set D=%ROOT%\.deps
set P=%D%\prefix
call "%~dp0..\vsenv.cmd" || exit /b 1
set C=-G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_DEFAULT_CMP0091=NEW -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded -DCMAKE_INSTALL_PREFIX=%P% -DBUILD_SHARED_LIBS=OFF
if "%JOBS%"=="" set JOBS=4
if not exist "%D%\build" mkdir "%D%\build"

call :dep zlib "-DZLIB_BUILD_TESTING=OFF -DZLIB_BUILD_SHARED=OFF" || exit /b 1
call :dep pcre2 "-DPCRE2_BUILD_PCRE2_8=ON -DPCRE2_SUPPORT_JIT=ON -DPCRE2_SUPPORT_UNICODE=ON -DPCRE2_MATCH_LIMIT=100000 -DPCRE2_BUILD_PCRE2GREP=OFF -DPCRE2_BUILD_TESTS=OFF -DPCRE2_STATIC_RUNTIME=ON" || exit /b 1
call :dep libexpat\expat "-DEXPAT_SHARED_LIBS=OFF -DEXPAT_MSVC_STATIC_CRT=ON -DEXPAT_BUILD_TESTS=OFF -DEXPAT_BUILD_TOOLS=OFF -DEXPAT_BUILD_EXAMPLES=OFF -DEXPAT_BUILD_DOCS=OFF" || exit /b 1
call :dep brotli "-DBROTLI_DISABLE_TESTS=ON -DBROTLI_BUILD_TOOLS=OFF" || exit /b 1
call :dep udns "-DUDNS_SRC=%D:\=/%/udns" "%~dp0udns" || exit /b 1
call :dep libmaxminddb "-DBUILD_TESTING=OFF -DMAXMINDDB_BUILD_BINARIES=OFF -DMSVC_STATIC_RUNTIME=ON" || exit /b 1
call :dep ip2location "-DIP2LOC_SRC=%D:\=/%/ip2location" "%~dp0ip2location" || exit /b 1

echo [boringssl]
cmake -S %D%\boringssl -B %D%\build\boringssl %C% -DOPENSSL_NO_ASM=1 -DBUILD_TESTING=OFF >%D%\build\boringssl.log 2>&1 || goto fail
ninja -C %D%\build\boringssl -j%JOBS% ssl crypto decrepit >>%D%\build\boringssl.log 2>&1 || goto fail
if not exist %P%\lib mkdir %P%\lib
copy /y %D%\build\boringssl\ssl.lib %P%\lib\ >nul
copy /y %D%\build\boringssl\crypto.lib %P%\lib\ >nul
copy /y %D%\build\boringssl\decrepit.lib %P%\lib\ >nul
xcopy /e /i /y /q %D%\boringssl\include\openssl %P%\include\openssl >nul
echo deps ready in %P%
exit /b 0

:dep
echo [%~1]
set SRCDIR=%D%\%~1
if not "%~3"=="" set SRCDIR=%~3
set B=%D%\build\%~n1
cmake -S "%SRCDIR%" -B "%B%" %C% %~2 >"%B%.log" 2>&1 || goto fail
ninja -C "%B%" -j%JOBS% >>"%B%.log" 2>&1 || goto fail
cmake --install "%B%" >>"%B%.log" 2>&1 || goto fail
exit /b 0

:fail
echo FAILED, see %D%\build
exit /b 1
