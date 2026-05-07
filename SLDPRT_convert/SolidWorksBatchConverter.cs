using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
using System.Diagnostics;
using System.Threading;
using SolidWorks.Interop.sldworks;
using SolidWorks.Interop.swconst;

namespace SolidWorksBatchConverter
{
    /// <summary>
    /// 使用 SOLIDWORKS 官方互操作程序集（早期绑定）打开/导出。
    /// 注意：ISldWorks 必须在创建它的 STA 线程上使用；后台请用单独 STA 线程而非 ThreadPool(MTA)。
    /// </summary>
    public class SolidWorksBatchConverter
    {
        private ISldWorks _swApp;
        private Logger _logger;
        private bool _isRunning;
        private Process _swProcess;

        public event EventHandler<string> FileProcessing;
        public event EventHandler<int> ProgressChanged;

        public bool IsRunning => _isRunning;

        private readonly string[] _swProgIds = {
            "SldWorks.Application.32",
            "SldWorks.Application.31",
            "SldWorks.Application.30",
            "SldWorks.Application.29",
            "SldWorks.Application.28",
            "SldWorks.Application"
        };

        public SolidWorksBatchConverter(Logger logger)
        {
            _logger = logger;
            _isRunning = false;
            _swProcess = null;
        }

        public bool StartSolidWorks()
        {
            _logger.Log("正在启动 SOLIDWORKS...");

            if (!KillExistingSolidWorksProcesses())
            {
                _logger.LogWarning("无法终止现有的 SOLIDWORKS 进程");
            }

            if (TryConnectViaCom())
            {
                return true;
            }

            _logger.Log("尝试通过进程启动 SOLIDWORKS...");
            if (StartSolidWorksProcess())
            {
                Thread.Sleep(5000);

                for (int i = 0; i < 20; i++)
                {
                    _logger.Log($"等待 SOLIDWORKS 启动... ({i + 1}/20)");
                    if (TryConnectViaCom())
                    {
                        return true;
                    }
                    Thread.Sleep(2000);
                }
            }

            _logger.LogError("无法创建或连接到 SOLIDWORKS");
            _logger.LogError("可能原因:");
            _logger.LogError("1. SOLIDWORKS COM 组件未正确注册");
            _logger.LogError("2. 权限不足（尝试以管理员身份运行）");
            _logger.LogError("3. 位数不匹配（需要 64 位程序）");
            _logger.LogError("4. SOLIDWORKS 安装损坏（尝试修复安装）");
            _logger.LogError("");
            _logger.LogError("建议解决步骤:");
            _logger.LogError("1. 关闭所有 SOLIDWORKS 进程");
            _logger.LogError("2. 以管理员身份运行此程序");
            _logger.LogError("3. 如果问题持续，修复或重新安装 SOLIDWORKS");
            return false;
        }

        private bool TryConnectViaCom()
        {
            foreach (string progId in _swProgIds)
            {
                try
                {
                    _logger.Log($"尝试 ProgID: {progId}");

                    object rawApp = null;
                    Type swType = Type.GetTypeFromProgID(progId);

                    if (swType == null)
                    {
                        _logger.LogWarning($"ProgID {progId} 未注册");
                        continue;
                    }

                    try
                    {
                        rawApp = Marshal.GetActiveObject(progId);
                        _logger.Log($"已连接到正在运行的 SOLIDWORKS 实例 (ProgID: {progId})");
                    }
                    catch (COMException ex)
                    {
                        if (ex.ErrorCode == -2147221021)
                        {
                            _logger.Log($"未找到运行中的 SOLIDWORKS，尝试创建新实例 (ProgID: {progId})...");
                            try
                            {
                                rawApp = Activator.CreateInstance(swType);
                                _logger.Log($"成功创建新的 SOLIDWORKS 实例 (ProgID: {progId})");
                            }
                            catch (Exception createEx)
                            {
                                _logger.LogWarning($"创建实例失败: {createEx.Message}");
                                continue;
                            }
                        }
                        else
                        {
                            _logger.LogWarning($"COM 异常 ({ex.ErrorCode:X8}): {ex.Message}");
                            continue;
                        }
                    }

                    if (rawApp != null)
                    {
                        try
                        {
                            _logger.Log("测试 SOLIDWORKS API 调用...");

                            var swApp = rawApp as ISldWorks;
                            if (swApp == null)
                            {
                                _logger.LogWarning("无法将实例转换为 ISldWorks");
                                TryRelease(rawApp);
                                continue;
                            }

                            string version = swApp.RevisionNumber() ?? "未知";
                            _logger.Log($"SOLIDWORKS 版本: {version}");

                            try
                            {
                                swApp.Visible = false;
                                _logger.Log("设置 Visible = false 成功");
                            }
                            catch (Exception visEx)
                            {
                                _logger.LogWarning($"设置 Visible 失败: {visEx.Message}，继续执行...");
                            }

                            _logger.LogSuccess("SOLIDWORKS 启动成功");
                            _swApp = swApp;
                            return true;
                        }
                        catch (Exception ex)
                        {
                            _logger.LogWarning($"SOLIDWORKS API 调用失败: {ex.Message}");
                            TryRelease(rawApp);
                            continue;
                        }
                    }
                }
                catch (Exception ex)
                {
                    _logger.LogWarning($"使用 ProgID {progId} 失败: {ex.Message}");
                }
            }
            return false;
        }

