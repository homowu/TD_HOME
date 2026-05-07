@echo off
setlocal

rem Root = folder where this bat lives. Put inputfile here; exe+dll live in runtime\
set "ROOT=%~dp0"
set "INPUT_DIR=%ROOT%inputfile"
set "OUTPUT_DIR=%ROOT%outputfile_stl"

set "CONVERTER=%ROOT%runtime\StepToStlConverter.exe"
if not exist "%CONVERTER%" set "CONVERTER=%ROOT%build\Release\StepToStlConverter.exe"

if not exist "%INPUT_DIR%\" (
    echo ERROR: Missing folder "inputfile" next to this bat.
    echo Create inputfile and put .step / .stp files inside.
    pause
    exit /b 1
)

if not exist "%CONVERTER%" (
    echo ERROR: StepToStlConverter.exe not found.
    echo Portable: keep folder runtime\ next to this bat. Dev: build Release to build\Release\
    pause
    exit /b 1
)

if not exist "%OUTPUT_DIR%\" mkdir "%OUTPUT_DIR%"

echo Batch: STEP -^> STL
echo Input:  %INPUT_DIR%
echo Output: %OUTPUT_DIR%
echo.

"%CONVERTER%" -batch "%INPUT_DIR%" "%OUTPUT_DIR%"
set "EC=%ERRORLEVEL%"

echo.
if "%EC%"=="0" (
    echo Done.
) else (
    echo Failed. Exit code: %EC%
)
pause
exit /b %EC%
