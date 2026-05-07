#include "ErrorHandler.h"
#include <filesystem>

namespace fs = std::filesystem;

std::string ErrorHandler::GetErrorMessage(ErrorCode code) {
    switch (code) {
        case ErrorCode::None:
            return "No error";
        case ErrorCode::FileNotFound:
            return "File not found";
        case ErrorCode::InvalidFileFormat:
            return "Invalid file format";
        case ErrorCode::GeometryError:
            return "Geometry processing error";
        case ErrorCode::MeshGenerationFailed:
            return "Mesh generation failed";
        case ErrorCode::WriteFailed:
            return "Write operation failed";
        case ErrorCode::InternalError:
            return "Internal error";
        case ErrorCode::InvalidArgument:
            return "Invalid argument";
        default:
            return "Unknown error";
    }
}

void ErrorHandler::ThrowException(ErrorCode code, const std::string& detail) {
    std::string msg = GetErrorMessage(code);
    if (!detail.empty()) {
        msg += ": " + detail;
    }
    throw std::runtime_error(msg);
}

bool ErrorHandler::CheckFileExists(const std::string& filePath) {
    return fs::exists(filePath);
}

bool ErrorHandler::CheckFilePathValid(const std::string& filePath) {
    if (filePath.empty()) {
        return false;
    }
    
    try {
        fs::path path(filePath);
        return !path.empty();
    }
    catch (...) {
        return false;
    }
}