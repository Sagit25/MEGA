@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "SCRIPT_DIR=%~dp0"
pushd "%SCRIPT_DIR%" >nul || exit /b 1

if not defined CONFIG set "CONFIG=Release"
if not defined GENERATOR set "GENERATOR=default"
if not defined ARCH set "ARCH=x64"
set "CLEAN=0"

:parse_args
if "%~1"=="" goto args_done
if /I "%~1"=="--clean" (
    set "CLEAN=1"
    shift
    goto parse_args
)
if /I "%~1"=="clean" (
    set "CLEAN=1"
    shift
    goto parse_args
)
if /I "%~1"=="--debug" (
    set "CONFIG=Debug"
    shift
    goto parse_args
)
if /I "%~1"=="--release" (
    set "CONFIG=Release"
    shift
    goto parse_args
)
if /I "%~1"=="--help" goto usage
if /I "%~1"=="-h" goto usage
echo Unknown argument: %~1
goto usage_error

:args_done
call :find_cmake
if errorlevel 1 goto fail

if "%CLEAN%"=="1" (
    if exist "build" (
        echo Removing build directory...
        rmdir /s /q "build"
        if errorlevel 1 goto fail
    )
)

echo Using CMake: "%CMAKE_EXE%"
echo Configuration: %CONFIG%
echo.

if /I "%GENERATOR%"=="default" (
    if /I "%ARCH%"=="default" (
        "%CMAKE_EXE%" -S . -B build
    ) else (
        "%CMAKE_EXE%" -S . -B build -A "%ARCH%"
    )
) else (
    if /I "%ARCH%"=="default" (
        "%CMAKE_EXE%" -S . -B build -G "%GENERATOR%"
    ) else (
        "%CMAKE_EXE%" -S . -B build -G "%GENERATOR%" -A "%ARCH%"
    )
)
if errorlevel 1 (
    echo.
    echo CMake configure failed. If the build cache was made with another generator, run:
    echo   build_windows.bat --clean
    goto fail
)

"%CMAKE_EXE%" --build build --config "%CONFIG%" --parallel
if errorlevel 1 goto fail

call :copy_dlls "build"
call :copy_dlls "build\%CONFIG%"

set "EXE_PATH="
set "EXE_DIR="
if exist "build\%CONFIG%\main.exe" (
    set "EXE_PATH=%SCRIPT_DIR%build\%CONFIG%\main.exe"
    set "EXE_DIR=%SCRIPT_DIR%build\%CONFIG%"
)
if not defined EXE_PATH if exist "build\main.exe" (
    set "EXE_PATH=%SCRIPT_DIR%build\main.exe"
    set "EXE_DIR=%SCRIPT_DIR%build"
)

echo.
if defined EXE_PATH (
    echo Build finished: "%EXE_PATH%"
    echo Run with:
    echo   cd /d "%EXE_DIR%" ^&^& main.exe
) else (
    echo Build finished, but main.exe was not found in build\%CONFIG% or build.
)

popd >nul
exit /b 0

:find_cmake
if defined CMAKE_EXE (
    if exist "%CMAKE_EXE%" exit /b 0
    echo CMAKE_EXE is set, but the file does not exist:
    echo   "%CMAKE_EXE%"
    exit /b 1
)

for /f "delims=" %%I in ('where cmake 2^>nul') do (
    set "CMAKE_EXE=%%I"
    goto cmake_found
)

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
    for /f "delims=" %%I in ('"%VSWHERE%" -latest -products * -requires Microsoft.Component.MSBuild -property installationPath 2^>nul') do (
        if exist "%%I\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" (
            set "CMAKE_EXE=%%I\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
            goto cmake_found
        )
    )
)

for %%R in (
    "%ProgramFiles%\Microsoft Visual Studio\18"
    "%ProgramFiles%\Microsoft Visual Studio\2022"
    "%ProgramFiles(x86)%\Microsoft Visual Studio\2019"
    "%ProgramFiles(x86)%\Microsoft Visual Studio\2017"
) do (
    for %%E in (Community Professional Enterprise BuildTools) do (
        if exist "%%~R\%%E\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" (
            set "CMAKE_EXE=%%~R\%%E\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
            goto cmake_found
        )
    )
)

echo Could not find cmake.exe.
echo Install CMake, open a Visual Studio Developer Command Prompt, or set CMAKE_EXE to cmake.exe.
exit /b 1

:cmake_found
exit /b 0

:copy_dlls
set "OUT_DIR=%~1"
if not exist "%OUT_DIR%" exit /b 0
if exist "..\dlls\*.dll" (
    for %%D in (..\dlls\*.dll) do copy /Y "%%~fD" "%OUT_DIR%\" >nul
)
exit /b 0

:usage
echo Usage: build_windows.bat [--clean] [--debug^|--release]
echo.
echo Environment overrides:
echo   CONFIG=Debug or Release
echo   GENERATOR=default or Visual Studio 18 2026 or Visual Studio 17 2022
echo   ARCH=x64 or default
echo   CMAKE_EXE=C:\path\to\cmake.exe
popd >nul
exit /b 0

:usage_error
echo Usage: build_windows.bat [--clean] [--debug^|--release]

:fail
popd >nul
exit /b 1
