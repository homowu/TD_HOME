#pragma once

#include <string>
#include <vector>
#include <stdexcept>

enum class ErrorCode {
    None = 0,
    FileNotFound,
    InvalidFileFormat,
    GeometryError,
    MeshGenerationFailed,
    WriteFailed,
    InternalError,
    InvalidArgument
};

struct ErrorInfo {
    ErrorCode code;
    std::string message;
    std::string detail;
};

class ErrorHandler {
public:
    static std::string GetErrorMessage(ErrorCode code);
    static void ThrowException(ErrorCode code, const std::string& detail = "");
    static bool CheckFileExists(const std::string& filePath);
    static bool CheckFilePathValid(const std::string& filePath);
};