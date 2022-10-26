@echo off
setlocal enabledelayedexpansion
pushd "%~dp0"

cd ..
if not exist bin mkdir bin
if not exist bin goto ERROR1
cd bin

if not exist mir_static.lib call ..\build\mir_vs2019.cmd
if not exist mir_static.lib goto ERROR

cl /Fekilite.exe ^
    ..\src\main.c ^
    ..\src\frontend\lexer.c ^
    ..\src\frontend\parser.c ^
    ..\src\frontend\error.c ^
    ..\src\frontend\dispast.c ^
    ..\src\frontend\mkkir.c ^
    ..\src\backend\dispkir.c ^
    ..\src\backend\translate.c

copy /y kilite.exe ..\kilite.exe

:ERROR
popd
endlocal
exit /b 0

:END
popd
endlocal
exit /b 0
