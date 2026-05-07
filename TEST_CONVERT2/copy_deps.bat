@echo off
setlocal

set "OCCT_BIN=%~dp0occt_install\win64\vc14\bin"
set "OUTPUT_DIR=%~dp0build\Release"

echo Copying OCCT DLLs to output directory...

copy "%OCCT_BIN%\TKDESTEP.dll" "%OUTPUT_DIR%\" /Y
copy "%OCCT_BIN%\TKDESTL.dll" "%OUTPUT_DIR%\" /Y
copy "%OCCT_BIN%\TKMesh.dll" "%OUTPUT_DIR%\" /Y
copy "%OCCT_BIN%\TKBRep.dll" "%OUTPUT_DIR%\" /Y
copy "%OCCT_BIN%\TKGeomAlgo.dll" "%OUTPUT_DIR%\" /Y
copy "%OCCT_BIN%\TKGeomBase.dll" "%OUTPUT_DIR%\" /Y
copy "%OCCT_BIN%\TKTopAlgo.dll" "%OUTPUT_DIR%\" /Y
copy "%OCCT_BIN%\TKBO.dll" "%OUTPUT_DIR%\" /Y
copy "%OCCT_BIN%\TKBool.dll" "%OUTPUT_DIR%\" /Y
copy "%OCCT_BIN%\TKXSBase.dll" "%OUTPUT_DIR%\" /Y
copy "%OCCT_BIN%\TKDE.dll" "%OUTPUT_DIR%\" /Y
copy "%OCCT_BIN%\TKG3d.dll" "%OUTPUT_DIR%\" /Y
copy "%OCCT_BIN%\TKG2d.dll" "%OUTPUT_DIR%\" /Y
copy "%OCCT_BIN%\TKMath.dll" "%OUTPUT_DIR%\" /Y
copy "%OCCT_BIN%\TKernel.dll" "%OUTPUT_DIR%\" /Y
copy "%OCCT_BIN%\TKShHealing.dll" "%OUTPUT_DIR%\" /Y
copy "%OCCT_BIN%\TKPrim.dll" "%OUTPUT_DIR%\" /Y
copy "%OCCT_BIN%\TKCAF.dll" "%OUTPUT_DIR%\" /Y
copy "%OCCT_BIN%\TKCDF.dll" "%OUTPUT_DIR%\" /Y
copy "%OCCT_BIN%\TKLCAF.dll" "%OUTPUT_DIR%\" /Y
copy "%OCCT_BIN%\TKXCAF.dll" "%OUTPUT_DIR%\" /Y
copy "%OCCT_BIN%\TKService.dll" "%OUTPUT_DIR%\" /Y
copy "%OCCT_BIN%\TKStd.dll" "%OUTPUT_DIR%\" /Y
copy "%OCCT_BIN%\TKStdL.dll" "%OUTPUT_DIR%\" /Y
copy "%OCCT_BIN%\TKTObj.dll" "%OUTPUT_DIR%\" /Y
copy "%OCCT_BIN%\TKVCAF.dll" "%OUTPUT_DIR%\" /Y
copy "%OCCT_BIN%\TKHLR.dll" "%OUTPUT_DIR%\" /Y
copy "%OCCT_BIN%\TKOffset.dll" "%OUTPUT_DIR%\" /Y
copy "%OCCT_BIN%\TKFillet.dll" "%OUTPUT_DIR%\" /Y
copy "%OCCT_BIN%\TKFeat.dll" "%OUTPUT_DIR%\" /Y
copy "%OCCT_BIN%\TKXMesh.dll" "%OUTPUT_DIR%\" /Y
copy "%OCCT_BIN%\TKDEIGES.dll" "%OUTPUT_DIR%\" /Y
copy "%OCCT_BIN%\TKDEVRML.dll" "%OUTPUT_DIR%\" /Y
copy "%OCCT_BIN%\TKRWMesh.dll" "%OUTPUT_DIR%\" /Y
copy "%OCCT_BIN%\TKDECascade.dll" "%OUTPUT_DIR%\" /Y
copy "%OCCT_BIN%\TKBin.dll" "%OUTPUT_DIR%\" /Y
copy "%OCCT_BIN%\TKBinL.dll" "%OUTPUT_DIR%\" /Y
copy "%OCCT_BIN%\TKBinTObj.dll" "%OUTPUT_DIR%\" /Y
copy "%OCCT_BIN%\TKBinXCAF.dll" "%OUTPUT_DIR%\" /Y
copy "%OCCT_BIN%\TKXml.dll" "%OUTPUT_DIR%\" /Y
copy "%OCCT_BIN%\TKXmlL.dll" "%OUTPUT_DIR%\" /Y
copy "%OCCT_BIN%\TKXmlTObj.dll" "%OUTPUT_DIR%\" /Y
copy "%OCCT_BIN%\TKXmlXCAF.dll" "%OUTPUT_DIR%\" /Y
copy "%OCCT_BIN%\TKDEOBJ.dll" "%OUTPUT_DIR%\" /Y
copy "%OCCT_BIN%\TKDEGLTF.dll" "%OUTPUT_DIR%\" /Y
copy "%OCCT_BIN%\TKDEPLY.dll" "%OUTPUT_DIR%\" /Y
copy "%OCCT_BIN%\TKExpress.dll" "%OUTPUT_DIR%\" /Y

echo DLLs copied successfully!

endlocal