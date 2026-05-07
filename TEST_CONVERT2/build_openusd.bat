@echo off
setlocal enabledelayedexpansion

echo ==============================================
echo OpenUSD Build Script
echo ==============================================
echo.

set "SCRIPT_DIR=%~dp0"
set "OPENUSD_SOURCE=%SCRIPT_DIR%OpenUSD"
set "OPENUSD_INSTALL=%SCRIPT_DIR%openusd_install"

echo [INFO] Step 1: Cloning OpenUSD source code...
echo.

if not exist "%OPENUSD_SOURCE%" (
    echo Cloning OpenUSD from GitHub...
    git clone https://github.com/PixarAnimationStudios/OpenUSD "%OPENUSD_SOURCE%"
    if !errorlevel! neq 0 (
        echo [ERROR] Failed to clone OpenUSD source code.
        echo.
        pause
        exit /b 1
    )
) else (
    echo [INFO] OpenUSD source already exists at: %OPENUSD_SOURCE%
)

echo.
echo [INFO] Step 2: Building OpenUSD (this may take 30 minutes to several hours)...
echo.
echo [INFO] OpenUSD install directory: %OPENUSD_INSTALL%
echo.

echo Starting build with build_usd.py script...
echo This process will:
echo   1. Download required dependencies (Boost, TBB, etc.)
echo   2. Build OpenUSD core libraries
echo   3. Install to: %OPENUSD_INSTALL%
echo.
echo Please wait...
echo.

python "%OPENUSD_SOURCE%\build_scripts\build_usd.py" "%OPENUSD_INSTALL%" --no-python

if !errorlevel! neq 0 (
    echo.
    echo [ERROR] OpenUSD build failed.
    echo.
    pause
    exit /b 1
)

echo.
echo ==============================================
echo OpenUSD Build Completed!
echo ==============================================
echo.
echo [INFO] OpenUSD installed at: %OPENUSD_INSTALL%
echo.
echo Next steps:
echo   1. Add OpenUSD to your project CMakeLists.txt
echo   2. Rebuild StepToStlConverter
echo.
echo To configure CMake to find OpenUSD, set these environment variables:
echo   set USD_ROOT=%OPENUSD_INSTALL%
echo   set PATH=%OPENUSD_INSTALL%\bin;%%PATH%%
echo.
pause

endlocal
