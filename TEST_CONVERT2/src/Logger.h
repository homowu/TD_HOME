#pragma once

#include <string>
#include <fstream>
#include <chrono>
#include <cstdint>

enum class LogLevel {
    INFO,
    WARNING,
    ERROR
};

class Logger {
public:
    static Logger& GetInstance();
    static void SetLogPrefix(const std::string& prefix);
    
    void SetLogFile(const std::string& filePath);
    void SetCurrentFile(const std::string& fileName);
    void Log(LogLevel level, const std::string& message);
    void LogInfo(const std::string& message);
    void LogWarning(const std::string& message);
    void LogError(const std::string& message);
    
    void StartTimer(const std::string& phaseName);
    void EndTimer(const std::string& phaseName);
    void FinishCurrentFile(bool success, const std::string& errorMessage);
    
private:
    Logger();
    ~Logger();
    
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    std::string GetTimestamp();
    std::string GetLevelString(LogLevel level);
    
    std::ofstream m_logFile;
    std::chrono::high_resolution_clock::time_point m_startTime;
    std::chrono::high_resolution_clock::time_point m_fileStartTime;
    std::string m_currentPhase;
    std::string m_currentFile;
    std::uint64_t m_fileStartCpuTime100ns = 0;
    std::uint64_t m_fileStartWorkingSetBytes = 0;
    std::uint64_t m_filePeakWorkingSetBytes = 0;
    std::uint64_t m_totalPhysicalBytes = 0;
};