#include "Logger.h"
#include <iostream>
#include <sstream>
#include <ctime>
#include <direct.h>
#include <errno.h>
#include <array>
#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")
#ifdef ERROR
#undef ERROR
#endif
#endif

static std::string g_logPrefix = "STL_converter";

std::string CreateLogDirectory() {
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    struct tm timeinfo;
    localtime_s(&timeinfo, &now_c);
    
    char cwd[256];
    _getcwd(cwd, sizeof(cwd));
    
    char baseDirBuffer[300];
    sprintf(baseDirBuffer, "%s\\logs", cwd);
    std::string baseDir = baseDirBuffer;
    _mkdir(baseDir.c_str());
    
    char dirBuffer[350];
    sprintf(dirBuffer, "%s\\logs\\%04d-%02d-%02d",
            cwd,
            timeinfo.tm_year + 1900,
            timeinfo.tm_mon + 1,
            timeinfo.tm_mday);
    
    std::string logDir = dirBuffer;
    _mkdir(logDir.c_str());
    
    return logDir;
}

std::string GenerateLogFileName(const std::string& logDir) {
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    struct tm timeinfo;
    localtime_s(&timeinfo, &now_c);
    
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    
    char fileBuffer[150];
    sprintf(fileBuffer, "%s\\%s_%02d-%02d-%02d_%03d.log",
            logDir.c_str(),
            g_logPrefix.c_str(),
            timeinfo.tm_hour,
            timeinfo.tm_min,
            timeinfo.tm_sec,
            static_cast<int>(ms.count()));
    
    return std::string(fileBuffer);
}

static bool IsKeyInfoMessage(const std::string& message) {
    static const std::array<const char*, 3> kPrefixes = {
        "Processing file:",
        "File summary:",
        "Found "
    };
    for (const char* prefix : kPrefixes) {
        if (message.rfind(prefix, 0) == 0) {
            return true;
        }
    }
    return false;
}

#ifdef _WIN32
static std::uint64_t FileTimeToUint64(const FILETIME& ft) {
    ULARGE_INTEGER li;
    li.LowPart = ft.dwLowDateTime;
    li.HighPart = ft.dwHighDateTime;
    return li.QuadPart;
}

static bool QueryProcessSnapshot(std::uint64_t& cpu100ns,
                                 std::uint64_t& workingSetBytes,
                                 std::uint64_t& peakWorkingSetBytes,
                                 std::uint64_t& totalPhysBytes) {
    HANDLE process = GetCurrentProcess();
    FILETIME createTime, exitTime, kernelTime, userTime;
    if (!GetProcessTimes(process, &createTime, &exitTime, &kernelTime, &userTime)) {
        return false;
    }
    cpu100ns = FileTimeToUint64(kernelTime) + FileTimeToUint64(userTime);

    PROCESS_MEMORY_COUNTERS_EX mem = {};
    mem.cb = sizeof(mem);
    if (!GetProcessMemoryInfo(process, reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&mem), sizeof(mem))) {
        return false;
    }
    workingSetBytes = static_cast<std::uint64_t>(mem.WorkingSetSize);
    peakWorkingSetBytes = static_cast<std::uint64_t>(mem.PeakWorkingSetSize);

    MEMORYSTATUSEX st = {};
    st.dwLength = sizeof(st);
    if (!GlobalMemoryStatusEx(&st)) {
        return false;
    }
    totalPhysBytes = static_cast<std::uint64_t>(st.ullTotalPhys);
    return true;
}
#endif

Logger::Logger() {
    std::string logDir = CreateLogDirectory();
    std::string logFile = GenerateLogFileName(logDir);
    m_logFile.open(logFile, std::ios::out);
}

Logger::~Logger() {
    if (m_logFile.is_open()) {
        m_logFile.close();
    }
}

Logger& Logger::GetInstance() {
    static Logger instance;
    return instance;
}

void Logger::SetLogPrefix(const std::string& prefix) {
    if (!prefix.empty()) {
        g_logPrefix = prefix;
    }
}

void Logger::SetLogFile(const std::string& filePath) {
    if (m_logFile.is_open()) {
        m_logFile.close();
    }
    m_logFile.open(filePath, std::ios::out | std::ios::app);
}

void Logger::SetCurrentFile(const std::string& fileName) {
    m_currentFile = fileName;
    m_fileStartTime = std::chrono::high_resolution_clock::now();
    
#ifdef _WIN32
    std::uint64_t cpu = 0;
    std::uint64_t ws = 0;
    std::uint64_t peak = 0;
    std::uint64_t total = 0;
    if (QueryProcessSnapshot(cpu, ws, peak, total)) {
        m_fileStartCpuTime100ns = cpu;
        m_fileStartWorkingSetBytes = ws;
        m_filePeakWorkingSetBytes = peak;
        m_totalPhysicalBytes = total;
    } else {
        m_fileStartCpuTime100ns = 0;
        m_fileStartWorkingSetBytes = 0;
        m_filePeakWorkingSetBytes = 0;
        m_totalPhysicalBytes = 0;
    }
#endif

    std::string separator = "=================================================";
    
    if (m_logFile.is_open()) {
        m_logFile << separator << std::endl;
    }
    std::cout << separator << std::endl;
    
    LogInfo("Processing file: " + fileName);
}

