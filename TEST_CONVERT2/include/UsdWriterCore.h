#pragma once

#include <vector>
#include <string>

struct MeshPart {
    std::vector<float> points;
    std::vector<int> faceVertexCounts;
    std::vector<int> faceVertexIndices;
    std::string path;
    double transform[16]{};
    bool hasTransform{false};

    //! From XCAF appearance (optional). When bindUsdPreviewMaterial is true, emit UsdPreviewSurface + Material binding.
    //! Otherwise hasDisplayColor sets mesh constant displayColor (no fake UV / textures).
    bool hasDisplayColor{false};
    float displayColor[3]{1.f, 1.f, 1.f};
    float opacity{1.f};
    bool bindUsdPreviewMaterial{false};
    float diffuseColor[3]{1.f, 1.f, 1.f};
    float metallic{0.f};
    float roughness{0.5f};
    float emissiveColor[3]{0.f, 0.f, 0.f};
    std::string previewMaterialKey;
};

bool WriteUsdMesh(const std::vector<float>& points, 
                  const std::vector<int>& faceVertexCounts, 
                  const std::vector<int>& faceVertexIndices,
                  const std::string& filePath,
                  std::string& errorMessage);

bool WriteUsdMeshHierarchical(const std::vector<MeshPart>& parts,
                              const std::string& filePath,
                              std::string& errorMessage);

//! STEP/XCAF assembly: empty Xform groups (logical paths) + mesh parts (same paths as source leaves).
bool WriteUsdAssemblyScene(const std::vector<std::string>& assemblyGroupLogicalPaths,
                           const std::vector<MeshPart>& meshParts,
                           const std::string& filePath,
                           std::string& errorMessage);