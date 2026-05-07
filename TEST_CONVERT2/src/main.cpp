#include "Converter.h"
#include "MeshSettings.h"
#include "Logger.h"
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <cstring>
#include <BRepPrimAPI_MakeBox.hxx>
#include <STEPControl_Writer.hxx>
#include <Interface_Static.hxx>
#include <pxr/base/plug/registry.h>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

//! Root dir passed to PXR_PLUGINPATH_NAME (must contain plugInfo.json). Empty if unknown.
static fs::path ResolveUsdPluginRoot(const fs::path& exeDir) {
    const fs::path portable[] = {
        exeDir / "plugin" / "usd",
        exeDir / "usd",
    };
    for (const fs::path& c : portable) {
        if (fs::is_regular_file(c / "plugInfo.json")) {
            return c.lexically_normal();
        }
    }
    fs::path walk = exeDir;
    for (int depth = 0; depth < 20 && !walk.empty(); ++depth, walk = walk.parent_path()) {
        const fs::path sdkUsd = walk / "openusd" / "lib" / "usd";
        if (fs::is_regular_file(sdkUsd / "plugInfo.json")) {
            return sdkUsd.lexically_normal();
        }
    }
#ifdef USD_FALLBACK_PLUGIN_ROOT
    {
        fs::path fb(USD_FALLBACK_PLUGIN_ROOT);
        if (fs::is_regular_file(fb / "plugInfo.json")) {
            return fb.lexically_normal();
        }
    }
#endif
    return {};
}

//! Directory containing the running EXE (for portable deployment: DLLs + USD plugins live beside it).
static std::string GetExecutableDirectory() {
#ifdef _WIN32
    wchar_t buf[MAX_PATH];
    const DWORD len = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) {
        return {};
    }
    std::filesystem::path p(buf);
    return p.parent_path().string();
#else
    return {};
#endif
}

static void InitializeEnvironment() {
#ifdef _WIN32
    const std::string exeDir = GetExecutableDirectory();
    if (!exeDir.empty()) {
        SetDllDirectoryA(exeDir.c_str());
    }

    std::filesystem::path base(exeDir);

    std::string libPosix = base.lexically_normal().string();
    std::replace(libPosix.begin(), libPosix.end(), '\\', '/');

    static std::string envPlugin;
    static std::string envLib;
    static std::string envPythonHome;

    envLib = "USD_LIBRARY_PATH=" + libPosix;
    _putenv(envLib.c_str());
    SetEnvironmentVariableA("USD_LIBRARY_PATH", libPosix.c_str());

    const fs::path pluginRoot = ResolveUsdPluginRoot(base);
    if (!pluginRoot.empty()) {
        std::string pluginUsdPosix = pluginRoot.string();
        std::replace(pluginUsdPosix.begin(), pluginUsdPosix.end(), '\\', '/');
        envPlugin = "PXR_PLUGINPATH_NAME=" + pluginUsdPosix;
        _putenv(envPlugin.c_str());
        SetEnvironmentVariableA("PXR_PLUGINPATH_NAME", pluginUsdPosix.c_str());
    } else {
        _putenv("PXR_PLUGINPATH_NAME=");
        SetEnvironmentVariableA("PXR_PLUGINPATH_NAME", nullptr);
    }

    if (!pluginRoot.empty()) {
        const fs::path plugInfoFile = pluginRoot / "plugInfo.json";
        if (fs::is_regular_file(plugInfoFile)) {
            pxr::PlugRegistry::GetInstance().RegisterPlugins(plugInfoFile.string());
        }
    }

    // Bundled layout: <exe_dir>/python/Lib (same tree as OpenUSD python bundle).
    // Helps avoid crashes when usd_python/python312 initializes without a valid home.
    const fs::path pyHomeCandidate = (base / "python").lexically_normal();
    if (fs::is_directory(pyHomeCandidate / "Lib")) {
        std::string pyHomeStr = pyHomeCandidate.string();
        std::replace(pyHomeStr.begin(), pyHomeStr.end(), '\\', '/');
        envPythonHome = "PYTHONHOME=" + pyHomeStr;
        _putenv(envPythonHome.c_str());
    }
#else
    _putenv("PXR_PLUGINPATH_NAME=");
    _putenv("USD_LIBRARY_PATH=");
#endif
}

