@echo off
setlocal

set "OCCT_SOURCE=C:\Users\bangmin.wu\Desktop\TEST_CONVERT2\occt_source\OCCT-7_8_0"
set "OCCT_BUILD=C:\Users\bangmin.wu\Desktop\TEST_CONVERT2\occt_build"
set "OCCT_INSTALL=C:\Users\bangmin.wu\Desktop\TEST_CONVERT2\occt_install"

set "VS_PATH=C:\Program Files\Microsoft Visual Studio\18\Community"

if exist "%VS_PATH%\VC\Auxiliary\Build\vcvars64.bat" (
    call "%VS_PATH%\VC\Auxiliary\Build\vcvars64.bat"
) else (
    echo Warning: Could not find vcvars64.bat
    exit /b 1
)

if not exist "%OCCT_BUILD%" mkdir "%OCCT_BUILD%"
cd "%OCCT_BUILD%"

echo Configuring OCCT with CMake...
cmake -G "Visual Studio 18 2026" -A x64 ^
    -DCMAKE_INSTALL_PREFIX="%OCCT_INSTALL%" ^
    -DBUILD_MODULE_Draw=OFF ^
    -DBUILD_MODULE_Visualization=OFF ^
    -DBUILD_MODULE_ApplicationFramework=OFF ^
    -DBUILD_MODULE_DataExchange=ON ^
    -DBUILD_MODULE_ModelingAlgorithms=ON ^
    -DBUILD_MODULE_ModelingData=ON ^
    -DBUILD_MODULE_FoundationClasses=ON ^
    -DBUILD_SHARED_LIBS=ON ^
    -DUSE_FREETYPE=OFF ^
    -DCMAKE_POLICY_DEFAULT_CMP0091=NEW ^
    "%OCCT_SOURCE%"

if %errorlevel% neq 0 (
    echo CMake configuration failed!
    exit /b 1
)

echo Building OCCT...
cmake --build . --config Release -- /maxcpucount:4

if %errorlevel% neq 0 (
    echo Build failed!
    exit /b 1
)

echo Installing OCCT...
cmake --install . --config Release

echo OCCT built and installed successfully!
echo Installed to: %OCCT_INSTALL%

endlocal