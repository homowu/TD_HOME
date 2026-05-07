#pragma once

#include "MeshSettings.h"
#include <TopoDS_Shape.hxx>
#include <string>
#include <vector>
#include <functional>

struct ConversionResult {
    std::string inputFile;
    std::string outputFile;
    bool success = false;
    std::string errorMessage;
    int warningCount = 0;
    std::vector<std::string> warnings;
};

enum class OutputFormat {
    STL,
    USD
};

class Converter {
public:
    Converter();
    ~Converter() = default;

    void SetMeshSettings(const MeshSettings& settings);
    const MeshSettings& GetMeshSettings() const { return m_meshSettings; }

    bool Convert(const std::string& stepFilePath, const std::string& outputFilePath);
    ConversionResult ConvertWithResult(const std::string& stepFilePath, const std::string& outputFilePath);
    
    bool ConvertBatch(const std::vector<std::string>& stepFiles, const std::string& outputDir,
                      std::vector<ConversionResult>& results,
                      std::function<void(size_t, size_t)> progressCallback = nullptr);

    const std::string& GetLastError() const { return m_lastError; }
    
    static OutputFormat DetectOutputFormat(const std::string& filePath);

private:
    MeshSettings m_meshSettings;
    std::string m_lastError;
    
    bool WriteOutput(const TopoDS_Shape& shape, const std::string& filePath, OutputFormat format);
};