bool CreateTestStepFile(const std::string& outputPath) {
    try {
        TopoDS_Shape box = BRepPrimAPI_MakeBox(10.0, 10.0, 10.0);
        
        STEPControl_Writer writer;
        Interface_Static::SetCVal("write.step.schema", "AP203");
        writer.Transfer(box, STEPControl_AsIs);
        
        IFSelect_ReturnStatus status = writer.Write(outputPath.c_str());
        
        if (status == IFSelect_RetDone) {
            std::cout << "[OK] Test STEP file created: " << outputPath << std::endl;
            return true;
        } else {
            std::cerr << "[FAIL] Failed to write STEP file" << std::endl;
            return false;
        }
    } catch (const std::exception& e) {
        std::cerr << "[FAIL] Exception: " << e.what() << std::endl;
        return false;
    }
}

void PrintUsage(const char* programName) {
    std::cout << "STEP to STL Converter - Open Cascade Technology based\n"
              << "Usage:\n"
              << "  " << programName << " [options] <input.step> <output.stl>\n"
              << "  " << programName << " [options] -batch <input_dir> <output_dir>\n"
              << "  " << programName << " -test <output.step>\n"
              << "\nOptions:\n"
              << "  -d, --deflection <value>   Mesh deflection (default: 0.1)\n"
              << "  -a, --angle <value>        Angular tolerance in radians (default: 0.5)\n"
              << "  -r, --relative             Use relative deflection (default: true)\n"
              << "  -f, --fixed                Use fixed deflection (overrides -r)\n"
              << "  -max, --max-triangles <n>  Maximum number of triangles (default: 1000000)\n"
              << "  -batch                     Enable batch processing mode\n"
              << "  -test                      Generate a test STEP file (10x10x10 box)\n"
              << "  -h, --help                 Show this help message\n"
              << "\nExamples:\n"
              << "  " << programName << " input.step output.stl\n"
              << "  " << programName << " -d 0.01 -a 0.3 input.step output.stl\n"
              << "  " << programName << " -batch ./step_files ./stl_output\n"
              << "  " << programName << " -test test_box.step\n"
              << std::endl;
}

bool GetStepFilesInDirectory(const std::string& dirPath, std::vector<std::string>& files) {
    files.clear();
    
    try {
        if (!fs::exists(dirPath)) {
            std::cerr << "Error: Directory does not exist: " << dirPath << std::endl;
            return false;
        }

        for (const auto& entry : fs::directory_iterator(dirPath)) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                
                if (ext == ".step" || ext == ".stp") {
                    files.push_back(entry.path().string());
                }
            }
        }

        std::sort(files.begin(), files.end());
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Error reading directory: " << e.what() << std::endl;
        return false;
    }
}