        private static void TryRelease(object comObj)
        {
            try
            {
                if (comObj != null)
                    Marshal.ReleaseComObject(comObj);
            }
            catch { /* ignore */ }
        }

        private bool StartSolidWorksProcess()
        {
            try
            {
                string swExePath = FindSolidWorksExecutable();
                if (string.IsNullOrEmpty(swExePath))
                {
                    _logger.LogError("无法找到 SOLIDWORKS 可执行文件");
                    return false;
                }

                _logger.Log($"启动 SOLIDWORKS: {swExePath}");

                ProcessStartInfo psi = new ProcessStartInfo
                {
                    FileName = swExePath,
                    Arguments = "/s",
                    CreateNoWindow = true,
                    WindowStyle = ProcessWindowStyle.Minimized
                };

                _swProcess = Process.Start(psi);
                if (_swProcess != null)
                {
                    _logger.Log($"SOLIDWORKS 进程已启动，PID: {_swProcess.Id}");
                    return true;
                }
            }
            catch (Exception ex)
            {
                _logger.LogError($"启动 SOLIDWORKS 进程失败: {ex.Message}");
            }
            return false;
        }

        private string FindSolidWorksExecutable()
        {
            string[] possiblePaths = {
                @"C:\Program Files\SOLIDWORKS Corp\SOLIDWORKS\sldworks.exe",
                @"C:\Program Files (x86)\SOLIDWORKS Corp\SOLIDWORKS\sldworks.exe",
                @"D:\Program Files\SOLIDWORKS Corp\SOLIDWORKS\sldworks.exe"
            };

            foreach (string path in possiblePaths)
            {
                if (File.Exists(path))
                {
                    return path;
                }
            }

            return null;
        }

        public bool KillExistingSolidWorksProcesses()
        {
            try
            {
                Process[] processes = Process.GetProcessesByName("SLDWORKS");
                if (processes.Length == 0)
                {
                    _logger.Log("未检测到正在运行的 SOLIDWORKS 进程");
                    return true;
                }

                _logger.Log($"检测到 {processes.Length} 个 SOLIDWORKS 进程，正在终止...");
                int killedCount = 0;

                foreach (Process process in processes)
                {
                    try
                    {
                        _logger.Log($"终止进程 PID: {process.Id}");
                        process.Kill();
                        process.WaitForExit(5000);
                        killedCount++;
                    }
                    catch (Exception ex)
                    {
                        _logger.LogWarning($"终止进程失败: {ex.Message}");
                    }
                }

                _logger.Log($"已终止 {killedCount} 个 SOLIDWORKS 进程");
                Thread.Sleep(2000);
                return true;
            }
            catch (Exception ex)
            {
                _logger.LogError($"终止 SOLIDWORKS 进程时发生错误: {ex.Message}");
                return false;
            }
        }

