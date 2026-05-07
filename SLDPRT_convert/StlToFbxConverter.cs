using System;
using System.IO;
using Assimp;

namespace SolidWorksBatchConverter
{
    internal sealed class MeshExportResult
    {
        public bool Success { get; set; }
        public string ActualOutputPath { get; set; }
        public string Message { get; set; }
    }

    /// <summary>
    /// SOLIDWORKS → STL 后，由 Assimp 写成 OBJ。
    /// </summary>
    internal static class StlToFbxConverter
    {
        private static readonly PostProcessSteps ImportFlags =
            PostProcessSteps.Triangulate
            | PostProcessSteps.JoinIdenticalVertices
            | PostProcessSteps.RemoveRedundantMaterials;

        /// <summary>从 STL 导出 OBJ，并删除同名 .mtl（按用户要求）。</summary>
        public static MeshExportResult ExportFromStl(string stlPath, string requestedOutputPath, Action<string> log)
        {
            var result = new MeshExportResult();
            string dir = Path.GetDirectoryName(requestedOutputPath);
            string baseName = Path.GetFileNameWithoutExtension(requestedOutputPath);

            if (string.IsNullOrEmpty(dir))
                dir = Directory.GetCurrentDirectory();

            try
            {
                using (var ctx = new AssimpContext())
                {
                    Scene scene = ctx.ImportFile(stlPath, ImportFlags);
                    if (scene == null)
                    {
                        result.Message = "Assimp 导入 STL 返回空场景";
                        return result;
                    }

                    string objPath = Path.Combine(dir, baseName + ".obj");
                    if (ctx.ExportFile(scene, objPath, "obj") && IsNonEmptyFile(objPath))
                    {
                        string mtlPath = Path.Combine(dir, baseName + ".mtl");
                        if (File.Exists(mtlPath))
                        {
                            try
                            {
                                File.Delete(mtlPath);
                                log?.Invoke($"已删除 MTL: {mtlPath}");
                            }
                            catch (Exception ex)
                            {
                                log?.Invoke($"[WARNING] 删除 MTL 失败: {ex.Message}");
                            }
                        }

                        result.Success = true;
                        result.ActualOutputPath = objPath;
                        result.Message = "导出成功：OBJ（已按要求移除同名 .mtl）";
                        return result;
                    }
                    result.Message = "Assimp 无法导出 OBJ。请确认 assimp.dll 已正确部署，且输出目录可写。";
                    return result;
                }
            }
            catch (Exception ex)
            {
                result.Message = "Assimp 异常: " + ex.Message;
                return result;
            }
        }

        private static bool IsNonEmptyFile(string path)
        {
            try
            {
                return File.Exists(path) && new FileInfo(path).Length > 0;
            }
            catch
            {
                return false;
            }
        }

    }
}
