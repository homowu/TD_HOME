#include "UsdWriter.h"
#include "UsdWriterCore.h"
#include "MeshExtractor.h"
#include "StepReader.h"
#include "ErrorHandler.h"
#include "Logger.h"
#include <TopoDS_Shape.hxx>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <vector>
#include <string>

static int g_partCounter = 0;

//! Uses ExtractMeshData() from MeshExtractor.cpp — identical sampling path as STL (BRep_Tool::Triangulation + face Location chain).
static bool FillMeshPartFromShape(const TopoDS_Shape& shape, const std::string& path, MeshPart& part) {
    part.path = path;
    part.hasTransform = false;
    return ExtractMeshData(shape, part.points, part.faceVertexCounts, part.faceVertexIndices);
}

static void CopyXcafAppearanceToMeshPart(const NamedShape& ns, MeshPart& part) {
    part.displayColor[0] = ns.diffuseColor[0];
    part.displayColor[1] = ns.diffuseColor[1];
    part.displayColor[2] = ns.diffuseColor[2];
    part.diffuseColor[0] = ns.diffuseColor[0];
    part.diffuseColor[1] = ns.diffuseColor[1];
    part.diffuseColor[2] = ns.diffuseColor[2];
    part.opacity = ns.opacity;
    part.metallic = ns.metallic;
    part.roughness = ns.roughness;
    part.emissiveColor[0] = ns.emissive[0];
    part.emissiveColor[1] = ns.emissive[1];
    part.emissiveColor[2] = ns.emissive[2];
    part.hasDisplayColor = ns.hasDiffuseColor;
    part.bindUsdPreviewMaterial = ns.bindUsdPreviewMaterial;
    if (ns.bindUsdPreviewMaterial || ns.hasDiffuseColor) {
        std::ostringstream k;
        k << std::fixed << std::setprecision(4) << "d" << ns.diffuseColor[0] << "_" << ns.diffuseColor[1] << "_"
          << ns.diffuseColor[2] << "_a" << ns.opacity << "_m" << ns.metallic << "_r" << ns.roughness << "_e"
          << ns.emissive[0] << "_" << ns.emissive[1] << "_" << ns.emissive[2];
        if (ns.physicalMaterialDensity > 1e-12) {
            k << "_rho" << ns.physicalMaterialDensity;
        }
        part.previewMaterialKey = k.str();
    }
}

static std::string GetNextPartName() {
    std::ostringstream oss;
    oss << "Part_" << g_partCounter++;
    return oss.str();
}

bool UsdWriter::Write(const TopoDS_Shape& shape, const std::string& filePath) {
    m_lastError.clear();
    Logger& logger = Logger::GetInstance();

    if (shape.IsNull()) {
        m_lastError = "Input shape is null";
        logger.LogError(m_lastError);
        return false;
    }

    if (!ErrorHandler::CheckFilePathValid(filePath)) {
        m_lastError = ErrorHandler::GetErrorMessage(ErrorCode::InvalidArgument) + ": Invalid file path";
        logger.LogError(m_lastError);
        return false;
    }

    std::filesystem::path outputPath(filePath);
    std::filesystem::path outputDir = outputPath.parent_path();

    if (!outputDir.empty() && !std::filesystem::exists(outputDir)) {
        logger.LogInfo("Creating output directory: " + outputDir.string());
        std::filesystem::create_directories(outputDir);
    }

    logger.StartTimer("USD writing");
    logger.LogInfo("Writing USD file (single mesh, same ExtractMeshData as STL): " + filePath);

    std::vector<float> points;
    std::vector<int> faceVertexCounts;
    std::vector<int> faceVertexIndices;

    if (!ExtractMeshData(shape, points, faceVertexCounts, faceVertexIndices)) {
        m_lastError = "Failed to extract mesh data from shape";
        logger.LogError(m_lastError);
        logger.EndTimer("USD writing");
        return false;
    }

    if (points.empty()) {
        m_lastError = "No triangulation data found in shape";
        logger.LogError(m_lastError);
        logger.EndTimer("USD writing");
        return false;
    }

    logger.LogInfo("Mesh data extracted: " + std::to_string(points.size() / 3) + " vertices, " +
                   std::to_string(faceVertexCounts.size()) + " faces");
    bool success = WriteUsdMesh(points, faceVertexCounts, faceVertexIndices, filePath, m_lastError);

    if (!success) {
        logger.LogError("USD writing failed: " + m_lastError);
    } else {
        logger.LogInfo("USD file written successfully: " + filePath);
    }

    logger.EndTimer("USD writing");
    return success;
}