int main(int argc, char* argv[]) {
    InitializeEnvironment();

    if (argc < 2) {
        PrintUsage(argv[0]);
        return 1;
    }

    MeshSettings settings;
    std::string inputFile;
    std::string outputFile;
    std::string inputDir;
    std::string outputDir;
    bool batchMode = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            PrintUsage(argv[0]);
            return 0;
        }
        if (arg == "-d" || arg == "--deflection") {
            if (i + 1 < argc) {
                settings.deflection = std::stod(argv[++i]);
            }
            continue;
        }
        if (arg == "-a" || arg == "--angle") {
            if (i + 1 < argc) {
                settings.angle = std::stod(argv[++i]);
            }
            continue;
        }
        if (arg == "-r" || arg == "--relative") {
            settings.relative = true;
            continue;
        }
        if (arg == "-f" || arg == "--fixed") {
            settings.relative = false;
            continue;
        }
        if (arg == "-max" || arg == "--max-triangles") {
            if (i + 1 < argc) {
                settings.maxNbTriangles = std::stoi(argv[++i]);
            }
            continue;
        }
        if (arg == "-batch") {
            batchMode = true;
            if (i + 2 >= argc) {
                std::cerr << "Error: -batch requires <input_dir> <output_dir>" << std::endl;
                return 1;
            }
            inputDir = argv[++i];
            outputDir = argv[++i];
            continue;
        }
        if (arg == "-test") {
            if (i + 1 < argc) {
                std::string testOutput = argv[++i];
                return CreateTestStepFile(testOutput) ? 0 : 1;
            }
            std::cerr << "Error: -test requires an output file path" << std::endl;
            return 1;
        }
        if (inputFile.empty()) {
            inputFile = arg;
            continue;
        }
        if (outputFile.empty()) {
            outputFile = arg;
            continue;
        }

        std::cerr << "Error: Unexpected argument: " << arg << std::endl;
        PrintUsage(argv[0]);
        return 1;
    }

    std::string logPrefix = "STL_converter";
    if (batchMode) {
        std::string outLower = outputDir;
        std::transform(outLower.begin(), outLower.end(), outLower.begin(), ::tolower);
        if (outLower.find("usd") != std::string::npos) {
            logPrefix = "USD_converter";
        }
    } else {
        std::string ext = fs::path(outputFile).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".usd" || ext == ".usda" || ext == ".usdc") {
            logPrefix = "USD_converter";
        }
    }
    Logger::SetLogPrefix(logPrefix);

    Converter converter;
    converter.SetMeshSettings(settings);

    if (batchMode) {
        if (inputDir.empty() || outputDir.empty()) {
            std::cerr << "Error: -batch requires both input and output directories." << std::endl;
            return 1;
        }
        try {
            inputDir = fs::absolute(inputDir).lexically_normal().string();
            outputDir = fs::absolute(outputDir).lexically_normal().string();
        } catch (const std::exception& e) {
            std::cerr << "Error: Invalid batch paths: " << e.what() << std::endl;
            return 1;
        }
        std::cout << "[BATCH] input_dir=" << inputDir << std::endl;
        std::cout << "[BATCH] output_dir=" << outputDir << std::endl;

        std::vector<std::string> stepFiles;
        if (!GetStepFilesInDirectory(inputDir, stepFiles)) {
            return 1;
        }

        if (stepFiles.empty()) {
            std::cerr << "Error: No STEP files found in directory: " << inputDir << std::endl;
            return 1;
        }

        const std::string foundMsg =
            "Found " + std::to_string(stepFiles.size()) + " STEP file(s) to process...";
        std::cout << foundMsg << std::endl;
        Logger::GetInstance().LogInfo(foundMsg);

        std::vector<ConversionResult> results;
        
        auto progressCallback = [](size_t current, size_t total) {
            std::cout << "\rProcessing: " << current << "/" << total << " (" 
                      << (current * 100 / total) << "%)" << std::flush;
        };

        bool success = converter.ConvertBatch(stepFiles, outputDir, results, progressCallback);
        std::cout << std::endl;

        if (!success) {
            std::cerr << "Batch conversion failed: " << converter.GetLastError() << std::endl;
            return 1;
        }

        int successCount = 0;
        int failCount = 0;
        
        for (const auto& result : results) {
            if (result.success) {
                std::cout << "[OK] " << result.inputFile << " -> " << result.outputFile << std::endl;
                successCount++;
            } else {
                std::cerr << "[FAIL] " << result.inputFile << ": " << result.errorMessage << std::endl;
                failCount++;
            }
        }

        std::cout << "\nConversion completed: " << successCount << " succeeded, " << failCount << " failed" << std::endl;
        
        return failCount > 0 ? 1 : 0;
    }
    else {
        if (inputFile.empty() || outputFile.empty()) {
            PrintUsage(argv[0]);
            return 1;
        }

        std::cout << "Converting " << inputFile << " to " << outputFile << std::endl;
        std::cout << "Mesh settings: deflection=" << settings.deflection 
                  << ", angle=" << settings.angle
                  << ", relative=" << std::boolalpha << settings.relative << std::endl;

        ConversionResult result = converter.ConvertWithResult(inputFile, outputFile);

        if (result.success) {
            std::cout << "[OK] Conversion successful!" << std::endl;
            return 0;
        } else {
            std::cerr << "[FAIL] Conversion failed: " << result.errorMessage << std::endl;
            return 1;
        }
    }
}