using System;
using System.IO;
using System.Text;
using System.Windows.Forms;

namespace SolidWorksBatchConverter
{
    public class Logger
    {
        private readonly string _logFilePath;
        private readonly RichTextBox _outputTextBox;
        private readonly object _lock = new object();
        private readonly bool _metricsOnly;
        private readonly bool _echoToConsole;

        public Logger(string rootDirectory, RichTextBox outputTextBox = null, bool metricsOnly = false, bool echoToConsole = true)
        {
            _outputTextBox = outputTextBox;
            _metricsOnly = metricsOnly;
            _echoToConsole = echoToConsole;
            Directory.CreateDirectory(rootDirectory);
            _logFilePath = Path.Combine(rootDirectory, $"ConversionLog_{DateTime.Now:yyyyMMdd_HHmmss}.txt");
        }

        public void Log(string message)
        {
            if (_metricsOnly)
                return;
            var logEntry = $"[{DateTime.Now:yyyy-MM-dd HH:mm:ss}] {message}";
            WriteEntry(logEntry);
        }

        public void LogSummary(string message)
        {
            var logEntry = $"[{DateTime.Now:yyyy-MM-dd HH:mm:ss}] {message}";
            WriteEntry(logEntry);
        }

        private void WriteEntry(string logEntry)
        {
            lock (_lock)
            {
                File.AppendAllText(_logFilePath, logEntry + Environment.NewLine, Encoding.UTF8);

                if (_echoToConsole)
                {
                    Console.WriteLine(logEntry);
                }

                if (_outputTextBox != null && _outputTextBox.InvokeRequired)
                {
                    _outputTextBox.Invoke(new Action(() => AppendToTextBox(logEntry)));
                }
                else if (_outputTextBox != null)
                {
                    AppendToTextBox(logEntry);
                }
            }
        }

        public void LogError(string message)
        {
            Log($"[ERROR] {message}");
        }

        public void LogWarning(string message)
        {
            Log($"[WARNING] {message}");
        }

        public void LogSuccess(string message)
        {
            Log($"[SUCCESS] {message}");
        }

        private void AppendToTextBox(string text)
        {
            if (_outputTextBox.TextLength > 0)
            {
                _outputTextBox.AppendText(Environment.NewLine);
            }
            _outputTextBox.AppendText(text);
            _outputTextBox.ScrollToCaret();
        }

        public string GetLogFilePath()
        {
            return _logFilePath;
        }
    }
}