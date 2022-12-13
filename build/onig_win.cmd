@echo off
setlocal enabledelayedexpansion
pushd "%~dp0"

cd ..
if not exist bin mkdir bin
if not exist bin goto ERROR1
cd bin

set CMKFILE=CMakeLists.txt
set ORGDIR=..\submodules\oniguruma

call :BUILD cl "Visual Studio 16 2019"
if exist onig\Release\onig.lib copy /y onig\Release\onig.lib onig.lib
@gcc -v > NUL 2>&1
if ERRORLEVEL 1 goto END
call :BUILD gcc "MinGW Makefiles" _gcc
if exist onig_gcc\libonig.a copy /y onig_gcc\libonig.a libonig.a

:END

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
if not exist onig%TGT% mkdir onig%TGT%
cd onig%TGT%

if "%CC%"=="cl" (
    cmake -DCMAKE_INSTALL_PREFIX=../../src/template/inc/lib/onig -DMSVC_STATIC_RUNTIME=ON -DBUILD_SHARED_LIBS=OFF -DBUILD_TEST=OFF ..\%ORGDIR% -G %GEN%
    msbuild /p:Configuration=Release oniguruma.sln
    msbuild /p:Configuration=Release INSTALL.vcxproj
) else (
    cmake -DCMAKE_INSTALL_PREFIX=../../src/template/inc/lib/onig -DBUILD_SHARED_LIBS=OFF -DBUILD_TEST=OFF ..\%ORGDIR% -G %GEN%
    mingw32-make -f Makefile
)

cd ..
exit /b 0
