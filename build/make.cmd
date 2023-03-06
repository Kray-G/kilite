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

echo Building zlib...
if not exist zlibstatic.lib call ..\build\zlib_win.cmd
if not exist zlibstatic.lib goto ERROR

echo Building minizip...
if not exist libminizip.lib call ..\build\minizip_win.cmd
if not exist libminizip.lib goto ERROR

echo Building oniguruma...
if not exist onig.lib call ..\build\onig_win.cmd
if not exist onig.lib goto ERROR

del libkilite.a
set TEMPF=%BIN%\libkilite.c

@echo Generating a header source code...
cl /nologo /O2 /MT ..\build\makecstr.c > NUL
cl /nologo /O2 /MT ..\build\clockspersec.c > NUL
echo #define KILITE_AMALGAMATION > %TEMPF%
type ..\src\template\lib\bign.h >> %TEMPF%
type ..\src\template\lib\bigz.h >> %TEMPF%
type ..\src\template\lib\printf.h >> %TEMPF%
type ..\src\template\header.h >> %TEMPF%
makecstr.exe %TEMPF% > ..\src\backend\header.c
del %TEMPF%

@echo Building a Kilite binary...
cl /nologo /O2 /I..\..\mir  /MT /DONIG_EXTERN=extern ^
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
    ..\bin\onig.lib ^
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
echo #line 1 "lib.h" >> %TEMPF%
type ..\src\template\lib.h >> %TEMPF%
copy /y "%TEMPF%" "..\src\template\inc\lib\kilite.h" 

echo #line 1 "bign.c" >> %TEMPF%
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
echo #line 1 "libstd.c" >> %TEMPF%
type ..\src\template\libstd.c >> %TEMPF%
echo #line 1 "libxml.c" >> %TEMPF%
type ..\src\template\libxml.c >> %TEMPF%
echo #line 1 "libzip.c" >> %TEMPF%
type ..\src\template\libzip.c >> %TEMPF%
echo #line 1 "libregex.c" >> %TEMPF%
type ..\src\template\libregex.c >> %TEMPF%
echo #line 1 "inc/platform.c" >> %TEMPF%
type ..\src\template\inc\platform.c >> %TEMPF%
pushd ..\src\template\std
echo #line 1 "makelib/integer" >> %TEMPF%
kilite.exe --makelib integer.klt >> %TEMPF%
echo #line 1 "makelib/string" >> %TEMPF%
kilite.exe --makelib string.klt >> %TEMPF%
echo #line 1 "makelib/array" >> %TEMPF%
kilite.exe --makelib array.klt >> %TEMPF%
echo #line 1 "makelib/file" >> %TEMPF%
kilite.exe --makelib file.klt >> %TEMPF%
echo #line 1 "makelib/system" >> %TEMPF%
kilite.exe --makelib system.klt >> %TEMPF%
popd

@echo Generating a static library file for cl...
cl /nologo /O2 /DUSE_INT64 /I..\src\template /I..\src\template\inc /DONIG_EXTERN=extern /c %TEMPF%
lib /nologo /out:klstd.lib %TEMPF:.c=.obj%
c2m -DUSE_INT64 -DONIG_EXTERN=extern -Ilib -c %TEMPF%
if exist kilite.bmir del kilite.bmir
ren %TEMPF:.c=.bmir% kilite.bmir
copy /y kilite.bmir ..\kilite.bmir > NUL

:GCC_CHK
@gcc -v > NUL 2>&1
if ERRORLEVEL 1 goto CHK_END
call :gcc %TEMPF%

:CHK_END
goto MKLIB

:ERROR
popd
endlocal
echo Some errors.
exit /b 0

:MKLIB
lib /nologo /OUT:kilite.lib klstd.lib onig.lib zlibstatic.lib libminizip.lib
copy /y kilite.lib ..\kilite.lib > NUL
if exist libkilite.a copy /y libkilite.a ..\libkilite.a

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
gcc -O3 -o libkilite.o -DUSE_INT64 -DONIG_EXTERN=extern -Wno-stringop-overflow -Ilib -I../src/template -I../src/template/inc -c %1%
ar rcs libklstd.a libkilite.o
if not exist libonig.a exit /b 0
if not exist libzlibstatic.a exit /b 0
if not exist libminizip.a exit /b 0
ar cqT libkilite.a libklstd.a libonig.a libzlibstatic.a libminizip.a
(
    echo create libkilite.a
    echo addlib libklstd.a
    echo addlib libonig.a
    echo addlib libzlibstatic.a
    echo addlib libminizip.a
    echo save
    echo end
) | ar -M
exit /b 0