        public List<ConversionResult> ConvertFolder(string inputDir, string outputDir)
        {
            var results = new List<ConversionResult>();

            if (!Directory.Exists(inputDir))
            {
                _logger.LogError($"输入目录不存在: {inputDir}");
                return results;
            }

            if (!Directory.Exists(outputDir))
            {
                try
                {
                    Directory.CreateDirectory(outputDir);
                    _logger.Log($"创建输出目录: {outputDir}");
                }
                catch (Exception ex)
                {
                    _logger.LogError($"无法创建输出目录: {ex.Message}");
                    return results;
                }
            }

            var files = GetSolidWorksFiles(inputDir);

            if (files.Count == 0)
            {
                _logger.LogWarning("未找到 .sldprt 或 .sldasm 文件");
                return results;
            }

            _isRunning = true;
            _logger.Log($"找到 {files.Count} 个文件待处理");

            for (int i = 0; i < files.Count; i++)
            {
                if (!_isRunning)
                    break;

                var sourcePath = files[i];
                var relativePath = GetRelativePath(inputDir, sourcePath);
                var outputFileName = Path.GetFileNameWithoutExtension(sourcePath) + ".obj";
                var outputPath = Path.Combine(outputDir, outputFileName);

                OnFileProcessing(sourcePath);
                OnProgressChanged((int)((i + 1) * 100.0 / files.Count));

                var result = ConvertSingleFile(sourcePath, outputPath);
                results.Add(result);

                if (result.Success)
                {
                    var outName = !string.IsNullOrEmpty(result.OutputPath)
                        ? Path.GetFileName(result.OutputPath)
                        : outputFileName;
                    _logger.LogSuccess($"转换成功: {relativePath} -> {outName}");
                }
                else
                {
                    _logger.LogError($"转换失败: {relativePath} - {result.Message}");
                }
            }

            _isRunning = false;
            return results;
        }

        private List<string> GetSolidWorksFiles(string inputDir)
        {
            var files = new List<string>();

            var sldprtFiles = Directory.GetFiles(inputDir, "*.sldprt", SearchOption.AllDirectories);
            var sldasmFiles = Directory.GetFiles(inputDir, "*.sldasm", SearchOption.AllDirectories);

            files.AddRange(sldprtFiles);
            files.AddRange(sldasmFiles);

            files.Sort();
            return files;
        }

        private string GetRelativePath(string basePath, string targetPath)
        {
            Uri baseUri = new Uri(basePath + Path.DirectorySeparatorChar);
            Uri targetUri = new Uri(targetPath);
            return baseUri.MakeRelativeUri(targetUri).ToString().Replace('/', Path.DirectorySeparatorChar);
        }

        public ConversionResult ConvertSingleFile(string sourcePath, string outputPath)
        {
            var result = new ConversionResult
            {
                SourcePath = sourcePath,
                OutputPath = outputPath,
                Success = false
            };

            var startTime = DateTime.Now;

            try
            {
                if (!File.Exists(sourcePath))
                {
                    result.Message = "源文件不存在";
                    return result;
                }

                ModelDoc2 model = OpenDocument(sourcePath);
                if (model == null)
                {
                    result.Message = "无法打开文档";
                    return result;
                }

                try
                {
                    var exportResult = ExportToFbx(model, outputPath);
                    result.Success = exportResult.Success;
                    result.ErrorCode = exportResult.ErrorCode;
                    result.WarningCode = exportResult.WarningCode;
                    result.Message = exportResult.Message;
                }
                finally
                {
                    CloseDocument(model);
                }
            }
            catch (Exception ex)
            {
                result.Message = $"异常: {ex.Message}";
            }

            result.Duration = DateTime.Now - startTime;
            return result;
        }

