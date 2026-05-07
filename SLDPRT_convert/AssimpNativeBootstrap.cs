using System;
using System.IO;
using System.Runtime.InteropServices;

namespace SolidWorksBatchConverter
{
    /// <summary>
    /// 将 assimp.dll 所在目录加入进程 PATH，便于 AssimpNet P/Invoke 找到原生库。
    /// </summary>
    internal static class AssimpNativeBootstrap
    {
        public static void Apply()
        {
            string baseDir = AppDomain.CurrentDomain.BaseDirectory.TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
            string runtimesDir = Path.Combine(baseDir, "runtimes", "win-x64", "native");
            if (Directory.Exists(runtimesDir))
            {
                AddDirectoryToPath(runtimesDir);
                return;
            }

            if (File.Exists(Path.Combine(baseDir, "assimp.dll")))
                AddDirectoryToPath(baseDir);
        }

        private static void AddDirectoryToPath(string dir)
        {
            try
            {
                string path = Environment.GetEnvironmentVariable("PATH", EnvironmentVariableTarget.Process) ?? "";
                if (path.IndexOf(dir, StringComparison.OrdinalIgnoreCase) >= 0)
                    return;
                Environment.SetEnvironmentVariable("PATH", dir + Path.PathSeparator + path, EnvironmentVariableTarget.Process);
            }
            catch
            {
                /* ignore */
            }
        }
    }
}
