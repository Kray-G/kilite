@echo off
setlocal enabledelayedexpansion
pushd "%~dp0"

cd ..
if not exist bin mkdir bin
if not exist bin goto ERROR1
cd bin

if not exist mir_static.lib call ..\build\mir_vs2019.cmd
if not exist mir_static.lib goto ERROR

set TEMPF=h.c

@REM Create a header source code.
cl -O2 ..\build\makecstr.c
type ..\src\template\lib\bign.h > %TEMPF%
type ..\src\template\lib\bigz.h >> %TEMPF%
type ..\src\template\lib\printf.h >> %TEMPF%
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
copy /y ..\src\template\lib\bign.h .\lib\bign.h
copy /y ..\src\template\lib\bigz.h .\lib\bigz.h
copy /y ..\src\template\lib\printf.h .\lib\printf.h
copy /y ..\src\template\common.h .\common.h
copy /y ..\src\template\header.h .\header.h
type ..\src\template\lib\bign.c > %TEMPF%
type ..\src\template\lib\bigz.c >> %TEMPF%
type ..\src\template\lib\printf.c >> %TEMPF%
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
