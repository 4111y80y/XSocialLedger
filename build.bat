@echo off
REM Build script for XSocialLedger
REM Setup MSVC 2022 environment and build with cmake + ninja

call "D:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64

set CMAKE_EXE=D:\Qt\Tools\CMake_64\bin\cmake.exe
set NINJA_EXE=D:\Qt\Tools\Ninja\ninja.exe

REM Configure
"%CMAKE_EXE%" -S "D:\5118\XSocialLedger" -B "D:\5118\XSocialLedger\build\Release" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="D:/Qt/6.10.1/msvc2022_64" -DCMAKE_MAKE_PROGRAM="%NINJA_EXE%"

if %ERRORLEVEL% neq 0 (
    echo CMake configure failed!
    exit /b %ERRORLEVEL%
)

REM Build
"%CMAKE_EXE%" --build "D:\5118\XSocialLedger\build\Release" --config Release

if %ERRORLEVEL% neq 0 (
    echo Build failed!
    exit /b %ERRORLEVEL%
)

echo Build succeeded!
