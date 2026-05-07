#include "Converter.h"
#include "StepReader.h"
#include "MeshGenerator.h"
#include "StlWriter.h"
#include "UsdWriter.h"
#include "ErrorHandler.h"
#include "Logger.h"
#include <filesystem>
#include <algorithm>
#include <string>
#include <vector>
#include <future>
#include <atomic>
#include <thread>
#include <TopoDS_Shape.hxx>

namespace fs = std::filesystem;

Converter::Converter() {
    m_meshSettings.deflection = 0.1;
    m_meshSettings.angle = 0.5;
    m_meshSettings.relative = true;
    m_meshSettings.adaptive = true;
    m_meshSettings.maxNbTriangles = 1000000;
}

void Converter::SetMeshSettings(const MeshSettings& settings) {
    m_meshSettings = settings;
}

OutputFormat Converter::DetectOutputFormat(const std::string& filePath) {
    std::string ext = fs::path(filePath).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    if (ext == ".usd" || ext == ".usda" || ext == ".usdc") {
        return OutputFormat::USD;
    }
    return OutputFormat::STL;
}

bool Converter::Convert(const std::string& stepFilePath, const std::string& outputFilePath) {
    ConversionResult result = ConvertWithResult(stepFilePath, outputFilePath);
    m_lastError = result.errorMessage;
    return result.success;
}

ConversionResult Converter::ConvertWithResult(const std::string& stepFilePath, const std::string& outputFilePath) {
    ConversionResult result;
    result.inputFile = stepFilePath;
    result.outputFile = outputFilePath;
    result.success = false;

    try {
        Logger& logger = Logger::GetInstance();
        logger.SetCurrentFile(stepFilePath);
        struct FileLogScope {
            Logger& loggerRef;
            ConversionResult& resultRef;
            ~FileLogScope() {
                loggerRef.FinishCurrentFile(resultRef.success, resultRef.errorMessage);
            }
        } fileLogScope{logger, result};
        
        std::cout << "[STATUS] Reading STEP file: " << stepFilePath << std::endl;
        StepReader reader;

        OutputFormat format = DetectOutputFormat(outputFilePath);
        
        std::cout << "[DEBUG] Output format detected: " << (format == OutputFormat::USD ? "USD" : "STL") << std::endl;

        auto meshWithFastPath = [&logger, this](MeshGenerator& mesher,
                                                const TopoDS_Shape& inShape,
                                                TopoDS_Shape& outShape) -> bool {
            // Fast path: mesh directly first; only run expensive shape-fix when direct meshing fails.
            if (mesher.Generate(inShape, outShape)) {
                return true;
            }
            logger.LogWarning("Direct meshing failed, fallback to repair+mesh: " + mesher.GetLastError());
            return mesher.RepairAndMesh(inShape, outShape);
        };

        if (format == OutputFormat::USD) {
            logger.LogInfo(
                "USD: STEPCAF -> XCAF (colors / VisMaterial / material-table density); "
                "RepairAndMesh per leaf; ExtractMeshData same as STL; "
                "UsdPreviewSurface or displayColor (no synthetic UV/textures).");
            std::vector<NamedShape> namedShapes;
            if (reader.ReadAssemblyWithNames(stepFilePath, namedShapes)) {
                logger.LogInfo("STEP read as XCAF assembly: " + std::to_string(namedShapes.size()) + " parts");

                std::vector<size_t> leafIndices;
                leafIndices.reserve(namedShapes.size());

                std::vector<NamedShape> stagedPrims(namedShapes.size());
                std::vector<char> keep(namedShapes.size(), 0);
                for (size_t i = 0; i < namedShapes.size(); ++i) {
                    if (namedShapes[i].isAssemblyGroup) {
                        stagedPrims[i] = namedShapes[i];
                        keep[i] = 1;
                        continue;
                    }
                    leafIndices.push_back(i);
                }

                auto meshLeafAt = [&](size_t srcIndex) {
                    const NamedShape& namedShape = namedShapes[srcIndex];
                    MeshGenerator mesher;
                    mesher.SetSettings(m_meshSettings);
                    TopoDS_Shape meshShape;
                    bool ok = mesher.Generate(namedShape.shape, meshShape);
                    if (!ok) {
                        ok = mesher.RepairAndMesh(namedShape.shape, meshShape);
                    }
                    if (ok) {
                        NamedShape ns = namedShape;
                        ns.shape = meshShape;
                        ns.isAssemblyGroup = false;
                        stagedPrims[srcIndex] = std::move(ns);
                        keep[srcIndex] = 1;
                    }
                };

                if (!leafIndices.empty()) {
                    unsigned int hw = std::thread::hardware_concurrency();
                    if (hw == 0) {
                        hw = 4;
                    }
                    const unsigned int workerCount =
                        std::min<unsigned int>(static_cast<unsigned int>(leafIndices.size()),
                                               std::max(1u, std::min(hw, 8u)));
                    if (workerCount <= 1) {
                        for (size_t srcIndex : leafIndices) {
                            meshLeafAt(srcIndex);
                        }
                    } else {
                        std::atomic<size_t> nextTask{0};
                        std::vector<std::future<void>> workers;
                        workers.reserve(workerCount);
                        for (unsigned int w = 0; w < workerCount; ++w) {
                            workers.emplace_back(std::async(std::launch::async, [&]() {
                                for (;;) {
                                    const size_t task = nextTask.fetch_add(1);
                                    if (task >= leafIndices.size()) {
                                        break;
                                    }
                                    meshLeafAt(leafIndices[task]);
                                }
                            }));
                        }
                        for (auto& f : workers) {
                            f.get();
                        }
                    }
                }

                std::vector<NamedShape> scenePrims;
                scenePrims.reserve(namedShapes.size());
                for (size_t i = 0; i < namedShapes.size(); ++i) {
                    if (keep[i]) {
                        scenePrims.push_back(std::move(stagedPrims[i]));
                    }
                }

                bool hasMesh = false;
                for (const NamedShape& p : scenePrims) {
                    if (!p.isAssemblyGroup && !p.shape.IsNull()) {
                        hasMesh = true;
                        break;
                    }
                }

                if (hasMesh && !scenePrims.empty()) {
                    logger.LogInfo("Writing USD (assembly tree + meshes): " + outputFilePath);
                    UsdWriter usdWriter;
                    if (usdWriter.WriteAssemblyWithNames(scenePrims, outputFilePath)) {
                        logger.LogInfo("USD file written successfully");
                        result.success = true;
                        return result;
                    }
                    result.errorMessage = "Failed to write USD file: " + usdWriter.GetLastError();
                    logger.LogError(result.errorMessage);
                    return result;
                }
                logger.LogWarning("All XCAF parts failed meshing; falling back to STEPControl OneShape.");
            } else {
                logger.LogWarning("ReadAssemblyWithNames failed: " + reader.GetLastError() +
                                  "; falling back to STEPControl OneShape (single USD mesh).");
            }
        }

        TopoDS_Shape shape;
        if (!reader.Read(stepFilePath, shape)) {
            result.errorMessage = "Failed to read STEP file: " + reader.GetLastError();
            std::cerr << "[ERROR] " << result.errorMessage << std::endl;
            return result;
        }
        std::cout << "[STATUS] STEP file read successfully" << std::endl;

        std::cout << "[STATUS] Repairing and meshing shape..." << std::endl;
        MeshGenerator mesher;
        mesher.SetSettings(m_meshSettings);
        
        TopoDS_Shape meshShape;
        if (!meshWithFastPath(mesher, shape, meshShape)) {
            result.errorMessage = "Failed to mesh shape: " + mesher.GetLastError();
            std::cerr << "[ERROR] " << result.errorMessage << std::endl;
            return result;
        }
        std::cout << "[STATUS] Shape meshed successfully" << std::endl;

        std::string formatStr = (format == OutputFormat::USD) ? "USD" : "STL";
        std::cout << "[STATUS] Writing " << formatStr << " file: " << outputFilePath << std::endl;

        if (!WriteOutput(meshShape, outputFilePath, format)) {
            result.errorMessage = "Failed to write " + formatStr + " file";
            std::cerr << "[ERROR] " << result.errorMessage << std::endl;
            return result;
        }
        std::cout << "[STATUS] " << formatStr << " file written successfully" << std::endl;

        result.success = true;
        return result;
    }
    catch (const std::exception& e) {
        result.errorMessage = "Unexpected exception: " + std::string(e.what());
        std::cerr << "[ERROR] " << result.errorMessage << std::endl;
        return result;
    }
    catch (...) {
        result.errorMessage = "Unknown exception during conversion";
        std::cerr << "[ERROR] " << result.errorMessage << std::endl;
        return result;
    }
}

