@echo off
setlocal enabledelayedexpansion
pushd "%~dp0"

cd ..
if not exist bin mkdir bin
if not exist bin goto ERROR1
cd bin

set BIN=%CD%
set PATH=%PATH%;%BIN%
if not exist mir_static.lib call ..\build\mir_vs2019.cmd
if not exist mir_static.lib goto ERROR

set TEMPF=%BIN%\h.c

@REM Create a header source code.
cl /nologo /O2 /MT ..\build\makecstr.c
type ..\src\template\lib\bign.h > %TEMPF%
type ..\src\template\lib\bigz.h >> %TEMPF%
type ..\src\template\lib\printf.h >> %TEMPF%
type ..\src\template\header.h >> %TEMPF%
makecstr.exe %TEMPF% > ..\src\backend\header.c
del %TEMPF%

cl /nologo /O2 /I ..\..\mir /MT ^
    /Fekilite.exe ^
    ..\src\main.c ^
    ..\src\frontend\lexer.c ^
    ..\src\frontend\parser.c ^
    ..\src\frontend\error.c ^
    ..\src\frontend\dispast.c ^
    ..\src\frontend\mkkir.c ^
    ..\src\frontend\opt/type.c ^
    ..\src\backend\dispkir.c ^
    ..\src\backend\translate.c ^
    ..\src\backend\resolver.c ^
    ..\src\backend\header.c ^
    ..\src\backend\cexec.c ^
    ..\bin\mir_static.lib

copy /y kilite.exe ..\kilite.exe

@REM Create a basic library.
if not exist lib mkdir lib
echo #define KILITE_AMALGAMATION > %TEMPF%
type ..\src\template\lib\bign.h >> %TEMPF%
type ..\src\template\lib\bigz.h >> %TEMPF%
echo #ifdef __MIRC__ >> %TEMPF%
type ..\src\template\lib\printf.h >> %TEMPF%
echo #endif >> %TEMPF%
type ..\src\template\header.h >> %TEMPF%
type ..\src\template\lib\bign.c >> %TEMPF%
type ..\src\template\lib\bigz.c >> %TEMPF%
echo #ifdef __MIRC__ >> %TEMPF%
type ..\src\template\lib\printf.c >> %TEMPF%
echo #endif >> %TEMPF%
type ..\src\template\alloc.c >> %TEMPF%
type ..\src\template\gc.c >> %TEMPF%
type ..\src\template\init.c >> %TEMPF%
type ..\src\template\main.c >> %TEMPF%
type ..\src\template\util.c >> %TEMPF%
type ..\src\template\format.c >> %TEMPF%
type ..\src\template\bigi.c >> %TEMPF%
type ..\src\template\str.c >> %TEMPF%
type ..\src\template\obj.c >> %TEMPF%
type ..\src\template\op.c >> %TEMPF%
type ..\src\template\libstd.c >> %TEMPF%
pushd ..\src\template\std
kilite.exe --makelib callbacks.klt >> %TEMPF%
popd

cl /nologo /O2 /DUSE_INT64 /I lib /c %TEMPF%
lib /nologo /out:kilite.lib %TEMPF:.c=.obj%
copy /y kilite.lib ..\kilite.lib
c2m -DUSE_INT64 -I lib -c %TEMPF%
del kilite.bmir
ren %TEMPF:.c=.bmir% kilite.bmir
copy /y kilite.bmir ..\kilite.bmir
@REM del %TEMPF%

goto CLEANUP

:ERROR
popd
endlocal
exit /b 0

:CLEANUP
del *.h *.obj
del makecstr.exe
rmdir /s /q lib

:END
popd
endlocal
exit /b 0
