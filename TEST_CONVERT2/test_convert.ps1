$env:PYTHONPATH="C:\Users\bangmin.wu\Desktop\TEST_CONVERT2\openusd\lib\python;C:\Users\bangmin.wu\Desktop\TEST_CONVERT2\openusd\python"
$env:PATH="C:\Users\bangmin.wu\Desktop\TEST_CONVERT2\openusd\bin;C:\Users\bangmin.wu\Desktop\TEST_CONVERT2\openusd\lib;C:\Users\bangmin.wu\Desktop\TEST_CONVERT2\openusd\plugin;$env:PATH"
$env:PXR_PLUGINPATH_NAME="C:\Users\bangmin.wu\Desktop\TEST_CONVERT2\openusd\lib\usd;C:\Users\bangmin.wu\Desktop\TEST_CONVERT2\openusd\plugin\usd"

Write-Host "Running converter..."
.\build\Release\StepToStlConverter.exe "inputfile\small_test.step" "outputfile_usd\small_test.usdc"
Write-Host "Exit code: $LASTEXITCODE"