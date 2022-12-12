@echo off
setlocal enabledelayedexpansion
pushd "%~dp0"

cd ..
if not exist bin mkdir bin
if not exist bin goto ERROR1
cd bin

set BIN=%CD%
set PATH=%PATH%;%BIN%
echo Building MIR...
if not exist mir_static.lib call ..\build\mir_win.cmd
if not exist mir_static.lib goto ERROR
copy /y mir_static.lib ..\mir_static.lib

echo Building zlib...
if not exist zlibstatic.lib call ..\build\zlib_win.cmd
if not exist zlibstatic.lib goto ERROR
copy /y zlibstatic.lib ..\zlibstatic.lib
if exist libzlibstatic.a copy /y libzlibstatic.a ..\libzlibstatic.a

echo Building minizip...
if not exist libminizip.lib call ..\build\minizip_win.cmd
if not exist libminizip.lib goto ERROR
copy /y libminizip.lib ..\libminizip.lib
if exist libminizip.a copy /y libminizip.a ..\libminizip.a

echo Building oniguruma...
if not exist onig.lib call ..\build\onig_win.cmd
if not exist onig.lib goto ERROR
copy /y onig.lib ..\onig.lib
if exist libonig.a copy /y libonig.a ..\libonig.a

del libkilite.a
set TEMPF=%BIN%\libkilite.c

@echo Generating a header source code...
cl /nologo /O2 /MT ..\build\makecstr.c > NUL
cl /nologo /O2 /MT ..\build\clockspersec.c > NUL
type ..\src\template\lib\bign.h > %TEMPF%
type ..\src\template\lib\bigz.h >> %TEMPF%
type ..\src\template\lib\printf.h >> %TEMPF%
type ..\src\template\header.h >> %TEMPF%
makecstr.exe %TEMPF% > ..\src\backend\header.c
del %TEMPF%

@echo Building a Kilite binary...
cl /nologo /O2 /I..\..\mir  /MT ^
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
    ..\bin\libminizip.lib ^
    ..\bin\zlibstatic.lib ^
    ..\bin\mir_static.lib

copy /y kilite.exe ..\kilite.exe > NUL

@echo Creating a basic library...
echo #define KILITE_AMALGAMATION > %TEMPF%
clockspersec.exe >> %TEMPF%
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
echo #line 1 "alloc.c" >> %TEMPF%
type ..\src\template\alloc.c >> %TEMPF%
echo #line 1 "gc.c" >> %TEMPF%
type ..\src\template\gc.c >> %TEMPF%
echo #line 1 "init.c" >> %TEMPF%
type ..\src\template\init.c >> %TEMPF%
echo #line 1 "main.c" >> %TEMPF%
type ..\src\template\main.c >> %TEMPF%
echo #line 1 "util.c" >> %TEMPF%
type ..\src\template\util.c >> %TEMPF%
echo #line 1 "format.c" >> %TEMPF%
type ..\src\template\format.c >> %TEMPF%
echo #line 1 "bigi.c" >> %TEMPF%
type ..\src\template\bigi.c >> %TEMPF%
echo #line 1 "str.c" >> %TEMPF%
type ..\src\template\str.c >> %TEMPF%
echo #line 1 "bin.c" >> %TEMPF%
type ..\src\template\bin.c >> %TEMPF%
echo #line 1 "obj.c" >> %TEMPF%
type ..\src\template\obj.c >> %TEMPF%
echo #line 1 "op.c" >> %TEMPF%
type ..\src\template\op.c >> %TEMPF%
echo #line 1 "lib.h" >> %TEMPF%
type ..\src\template\lib.h >> %TEMPF%
echo #line 1 "libstd.c" >> %TEMPF%
type ..\src\template\libstd.c >> %TEMPF%
echo #line 1 "libxml.c" >> %TEMPF%
type ..\src\template\libxml.c >> %TEMPF%
echo #line 1 "libzip.c" >> %TEMPF%
type ..\src\template\libzip.c >> %TEMPF%
echo #line 1 "inc/platform.c" >> %TEMPF%
type ..\src\template\inc\platform.c >> %TEMPF%
pushd ..\src\template\std
kilite.exe --makelib callbacks.klt >> %TEMPF%
popd

@echo Generating a static library file for cl...
cl /nologo /O2 /DUSE_INT64 /I..\src\template /I..\src\template\inc /c %TEMPF%
lib /nologo /out:kilite.lib %TEMPF:.c=.obj%
copy /y kilite.lib ..\kilite.lib > NUL
c2m -DUSE_INT64 -Ilib -c %TEMPF%
if exist kilite.bmir del kilite.bmir
ren %TEMPF:.c=.bmir% kilite.bmir
copy /y kilite.bmir ..\kilite.bmir > NUL

:GCC_CHK
@gcc -v > NUL 2>&1
if ERRORLEVEL 1 goto CHK_END
call :gcc %TEMPF%

:CHK_END
if not exist libkilite.a @copy /y libkilite.a libkilite.a > NUL 2>&1
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

:gcc
@echo Generating a static library file for gcc...
gcc -O3 -o libkilite.o -DUSE_INT64 -Ilib -I../src/template -I../src/template/inc -c %1%
ar rcs libkilite.a libkilite.o
copy /y libkilite.a ..\libkilite.a > NUL
exit /b 0
