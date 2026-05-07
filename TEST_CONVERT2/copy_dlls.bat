@echo off
setlocal enabledelayedexpansion

set "OCCT_BIN_DIR=C:\Users\bangmin.wu\Desktop\TEST_CONVERT2\occt_install\win64\vc14\bin"
set "USD_LIB_DIR=C:\Users\bangmin.wu\Desktop\TEST_CONVERT2\openusd\lib"
set "USD_BIN_DIR=C:\Users\bangmin.wu\Desktop\TEST_CONVERT2\openusd\bin"
set "USD_PYTHON_DIR=C:\Users\bangmin.wu\Desktop\TEST_CONVERT2\openusd\python"
set "OUTPUT_DIR=C:\Users\bangmin.wu\Desktop\TEST_CONVERT2\build\Release"

echo Copying OCCT DLLs...
xcopy "%OCCT_BIN_DIR%\TK*.dll" "%OUTPUT_DIR%\" /Y /Q

echo Copying USD DLLs...
xcopy "%USD_LIB_DIR%\usd_*.dll" "%OUTPUT_DIR%\" /Y /Q

echo Copying Boost DLLs...
xcopy "%USD_LIB_DIR%\boost_*.dll" "%OUTPUT_DIR%\" /Y /Q

echo Copying USD Python DLLs...
xcopy "%USD_PYTHON_DIR%\python312.dll" "%OUTPUT_DIR%\" /Y /Q

echo Copying other required DLLs...
xcopy "%USD_BIN_DIR%\tbb.dll" "%OUTPUT_DIR%\" /Y /Q
xcopy "%USD_BIN_DIR%\tbbmalloc.dll" "%OUTPUT_DIR%\" /Y /Q
xcopy "%USD_BIN_DIR%\tbbmalloc_proxy.dll" "%OUTPUT_DIR%\" /Y /Q
xcopy "%USD_BIN_DIR%\OpenEXR-3_3.dll" "%OUTPUT_DIR%\" /Y /Q
xcopy "%USD_BIN_DIR%\OpenEXRCore-3_3.dll" "%OUTPUT_DIR%\" /Y /Q
xcopy "%USD_BIN_DIR%\Imath-3_1.dll" "%OUTPUT_DIR%\" /Y /Q
xcopy "%USD_BIN_DIR%\Iex-3_3.dll" "%OUTPUT_DIR%\" /Y /Q
xcopy "%USD_BIN_DIR%\IlmThread-3_3.dll" "%OUTPUT_DIR%\" /Y /Q
xcopy "%USD_BIN_DIR%\zlib1.dll" "%OUTPUT_DIR%\" /Y /Q
xcopy "%USD_BIN_DIR%\libpng16.dll" "%OUTPUT_DIR%\" /Y /Q
xcopy "%USD_BIN_DIR%\tiff.dll" "%OUTPUT_DIR%\" /Y /Q
xcopy "%USD_BIN_DIR%\jpeg8.dll" "%OUTPUT_DIR%\" /Y /Q
xcopy "%USD_BIN_DIR%\turbojpeg.dll" "%OUTPUT_DIR%\" /Y /Q
xcopy "%USD_BIN_DIR%\OpenImageIO.dll" "%OUTPUT_DIR%\" /Y /Q
xcopy "%USD_BIN_DIR%\MaterialXCore.dll" "%OUTPUT_DIR%\" /Y /Q
xcopy "%USD_BIN_DIR%\Alembic.dll" "%OUTPUT_DIR%\" /Y /Q

echo DLLs copied successfully!
pause