bool UsdWriter::WriteAssembly(const std::vector<TopoDS_Shape>& shapes, const std::string& filePath) {
    m_lastError.clear();
    Logger& logger = Logger::GetInstance();

    if (shapes.empty()) {
        m_lastError = "No shapes provided for assembly";
        logger.LogError(m_lastError);
        return false;
    }

    if (!ErrorHandler::CheckFilePathValid(filePath)) {
        m_lastError = ErrorHandler::GetErrorMessage(ErrorCode::InvalidArgument) + ": Invalid file path";
        logger.LogError(m_lastError);
        return false;
    }

    std::filesystem::path outputPath(filePath);
    std::filesystem::path outputDir = outputPath.parent_path();

    if (!outputDir.empty() && !std::filesystem::exists(outputDir)) {
        logger.LogInfo("Creating output directory: " + outputDir.string());
        std::filesystem::create_directories(outputDir);
    }

    logger.StartTimer("USD writing");
    logger.LogInfo("Writing USD assembly file (ExtractMeshData per root shape): " + filePath);

    std::vector<MeshPart> parts;
    g_partCounter = 0;

    for (const TopoDS_Shape& shape : shapes) {
        if (shape.IsNull()) {
            continue;
        }
        MeshPart part;
        const std::string ppath = "/" + GetNextPartName();
        if (FillMeshPartFromShape(shape, ppath, part) && !part.points.empty()) {
            parts.push_back(std::move(part));
        }
    }

    if (parts.empty()) {
        m_lastError = "No valid mesh data found in assembly";
        logger.LogError(m_lastError);
        logger.EndTimer("USD writing");
        return false;
    }

    int totalVertices = 0;
    int totalFaces = 0;
    for (const MeshPart& part : parts) {
        totalVertices += static_cast<int>(part.points.size() / 3);
        totalFaces += static_cast<int>(part.faceVertexCounts.size());
    }
    logger.LogInfo("Assembly mesh data extracted: " + std::to_string(parts.size()) + " parts, " +
                   std::to_string(totalVertices) + " vertices, " + std::to_string(totalFaces) + " faces");

    bool success = WriteUsdMeshHierarchical(parts, filePath, m_lastError);

    if (!success) {
        logger.LogError("USD writing failed: " + m_lastError);
    } else {
        logger.LogInfo("USD assembly file written successfully: " + filePath);
    }

    logger.EndTimer("USD writing");
    return success;
}

bool UsdWriter::WriteAssemblyWithNames(const std::vector<NamedShape>& namedShapes, const std::string& filePath) {
    m_lastError.clear();
    Logger& logger = Logger::GetInstance();

    if (namedShapes.empty()) {
        m_lastError = "No shapes provided for assembly";
        logger.LogError(m_lastError);
        return false;
    }

    if (!ErrorHandler::CheckFilePathValid(filePath)) {
        m_lastError = ErrorHandler::GetErrorMessage(ErrorCode::InvalidArgument) + ": Invalid file path";
        logger.LogError(m_lastError);
        return false;
    }

    std::filesystem::path outputPath(filePath);
    std::filesystem::path outputDir = outputPath.parent_path();

    if (!outputDir.empty() && !std::filesystem::exists(outputDir)) {
        logger.LogInfo("Creating output directory: " + outputDir.string());
        std::filesystem::create_directories(outputDir);
    }

    logger.StartTimer("USD writing");
    logger.LogInfo("Writing USD assembly scene (XCAF tree + meshes): " + filePath);

    std::vector<std::string> assemblyGroups;
    std::vector<MeshPart> meshLeaves;
    assemblyGroups.reserve(namedShapes.size());
    meshLeaves.reserve(namedShapes.size());
    for (const NamedShape& ns : namedShapes) {
        if (ns.isAssemblyGroup) {
            std::string p = ns.path;
            if (!p.empty() && p[0] != '/') {
                p = "/" + p;
            }
            if (!p.empty()) {
                assemblyGroups.push_back(p);
            }
            continue;
        }
        if (ns.shape.IsNull()) {
            continue;
        }
        MeshPart part;
        if (!FillMeshPartFromShape(ns.shape, ns.path, part) || part.points.empty()) {
            continue;
        }
        CopyXcafAppearanceToMeshPart(ns, part);
        meshLeaves.push_back(std::move(part));
    }

    const bool success = WriteUsdAssemblyScene(assemblyGroups, meshLeaves, filePath, m_lastError);

    if (!success) {
        logger.LogError("USD writing failed: " + m_lastError);
    } else {
        logger.LogInfo("USD file written successfully: " + filePath);
    }

    logger.EndTimer("USD writing");
    return success;
}
