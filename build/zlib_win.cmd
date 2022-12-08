@echo off
setlocal enabledelayedexpansion
pushd "%~dp0"

cd ..
if not exist bin mkdir bin
if not exist bin goto ERROR1
cd bin

set CMKFILE=CMakeLists.txt
set ORGDIR=..\submodules\zlib
set ORGFILE=%ORGDIR%\%CMKFILE%
copy /y %ORGFILE% .\
del %ORGFILE%

for /f "tokens=* delims=0123456789 eol=" %%a in ('findstr /n "^" %CMKFILE%') do (
    set b=%%a
    set b=!b:~1!
    (echo.!b!) >> %ORGFILE%
    set c=!b:"=!
    if /i "!c!"=="project(zlib C)" (
        (echo.if^(MSVC^)) >> %ORGFILE%
        (echo.    string^(REPLACE "/MD" "/MT" CMAKE_C_FLAGS_DEBUG            ${CMAKE_C_FLAGS_DEBUG}^)) >> %ORGFILE%
        (echo.    string^(REPLACE "/MD" "/MT" CMAKE_C_FLAGS_MINSIZEREL       ${CMAKE_C_FLAGS_MINSIZEREL}^)) >> %ORGFILE%
        (echo.    string^(REPLACE "/MD" "/MT" CMAKE_C_FLAGS_RELEASE          ${CMAKE_C_FLAGS_RELEASE}^)) >> %ORGFILE%
        (echo.    string^(REPLACE "/MD" "/MT" CMAKE_C_FLAGS_RELWITHDEBINFO   ${CMAKE_C_FLAGS_RELWITHDEBINFO}^)) >> %ORGFILE%
        (echo.endif^(MSVC^)) >> %ORGFILE%
    )
)

call :BUILD cl "Visual Studio 16 2019"
if exist zlib\Release\zlibstatic.lib copy /y zlib\Release\zlibstatic.lib zlibstatic.lib
@gcc -v > NUL 2>&1
if ERRORLEVEL 1 goto END
call :BUILD gcc "MinGW Makefiles" _gcc
if exist zlib_gcc\libzlibstatic.a copy /y zlib_gcc\libzlibstatic.a libzlibstatic.a

:END
if exist .\%CMKFILE% copy .\%CMKFILE% %ORGDIR%\
move %ORGDIR%\zconf.h.included %ORGDIR%\zconf.h

popd
endlocal
exit /b 0

:ERROR1
echo Error occurred.
popd
endlocal
exit /b 0

:BUILD
set CC=%1
set GEN=%2
set TGT=%3
echo Generating with %CC% and %GEN% ...
if not exist zlib%TGT% mkdir zlib%TGT%
cd zlib%TGT%

cmake -DCMAKE_INSTALL_PREFIX=../../src/template/inc/lib/zlib ..\%ORGDIR% -G %GEN%
if "%CC%"=="cl" (
    msbuild /p:Configuration=Release zlib.sln
    msbuild /p:Configuration=Release INSTALL.vcxproj
) else (
    mingw32-make -f Makefile zlibstatic
)

cd ..
exit /b 0
