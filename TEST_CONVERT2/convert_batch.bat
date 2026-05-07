@echo off
setlocal enabledelayedexpansion

set "CONVERTER=%~dp0build\Release\StepToStlConverter.exe"
set "INPUT_DIR=inputfile"
set "OUTPUT_DIR=outputfile"

rem =============================================
rem OUTPUT FORMAT CONFIGURATION
rem =============================================
rem Choose output format by uncommenting one line below:
rem
rem For STL output:
set "OUTPUT_EXT=.stl"
set "FORMAT_NAME=STL"
rem
rem For USD output:
rem set "OUTPUT_EXT=.usda"
rem set "FORMAT_NAME=USD"
rem =============================================

echo ==============================================
echo STEP to STL/USD Batch Converter
echo ==============================================
echo.
echo [CONFIG] Output format: !FORMAT_NAME!
echo.

if not exist "%CONVERTER%" (
    echo [ERROR] Converter not found at:
    echo         %CONVERTER%
    echo.
    pause
    exit /b 1
)

if not exist "%INPUT_DIR%" (
    echo [ERROR] Input directory not found:
    echo         %INPUT_DIR%
    echo.
    pause
    exit /b 1
)

if not exist "%OUTPUT_DIR%" (
    mkdir "%OUTPUT_DIR%"
    echo [INFO] Created output directory: %OUTPUT_DIR%
    echo.
)

echo [INFO] Scanning for STEP files in: %INPUT_DIR%
echo.

set "file_count=0"
for %%f in ("%INPUT_DIR%\*.step" "%INPUT_DIR%\*.stp") do (
    set /a file_count+=1
)

if %file_count% equ 0 (
    echo [ERROR] No STEP files found in directory: %INPUT_DIR%
    echo         Supported formats: .step, .stp
    echo.
    pause
    exit /b 1
)

echo [INFO] Found %file_count% STEP file(s) to process
echo.

set "success_count=0"
set "fail_count=0"

for %%f in ("%INPUT_DIR%\*.step" "%INPUT_DIR%\*.stp") do (
    set "INPUT_FILE=%%f"
    set "FILENAME=%%~nf"
    set "OUTPUT_FILE=!OUTPUT_DIR!\!FILENAME!!OUTPUT_EXT!"

    echo --------------------------------------------------
    echo Processing: !FILENAME! --^> !FORMAT_NAME!

    "%CONVERTER%" "!INPUT_FILE!" "!OUTPUT_FILE!"

    if !errorlevel! equ 0 (
        echo   [SUCCESS] Converted to: !FILENAME!!OUTPUT_EXT!
        set /a success_count+=1
    ) else (
        echo   [FAILED] Conversion failed for: !FILENAME!
        set /a fail_count+=1
    )
)

echo.
echo ==============================================
echo Conversion Summary
echo ==============================================
echo Total files processed: %file_count%
echo Success: !success_count!
echo Failed: !fail_count!
echo Output format: !FORMAT_NAME!
echo ==============================================

if !fail_count! equ 0 (
    echo.
    echo [SUCCESS] All conversions completed successfully!
) else (
    echo.
    echo [WARNING] Some conversions failed. Please check the errors above.
)

echo.
pause

endlocal
