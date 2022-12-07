@echo off
setlocal enabledelayedexpansion
pushd "%~dp0"

cd ..
if not exist bin mkdir bin
if not exist bin goto ERROR1
cd bin

if not exist zlib (
    echo Build Zlib first.
    exit /b 1
)
if not exist minizip mkdir minizip
set CMKFILE=CMakeLists.txt
set ORGDIR=..\..\submodules\minizip-ng
set ORGFILE=%ORGDIR%\%CMKFILE%
cd minizip
copy /y %ORGFILE% .\
del %ORGFILE%

for /f "tokens=* delims=0123456789 eol=" %%a in ('findstr /n "^" %CMKFILE%') do (
    set b=%%a
    set b=!b:~1!
    (echo.!b!) >> %ORGFILE%
    set c=!b:"=!
    if /i "!c!"=="enable_language(C)" (
        (echo.set^(ZLIB_ROOT ../zlib/install^)) >> %ORGFILE%
        (echo.set^(MZ_BZIP2 OFF^)) >> %ORGFILE%
        (echo.set^(MZ_LZMA OFF^)) >> %ORGFILE%
        (echo.set^(MZ_ZSTD OFF^)) >> %ORGFILE%
    )
    if /i "!c!"=="project(minizip${MZ_PROJECT_SUFFIX} LANGUAGES C VERSION ${VERSION})" (
        (echo.if^(MSVC^)) >> %ORGFILE%
        (echo.    string^(REPLACE "/MD" "/MT" CMAKE_C_FLAGS_DEBUG            ${CMAKE_C_FLAGS_DEBUG}^)) >> %ORGFILE%
        (echo.    string^(REPLACE "/MD" "/MT" CMAKE_C_FLAGS_MINSIZEREL       ${CMAKE_C_FLAGS_MINSIZEREL}^)) >> %ORGFILE%
        (echo.    string^(REPLACE "/MD" "/MT" CMAKE_C_FLAGS_RELEASE          ${CMAKE_C_FLAGS_RELEASE}^)) >> %ORGFILE%
        (echo.    string^(REPLACE "/MD" "/MT" CMAKE_C_FLAGS_RELWITHDEBINFO   ${CMAKE_C_FLAGS_RELWITHDEBINFO}^)) >> %ORGFILE%
        (echo.endif^(MSVC^)) >> %ORGFILE%
    )
)

cmake -DCMAKE_INSTALL_PREFIX=.\install %ORGDIR% -G "Visual Studio 16 2019"
msbuild /p:Configuration=Release minizip.sln
if exist Release\libminizip.lib copy /y Release\libminizip.lib ..\libminizip.lib

:END
if exist .\%CMKFILE% copy .\%CMKFILE% %ORGDIR%\

popd
endlocal
exit /b 0

:ERROR1
echo Error occurred.
popd
endlocal
exit /b 0