        public ModelDoc2 OpenDocument(string sourcePath)
        {
            try
            {
                var extension = Path.GetExtension(sourcePath).ToLowerInvariant();
                int docType;

                switch (extension)
                {
                    case ".sldprt":
                        docType = (int)swDocumentTypes_e.swDocPART;
                        break;
                    case ".sldasm":
                        docType = (int)swDocumentTypes_e.swDocASSEMBLY;
                        break;
                    default:
                        _logger.LogError($"不支持的文件类型: {extension}");
                        return null;
                }

                _logger.Log($"尝试打开文件: {sourcePath}");

                ModelDoc2 model = TryOpenDocument(sourcePath, docType);

                if (model == null)
                {
                    _logger.LogError($"无法打开文件: {sourcePath}");
                    return null;
                }

                _logger.Log($"已打开文档: {Path.GetFileName(sourcePath)}");
                return model;
            }
            catch (Exception ex)
            {
                _logger.LogError($"打开文档失败: {ex.Message}");
                LogExceptionChain(ex);
                return null;
            }
        }

        /// <summary>
        /// OpenDoc6：推荐入口（路径、类型、选项位掩码、配置名、错误/警告）。
        /// OpenDoc3（新版）：布尔开关 + ref Errors，不再是旧的 Options+Configuration 形式。
        /// OpenDoc：仅 Name + Type。
        /// </summary>
        private ModelDoc2 TryOpenDocument(string sourcePath, int docType)
        {
            int errors = 0;
            int warnings = 0;

            try
            {
                ModelDoc2 doc = _swApp.OpenDoc6(
                    sourcePath,
                    docType,
                    (int)swOpenDocOptions_e.swOpenDocOptions_Silent,
                    "",
                    ref errors,
                    ref warnings);

                _logger.Log($"OpenDoc6: swFileLoadError={errors}, swFileLoadWarning={warnings}");

                if (doc != null)
                {
                    _logger.Log("使用 OpenDoc6 成功");
                    return doc;
                }
            }
            catch (Exception ex)
            {
                _logger.LogWarning($"OpenDoc6 失败: {ex.Message}");
                LogExceptionChain(ex);
            }

            try
            {
                int err3 = 0;
                object o = _swApp.OpenDoc3(
                    sourcePath,
                    docType,
                    false,
                    false,
                    false,
                    true,
                    ref err3);

                _logger.Log($"OpenDoc3: Errors={err3}");

                if (o is ModelDoc2 m)
                {
                    _logger.Log("使用 OpenDoc3 成功");
                    return m;
                }
            }
            catch (Exception ex)
            {
                _logger.LogWarning($"OpenDoc3 失败: {ex.Message}");
                LogExceptionChain(ex);
            }

            try
            {
                object o = _swApp.OpenDoc(sourcePath, docType);
                if (o is ModelDoc2 m)
                {
                    _logger.Log("使用 OpenDoc 成功");
                    return m;
                }
            }
            catch (Exception ex)
            {
                _logger.LogWarning($"OpenDoc 失败: {ex.Message}");
                LogExceptionChain(ex);
            }

            return null;
        }

        private void LogExceptionChain(Exception ex)
        {
            for (Exception e = ex.InnerException; e != null; e = e.InnerException)
            {
                if (e is COMException ce)
                    _logger.LogError($"COM HRESULT: 0x{(uint)ce.ErrorCode:X8} — {ce.Message}");
                else
                    _logger.LogError($"内部异常: {e.Message}");
            }
        }

        private void TryLogDocumentPath(ModelDoc2 model)
        {
            try
            {
                string p = model.GetPathName();
                if (!string.IsNullOrEmpty(p))
                    _logger.Log($"当前文档路径: {p}");
            }
            catch
            {
                /* ignore */
            }
        }

        /// <summary>SolidWorks 保存后文件句柄可能略有延迟，装配体写入 STL 可能较慢。</summary>
        private static bool WaitForNonEmptyFile(string path, int timeoutMs)
        {
            Stopwatch sw = Stopwatch.StartNew();
            while (sw.ElapsedMilliseconds < timeoutMs)
            {
                try
                {
                    if (File.Exists(path))
                    {
                        var fi = new FileInfo(path);
                        if (fi.Length > 0)
                            return true;
                    }
                }
                catch
                {
                    /* ignore */
                }

                Thread.Sleep(100);
            }

            return false;
        }

