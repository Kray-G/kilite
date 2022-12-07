@echo off
setlocal enabledelayedexpansion
pushd "%~dp0"

cd ..
if not exist bin mkdir bin
if not exist bin goto ERROR1
cd bin

if not exist zlib mkdir zlib
set CMKFILE=CMakeLists.txt
set ORGDIR=..\..\submodules\zlib
set ORGFILE=%ORGDIR%\%CMKFILE%
cd zlib
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

cmake -DCMAKE_INSTALL_PREFIX=.\install %ORGDIR% -G "Visual Studio 16 2019"
msbuild /p:Configuration=Release zlib.sln
msbuild /p:Configuration=Release INSTALL.vcxproj
if exist Release\zlibstatic.lib copy /y Release\zlibstatic.lib ..\zlibstatic.lib

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
