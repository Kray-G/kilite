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

set TEMPF=%BIN%\libkilite.c

@echo Generating a header source code...
cl /nologo /O2 /MT ..\build\makecstr.c > NUL
type ..\src\template\lib\bign.h > %TEMPF%
type ..\src\template\lib\bigz.h >> %TEMPF%
type ..\src\template\lib\printf.h >> %TEMPF%
type ..\src\template\header.h >> %TEMPF%
makecstr.exe %TEMPF% > ..\src\backend\header.c
del %TEMPF%

@echo Building a Kilite binary...
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

copy /y kilite.exe ..\kilite.exe > NUL

@echo Creating a basic library...
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
type ..\src\template\inc\platform.c >> %TEMPF%
pushd ..\src\template\std
kilite.exe --makelib callbacks.klt >> %TEMPF%
popd

@echo Generating a static library file for cl...
cl /nologo /O2 /DUSE_INT64 /c %TEMPF%
lib /nologo /out:kilite.lib %TEMPF:.c=.obj%
copy /y kilite.lib ..\kilite.lib > NUL
c2m -DUSE_INT64 -I lib -c %TEMPF%
if exist kilite.bmir del kilite.bmir
ren %TEMPF:.c=.bmir% kilite.bmir
copy /y kilite.bmir ..\kilite.bmir > NUL

:TCC_CHK
@tcc -v > NUL 2>&1
if ERRORLEVEL 1 goto GCC_CHK
call :tcc %TEMPF%
:GCC_CHK
@gcc -v > NUL 2>&1
if ERRORLEVEL 1 goto CHK_END
call :gcc %TEMPF%

:CHK_END
if not exist libkilite.a @copy /y libkilite_tcc.a libkilite.a > NUL 2>&1
if not exist libkilite.a @copy /y libkilite_gcc.a libkilite.a > NUL 2>&1
copy /y libkilite.a ..\libkilite.a > NUL

goto CLEANUP

:ERROR
popd
endlocal
exit /b 0

:CLEANUP
@REM del %TEMPF%
del *.h *.o *.obj
del makecstr.exe
@echo Building Kilite modules successfully ended.

:END
popd
endlocal
exit /b 0

:tcc
@echo Generating a static library file for tcc...
tcc -o libkilite.o -DUSE_INT64 -I lib -c %1%
tcc -ar rcs libkilite_tcc.a libkilite.o
copy /y libkilite_tcc.a ..\libkilite_tcc.a > NUL
exit /b 0

:gcc
@echo Generating a static library file for gcc...
gcc -O3 -o libkilite.o -DUSE_INT64 -I lib -c %1%
ar rcs libkilite_gcc.a libkilite.o
copy /y libkilite_gcc.a ..\libkilite_gcc.a > NUL
exit /b 0