        private void PrepareModelForExport(ModelDoc2 model)
        {
            // 与手动操作尽量一致：激活文档 -> 装配体解析轻化组件 -> 重建
            try
            {
                int activateError = 0;
                object activated = _swApp.ActivateDoc3(model.GetTitle(), true, 0, ref activateError);
                if (activated == null && activateError != 0)
                    _logger.LogWarning($"ActivateDoc3 返回错误码: {activateError}");
            }
            catch (Exception ex)
            {
                _logger.LogWarning($"ActivateDoc3 失败: {ex.Message}");
            }

            if (model is AssemblyDoc asm)
            {
                try
                {
                    int lightCount = asm.GetLightWeightComponentCount();
                    if (lightCount > 0)
                    {
                        _logger.Log($"检测到轻化组件数量: {lightCount}，尝试全部解析...");
                        int resolveCode = asm.ResolveAllLightWeightComponents(true);
                        _logger.Log($"ResolveAllLightWeightComponents 返回: {resolveCode}");
                    }

                    bool resolved = asm.ResolveAllLightweight();
                    bool allResolved = asm.LightweightAllResolved();
                    _logger.Log($"ResolveAllLightweight={resolved}, LightweightAllResolved={allResolved}");
                }
                catch (Exception ex)
                {
                    _logger.LogWarning($"装配体轻化解析失败: {ex.Message}");
                }
            }

            try
            {
                model.EditRebuild3();
            }
            catch (Exception ex)
            {
                _logger.LogWarning($"EditRebuild3 失败: {ex.Message}");
            }

            try
            {
                model.ForceRebuild3(true);
            }
            catch (Exception ex)
            {
                _logger.LogWarning($"ForceRebuild3 失败: {ex.Message}");
            }
        }

        private bool SaveStl(ModelDocExtension extension, string stlPath, ref int stlErrs, ref int stlWarns)
        {
            // 对装配体导 STL，强制输出“单文件”，否则 SaveAs2 可能成功但目标文件不存在（拆分到多文件）。
            int prefId = (int)swUserPreferenceToggle_e.swSTLComponentsIntoOneFile;
            bool oldSingleFile = false;
            bool prefRead = false;

            try
            {
                oldSingleFile = _swApp.GetUserPreferenceToggle(prefId);
                prefRead = true;
            }
            catch (Exception ex)
            {
                _logger.LogWarning($"读取 STL 单文件偏好失败: {ex.Message}");
            }

            try
            {
                _swApp.SetUserPreferenceToggle(prefId, true);
            }
            catch (Exception ex)
            {
                _logger.LogWarning($"设置 STL 单文件偏好失败: {ex.Message}");
            }

            try
            {
                return extension.SaveAs2(
                    stlPath,
                    (int)swSaveAsVersion_e.swSaveAsCurrentVersion,
                    (int)swSaveAsOptions_e.swSaveAsOptions_Silent,
                    null,
                    "",
                    false,
                    ref stlErrs,
                    ref stlWarns);
            }
            finally
            {
                if (prefRead)
                {
                    try
                    {
                        _swApp.SetUserPreferenceToggle(prefId, oldSingleFile);
                    }
                    catch (Exception ex)
                    {
                        _logger.LogWarning($"恢复 STL 单文件偏好失败: {ex.Message}");
                    }
                }
            }
        }

