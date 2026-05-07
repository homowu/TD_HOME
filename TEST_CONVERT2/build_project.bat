@echo off
setlocal

set "VS_PATH=C:\Program Files\Microsoft Visual Studio\18\Community"
set "CMAKE_PATH=C:\Program Files\CMake\bin\cmake.exe"

if exist "%VS_PATH%\VC\Auxiliary\Build\vcvars64.bat" (
    call "%VS_PATH%\VC\Auxiliary\Build\vcvars64.bat"
) else (
    echo Warning: Could not find vcvars64.bat
    exit /b 1
)

set "BUILD_DIR=%~dp0build"

if exist "%BUILD_DIR%" (
    rmdir /s /q "%BUILD_DIR%"
)
mkdir "%BUILD_DIR%"
cd "%BUILD_DIR%"

echo Configuring project with CMake...
"%CMAKE_PATH%" -G "Visual Studio 18 2026" -A x64 ..

if %errorlevel% neq 0 (
    echo CMake configuration failed!
    exit /b 1
)

echo Building project...
"%CMAKE_PATH%" --build . --config Release

if %errorlevel% neq 0 (
    echo Build failed!
    exit /b 1
)

echo Project built successfully!
echo Output: %BUILD_DIR%\Release\StepToStlConverter.exe

endlocal