std::string Logger::GetTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    struct tm timeinfo;
    localtime_s(&timeinfo, &now_c);
    
    char buffer[20];
    sprintf(buffer, "%04d-%02d-%02d %02d:%02d:%02d",
            timeinfo.tm_year + 1900,
            timeinfo.tm_mon + 1,
            timeinfo.tm_mday,
            timeinfo.tm_hour,
            timeinfo.tm_min,
            timeinfo.tm_sec);
    
    return std::string(buffer);
}

std::string Logger::GetLevelString(LogLevel level) {
    switch (level) {
        case LogLevel::INFO:    return "INFO";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::ERROR:   return "ERROR";
        default:                return "UNKNOWN";
    }
}

void Logger::Log(LogLevel level, const std::string& message) {
    if (level == LogLevel::INFO && !IsKeyInfoMessage(message)) {
        return;
    }

    std::string logLine = "[" + GetTimestamp() + "] [" + GetLevelString(level) + "] " + message;
    
    if (m_logFile.is_open()) {
        m_logFile << logLine << std::endl;
    }
    
    switch (level) {
        case LogLevel::ERROR:
            std::cerr << logLine << std::endl;
            break;
        case LogLevel::WARNING:
            std::cout << logLine << std::endl;
            break;
        default:
            std::cout << logLine << std::endl;
            break;
    }
}

void Logger::LogInfo(const std::string& message) {
    Log(LogLevel::INFO, message);
}

void Logger::LogWarning(const std::string& message) {
    Log(LogLevel::WARNING, message);
}

void Logger::LogError(const std::string& message) {
    Log(LogLevel::ERROR, message);
}

void Logger::StartTimer(const std::string& phaseName) {
    m_startTime = std::chrono::high_resolution_clock::now();
    m_currentPhase = phaseName;
}

void Logger::EndTimer(const std::string& phaseName) {
    (void)phaseName;
    (void)m_startTime;
}

void Logger::FinishCurrentFile(bool success, const std::string& errorMessage) {
    if (m_currentFile.empty()) {
        return;
    }

    const auto endTime = std::chrono::high_resolution_clock::now();
    const auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - m_fileStartTime);
    const double durationSec = durationMs.count() / 1000.0;

    double cpuPercent = 0.0;
    double memoryPercent = 0.0;
    std::uint64_t memoryBytes = 0;

#ifdef _WIN32
    std::uint64_t endCpu100ns = 0;
    std::uint64_t endWsBytes = 0;
    std::uint64_t endPeakBytes = 0;
    std::uint64_t endTotalPhys = 0;
    if (QueryProcessSnapshot(endCpu100ns, endWsBytes, endPeakBytes, endTotalPhys)) {
        const std::uint64_t deltaCpu100ns =
            (endCpu100ns >= m_fileStartCpuTime100ns) ? (endCpu100ns - m_fileStartCpuTime100ns) : 0;
        const double wallSec = durationSec > 0.0 ? durationSec : 0.0;
        SYSTEM_INFO si = {};
        GetSystemInfo(&si);
        const unsigned int cores = si.dwNumberOfProcessors > 0 ? si.dwNumberOfProcessors : 1;
        if (wallSec > 0.0) {
            const double cpuSec = static_cast<double>(deltaCpu100ns) / 10000000.0;
            cpuPercent = (cpuSec / (wallSec * static_cast<double>(cores))) * 100.0;
            if (cpuPercent < 0.0) cpuPercent = 0.0;
        }

        memoryBytes = (endPeakBytes > m_filePeakWorkingSetBytes) ? endPeakBytes : m_filePeakWorkingSetBytes;
        const std::uint64_t totalPhys = endTotalPhys > 0 ? endTotalPhys : m_totalPhysicalBytes;
        if (totalPhys > 0) {
            memoryPercent = (static_cast<double>(memoryBytes) / static_cast<double>(totalPhys)) * 100.0;
        }
    }
#endif

    std::ostringstream summary;
    summary.setf(std::ios::fixed);
    summary.precision(3);
    summary << "File summary: status=" << (success ? "OK" : "FAIL")
            << ", total=" << durationSec << " s";
    summary.precision(2);
    summary << ", cpu=" << cpuPercent << "%"
            << ", memory=" << memoryPercent << "%";
    if (memoryBytes > 0) {
        summary << " (peak_ws=" << (static_cast<double>(memoryBytes) / (1024.0 * 1024.0)) << " MB)";
    }
    if (!success && !errorMessage.empty()) {
        summary << ", error=" << errorMessage;
    }

    LogInfo(summary.str());
    m_currentFile.clear();
}