        public ConversionResult ExportToFbx(ModelDoc2 model, string outputPath)
        {
            var result = new ConversionResult
            {
                OutputPath = outputPath,
                Success = false
            };

            // 固定 OBJ 输出：SaveAs2 -> 临时 STL -> Assimp OBJ（并删除同名 MTL）。
            string tempStl = Path.Combine(Path.GetTempPath(), "sw2fbx_" + Guid.NewGuid().ToString("N") + ".stl");

            try
            {
                PrepareModelForExport(model);

                ModelDocExtension extension = model.Extension;
                if (extension == null)
                {
                    result.Message = "无法获取 ModelDocExtension";
                    return result;
                }

                int stlErrs = 0;
                int stlWarns = 0;

                _logger.Log($"中间步骤: 导出 STL → {tempStl}");

                bool stlOk = SaveStl(extension, tempStl, ref stlErrs, ref stlWarns);

                result.ErrorCode = stlErrs;
                result.WarningCode = stlWarns;

                if (!stlOk)
                {
                    result.Message =
                        $"SOLIDWORKS 导出 STL 失败（SaveAs2=false, swFileSaveError=0x{stlErrs:X}）。";
                    TryLogDocumentPath(model);
                    _logger.LogError(result.Message);
                    return result;
                }

                if (!WaitForNonEmptyFile(tempStl, 8000))
                {
                    // 装配体常出现 SaveAs2=true 但首轮尚未落盘，执行一次强化重试
                    if (model is AssemblyDoc)
                    {
                        _logger.LogWarning("首轮 STL 未落盘，检测到装配体，执行预处理后重试导出...");
                        PrepareModelForExport(model);
                        stlErrs = 0;
                        stlWarns = 0;
                        stlOk = SaveStl(extension, tempStl, ref stlErrs, ref stlWarns);
                        result.ErrorCode = stlErrs;
                        result.WarningCode = stlWarns;
                    }

                    if (!stlOk || !WaitForNonEmptyFile(tempStl, 12000))
                    {
                        result.Message =
                            $"SOLIDWORKS 报告 SaveAs2 结果异常（ok={stlOk}, err=0x{stlErrs:X}），且未在磁盘上找到 STL：{tempStl}。";
                        TryLogDocumentPath(model);
                        _logger.LogError(result.Message);
                        return result;
                    }
                }

                if (stlWarns != 0)
                {
                    _logger.LogWarning($"STL 导出警告位掩码: 0x{stlWarns:X}");
                }

                _logger.Log("STL 已生成，正在通过 Assimp 导出 OBJ（将自动删除同名 .mtl）...");

                MeshExportResult mesh = StlToFbxConverter.ExportFromStl(tempStl, outputPath, _logger.Log);
                if (!mesh.Success)
                {
                    result.Message = mesh.Message ?? "网格导出失败";
                    _logger.LogError(result.Message);
                    return result;
                }

                result.Success = true;
                result.OutputPath = mesh.ActualOutputPath ?? outputPath;
                result.Message = mesh.Message ?? "导出成功";
                return result;
            }
            catch (Exception ex)
            {
                result.Message = $"导出异常: {ex.Message}";
                _logger.LogError($"导出异常: {ex.Message}");
                LogExceptionChain(ex);
                return result;
            }
            finally
            {
                try
                {
                    if (File.Exists(tempStl))
                        File.Delete(tempStl);
                }
                catch
                {
                    /* ignore */
                }
            }
        }

        public void CloseDocument(ModelDoc2 model)
        {
            try
            {
                if (model != null && _swApp != null)
                {
                    string title = model.GetTitle();
                    _swApp.CloseDoc(title);
                    Marshal.ReleaseComObject(model);
                    _logger.Log($"已关闭文档: {title}");
                }
            }
            catch (Exception ex)
            {
                _logger.LogWarning($"关闭文档时发生错误: {ex.Message}");
            }
        }

        public void ShutdownSolidWorks()
        {
            _logger.Log("正在关闭 SOLIDWORKS...");

            try
            {
                if (_swApp != null)
                {
                    try
                    {
                        _swApp.ExitApp();
                        _logger.Log("已调用 ExitApp()");
                    }
                    catch (Exception ex)
                    {
                        _logger.LogWarning($"ExitApp 失败: {ex.Message}");
                    }

                    try
                    {
                        Marshal.ReleaseComObject(_swApp);
                        _logger.Log("已释放 COM 对象");
                    }
                    catch (Exception ex)
                    {
                        _logger.LogWarning($"释放 COM 对象失败: {ex.Message}");
                    }

                    _swApp = null;
                }
            }
            catch (Exception ex)
            {
                _logger.LogError($"关闭 SOLIDWORKS COM 失败: {ex.Message}");
            }

            Thread.Sleep(2000);

            KillExistingSolidWorksProcesses();

            _logger.LogSuccess("SOLIDWORKS 已完全退出");
        }

        public void Cancel()
        {
            _isRunning = false;
            _logger.Log("用户取消操作");
        }

        private void OnFileProcessing(string filePath)
        {
            FileProcessing?.Invoke(this, filePath);
        }

        private void OnProgressChanged(int progress)
        {
            ProgressChanged?.Invoke(this, progress);
        }
    }
}