bool Converter::WriteOutput(const TopoDS_Shape& shape, const std::string& filePath, OutputFormat format) {
    if (format == OutputFormat::USD) {
        UsdWriter usdWriter;
        return usdWriter.Write(shape, filePath);
    } else {
        StlWriter stlWriter;
        return stlWriter.WriteBinary(shape, filePath);
    }
}

bool Converter::ConvertBatch(const std::vector<std::string>& stepFiles, const std::string& outputDir,
                            std::vector<ConversionResult>& results,
                            std::function<void(size_t, size_t)> progressCallback) {
    results.clear();
    m_lastError.clear();

    try {
        if (!fs::exists(outputDir)) {
            if (!fs::create_directories(outputDir)) {
                m_lastError = "Failed to create output directory: " + outputDir;
                return false;
            }
        }

        for (size_t i = 0; i < stepFiles.size(); ++i) {
            const std::string& stepFile = stepFiles[i];
            
            if (!fs::exists(stepFile)) {
                ConversionResult result;
                result.inputFile = stepFile;
                result.success = false;
                result.errorMessage = "File not found";
                results.push_back(result);
                continue;
            }

            fs::path stepPath(stepFile);
            std::string outputFileName;
            std::string dirLower = outputDir;
            std::transform(dirLower.begin(), dirLower.end(), dirLower.begin(), ::tolower);
            
            if (dirLower.find("usd") != std::string::npos) {
                outputFileName = stepPath.stem().string() + ".usdc";
            } else {
                outputFileName = stepPath.stem().string() + ".stl";
            }
            std::string outputFilePath = (fs::path(outputDir) / outputFileName).string();

            ConversionResult result = ConvertWithResult(stepFile, outputFilePath);
            results.push_back(result);

            if (progressCallback) {
                progressCallback(i + 1, stepFiles.size());
            }
        }

        return true;
    }
    catch (const std::exception& e) {
        m_lastError = "Exception during batch conversion: " + std::string(e.what());
        return false;
    }
    catch (...) {
        m_lastError = "Unknown exception during batch conversion";
        return false;
    }
}
