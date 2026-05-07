using System;
using System.Diagnostics;
using System.IO;
using System.Linq;

namespace SolidWorksBatchConverter
{
    static class Program
    {
        [STAThread]
        static int Main()
        {
            AssimpNativeBootstrap.Apply();

            string rootDir = AppDomain.CurrentDomain.BaseDirectory;
            string inputDir = Path.Combine(rootDir, "input");
            string outputDir = Path.Combine(rootDir, "output");

            Directory.CreateDirectory(outputDir);

            var logger = new Logger(rootDir, null, metricsOnly: true);
            var converter = new SolidWorksBatchConverter(logger);
            converter.FileProcessing += (_, filePath) =>
            {
                logger.LogSummary($"处理中: {Path.GetFileName(filePath)}");
            };
            converter.ProgressChanged += (_, progress) =>
            {
                logger.LogSummary($"进度: {progress}%");
            };

            var process = Process.GetCurrentProcess();
            TimeSpan cpuStart = process.TotalProcessorTime;
            Stopwatch totalSw = Stopwatch.StartNew();

            logger.LogSummary($"开始批处理: input={inputDir}, output={outputDir}");

            if (!Directory.Exists(inputDir))
            {
                logger.LogSummary("输入目录不存在：请在程序根目录创建 input 文件夹并放入 .sldprt/.sldasm 文件。");
                return 1;
            }

            try
            {
                if (!converter.StartSolidWorks())
                {
                    logger.LogSummary("启动 SOLIDWORKS 失败。");
                    return 2;
                }

                var results = converter.ConvertFolder(inputDir, outputDir);
                int total = results.Count;
                int success = results.Count(r => r.Success);
                int failed = total - success;

                for (int i = 0; i < total; i++)
                {
                    var r = results[i];
                    string fileName = Path.GetFileName(r.SourcePath ?? string.Empty);
                    string mark = r.Success ? "OK" : "FAIL";
                    logger.LogSummary($"[{i + 1}/{total}] {fileName} {mark} {r.Duration.TotalSeconds:F3}s");
                }

                totalSw.Stop();
                process.Refresh();
                TimeSpan cpuEnd = process.TotalProcessorTime;
                double elapsedMs = totalSw.Elapsed.TotalMilliseconds;
                double cpuMs = (cpuEnd - cpuStart).TotalMilliseconds;
                double avgCpuPercent = elapsedMs > 0
                    ? (cpuMs / (elapsedMs * Environment.ProcessorCount)) * 100.0
                    : 0.0;

                double wsMb = process.WorkingSet64 / 1024.0 / 1024.0;
                double peakWsMb = process.PeakWorkingSet64 / 1024.0 / 1024.0;
                double privateMb = process.PrivateMemorySize64 / 1024.0 / 1024.0;

                logger.LogSummary($"数量统计: 总数={total}, 成功={success}, 失败={failed}");
                logger.LogSummary($"总耗时: {totalSw.Elapsed.TotalSeconds:F3}s");
                logger.LogSummary($"CPU: 进程CPU时间={cpuEnd - cpuStart}, 平均占用={avgCpuPercent:F2}%");
                logger.LogSummary($"内存: WorkingSet={wsMb:F2}MB, PeakWorkingSet={peakWsMb:F2}MB, Private={privateMb:F2}MB");

                return failed == 0 ? 0 : 3;
            }
            catch (Exception ex)
            {
                logger.LogSummary($"异常: {ex.Message}");
                return 99;
            }
            finally
            {
                try
                {
                    converter.ShutdownSolidWorks();
                }
                catch
                {
                    // ignore
                }
            }
        }
    }
}