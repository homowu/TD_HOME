using System;

namespace SolidWorksBatchConverter
{
    public class ConversionResult
    {
        public string SourcePath { get; set; }
        public string OutputPath { get; set; }
        public bool Success { get; set; }
        public int ErrorCode { get; set; }
        public int WarningCode { get; set; }
        public string Message { get; set; }
        public TimeSpan Duration { get; set; }
    }
}