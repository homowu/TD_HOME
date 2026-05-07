@echo off
setlocal enabledelayedexpansion

set "PYTHONPATH=C:\Users\bangmin.wu\Desktop\TEST_CONVERT2\openusd\lib\python;C:\Users\bangmin.wu\Desktop\TEST_CONVERT2\openusd\python"
set "PATH=C:\Users\bangmin.wu\Desktop\TEST_CONVERT2\openusd\bin;C:\Users\bangmin.wu\Desktop\TEST_CONVERT2\openusd\lib;C:\Users\bangmin.wu\Desktop\TEST_CONVERT2\openusd\plugin;C:\Users\bangmin.wu\Desktop\TEST_CONVERT2\openusd\python;C:\Users\bangmin.wu\Desktop\TEST_CONVERT2\occt_install\win64\vc14\bin;%PATH%"
set "PXR_PLUGINPATH_NAME=C:\Users\bangmin.wu\Desktop\TEST_CONVERT2\openusd\lib\usd;C:\Users\bangmin.wu\Desktop\TEST_CONVERT2\openusd\plugin\usd"

echo Checking converter existence...
if exist "build\Release\StepToStlConverter.exe" (
    echo Converter found!
) else (
    echo Converter NOT found!
)

echo Current PATH: %PATH%
echo.

echo Running converter with test file...
"build\Release\StepToStlConverter.exe" "inputfile\small_test.step" "outputfile_usd\small_test.usdc"
echo Exit code: %ERRORLEVEL%

echo.
dir "outputfile_usd"

pause