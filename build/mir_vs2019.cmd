@echo off
setlocal enabledelayedexpansion
pushd "%~dp0"

cd ..
if not exist bin mkdir bin
if not exist bin goto ERROR1
cd bin

if not exist mir mkdir mir
set CMKFILE=CMakeLists.txt
set ORGDIR=..\..\mir
set ORGFILE=%ORGDIR%\%CMKFILE%
cd mir
copy /y %ORGFILE% .\
del %ORGFILE%

for /f "tokens=* delims=0123456789 eol=" %%a in ('findstr /n "^" %CMKFILE%') do (
    set b=%%a
    set b=!b:~1!
    (echo.!b!) >> %ORGFILE%
    set c=!b:"=!
    if /i "!c!"=="project (MIR)" (
        (echo.if^(MSVC^)) >> %ORGFILE%
        (echo.    string^(REPLACE "/MD" "/MT" CMAKE_C_FLAGS_DEBUG            ${CMAKE_C_FLAGS_DEBUG}^)) >> %ORGFILE%
        (echo.    string^(REPLACE "/MD" "/MT" CMAKE_C_FLAGS_MINSIZEREL       ${CMAKE_C_FLAGS_MINSIZEREL}^)) >> %ORGFILE%
        (echo.    string^(REPLACE "/MD" "/MT" CMAKE_C_FLAGS_RELEASE          ${CMAKE_C_FLAGS_RELEASE}^)) >> %ORGFILE%
        (echo.    string^(REPLACE "/MD" "/MT" CMAKE_C_FLAGS_RELWITHDEBINFO   ${CMAKE_C_FLAGS_RELWITHDEBINFO}^)) >> %ORGFILE%
        (echo.    string^(REPLACE "/MD" "/MT" CMAKE_CXX_FLAGS_DEBUG          ${CMAKE_CXX_FLAGS_DEBUG}^)) >> %ORGFILE%
        (echo.    string^(REPLACE "/MD" "/MT" CMAKE_CXX_FLAGS_MINSIZEREL     ${CMAKE_CXX_FLAGS_MINSIZEREL}^)) >> %ORGFILE%
        (echo.    string^(REPLACE "/MD" "/MT" CMAKE_CXX_FLAGS_RELEASE        ${CMAKE_CXX_FLAGS_RELEASE}^)) >> %ORGFILE%
        (echo.    string^(REPLACE "/MD" "/MT" CMAKE_CXX_FLAGS_RELWITHDEBINFO ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}^)) >> %ORGFILE%
        (echo.endif^(MSVC^)) >> %ORGFILE%
    )
)

cmake %ORGDIR% -G "Visual Studio 16 2019"
msbuild /p:Configuration=Release MIR.sln
if exist Release\c2m.exe copy /y Release\c2m.exe ..\c2m.exe
if exist Release\mir_static.lib copy /y Release\mir_static.lib ..\mir_static.lib

:END
if exist .\%CMKFILE% copy .\%CMKFILE% ..\..\mir\

popd
endlocal
exit /b 0

:ERROR1
echo Error occurred.
popd
endlocal
exit /b 0
