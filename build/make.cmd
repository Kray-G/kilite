@echo off
setlocal enabledelayedexpansion
pushd "%~dp0"

cd ..
if not exist bin mkdir bin
if not exist bin goto ERROR1
cd bin

if not exist mir_static.lib call ..\build\mir_vs2019.cmd
if not exist mir_static.lib goto ERROR

set TEMPF=h.txt
cl -O2 ..\util\makecstr.c
type ..\src\template\lib\bign.h > %TEMPF%
type ..\src\template\lib\bigz.h >> %TEMPF%
type ..\src\template\header.h >> %TEMPF%
makecstr.exe %TEMPF% > ..\src\backend\header.c
del %TEMPF%

cl -O2 -I ..\..\mir ^
    /Fekilite.exe ^
    ..\src\main.c ^
    ..\src\frontend\lexer.c ^
    ..\src\frontend\parser.c ^
    ..\src\frontend\error.c ^
    ..\src\frontend\dispast.c ^
    ..\src\frontend\mkkir.c ^
    ..\src\backend\dispkir.c ^
    ..\src\backend\translate.c ^
    ..\src\backend\resolver.c ^
    ..\src\backend\header.c ^
    ..\src\backend\cexec.c ^
    ..\bin\mir_static.lib

copy /y kilite.exe ..\kilite.exe

:ERROR
popd
endlocal
exit /b 0

:END
popd
endlocal
exit /b 0
