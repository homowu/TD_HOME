#include "UsdWriterCore.h"
#include <iostream>
#include <unordered_map>
#include <functional>
#include <pxr/base/tf/stringUtils.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/xform.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>
#include <pxr/usd/usdGeom/tokens.h>
#include <pxr/usd/usdShade/material.h>
#include <pxr/usd/usdShade/shader.h>
#include <pxr/usd/usdShade/tokens.h>
#include <pxr/usd/usdShade/materialBindingAPI.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/vt/array.h>
#include <algorithm>

static pxr::VtArray<pxr::GfVec3f> BuildUsdPoints(const std::vector<float>& points) {
    const size_t n = points.size() / 3;
    pxr::VtArray<pxr::GfVec3f> out(static_cast<size_t>(n));
    for (size_t i = 0; i < n; ++i) {
        const size_t b = i * 3;
        out[i] = pxr::GfVec3f(points[b], points[b + 1], points[b + 2]);
    }
    return out;
}

static pxr::VtArray<int> BuildUsdIntArray(const std::vector<int>& src) {
    pxr::VtArray<int> out(static_cast<size_t>(src.size()));
    for (size_t i = 0; i < src.size(); ++i) {
        out[i] = src[i];
    }
    return out;
}

static std::string MakeUsdModelPath(const std::string& logicalPath) {
    if (logicalPath.empty()) {
        return "/Model";
    }
    std::string result;
    std::string segment;
    for (size_t i = 0; i <= logicalPath.size(); ++i) {
        char c = (i < logicalPath.size()) ? logicalPath[i] : '/';
        if (c == '/' || i == logicalPath.size()) {
            if (!segment.empty()) {
                std::string id = pxr::TfMakeValidIdentifier(segment);
                if (id.empty()) {
                    id = "geom";
                }
                result += "/" + id;
                segment.clear();
            }
        } else {
            segment += c;
        }
    }
    return result.empty() ? "/Model" : result;
}

static void EnsureParentNodes(pxr::UsdStageRefPtr stage, const pxr::SdfPath& path) {
    pxr::SdfPath parentPath = path.GetParentPath();
    
    while (!parentPath.IsEmpty()) {
        if (!stage->GetPrimAtPath(parentPath)) {
            pxr::UsdGeomXform::Define(stage, parentPath);
        }
        parentPath = parentPath.GetParentPath();
    }
}

static pxr::SdfPath EnsureUsdPreviewMaterial(pxr::UsdStageRefPtr stage,
                                             std::unordered_map<std::string, pxr::SdfPath>& cache,
                                             const std::string& key,
                                             const MeshPart& part) {
    const auto inserted = cache.find(key);
    if (inserted != cache.end()) {
        return inserted->second;
    }

    std::string id = pxr::TfMakeValidIdentifier(key);
    if (id.empty()) {
        id = "material";
    }
    if (id.size() > 100) {
        id = "m_" + std::to_string(std::hash<std::string>{}(key));
        id = pxr::TfMakeValidIdentifier(id);
    }

    pxr::SdfPath matPath = pxr::SdfPath("/Materials").AppendChild(pxr::TfToken(id));
    EnsureParentNodes(stage, matPath);

    pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(stage, matPath);
    pxr::SdfPath shaderPath = matPath.AppendChild(pxr::TfToken("PreviewSurface"));
    pxr::UsdShadeShader shader = pxr::UsdShadeShader::Define(stage, shaderPath);
    shader.CreateIdAttr(pxr::VtValue(std::string("UsdPreviewSurface")));

    shader.CreateInput(pxr::TfToken("diffuseColor"), pxr::SdfValueTypeNames->Color3f)
        .Set(pxr::GfVec3f(part.diffuseColor[0], part.diffuseColor[1], part.diffuseColor[2]));
    shader.CreateInput(pxr::TfToken("metallic"), pxr::SdfValueTypeNames->Float).Set(part.metallic);
    shader.CreateInput(pxr::TfToken("roughness"), pxr::SdfValueTypeNames->Float).Set(part.roughness);
    shader.CreateInput(pxr::TfToken("opacity"), pxr::SdfValueTypeNames->Float).Set(part.opacity);
    shader.CreateInput(pxr::TfToken("emissiveColor"), pxr::SdfValueTypeNames->Color3f)
        .Set(pxr::GfVec3f(part.emissiveColor[0], part.emissiveColor[1], part.emissiveColor[2]));

    pxr::UsdShadeOutput shaderOut =
        shader.CreateOutput(pxr::UsdShadeTokens->surface, pxr::SdfValueTypeNames->Token);
    pxr::UsdShadeOutput matSurface = material.CreateSurfaceOutput();
    matSurface.ConnectToSource(shaderOut);

    cache[key] = matPath;
    return matPath;
}

bool WriteUsdMesh(const std::vector<float>& points,
                  const std::vector<int>& faceVertexCounts,
                  const std::vector<int>& faceVertexIndices,
                  const std::string& filePath,
                  std::string& errorMessage) {
    try {
        if (points.empty()) {
            errorMessage = "No points data provided";
            return false;
        }

        pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateNew(filePath);

        if (!stage) {
            errorMessage = "Failed to create USD stage";
            return false;
        }

        pxr::UsdGeomMesh mesh = pxr::UsdGeomMesh::Define(stage, pxr::SdfPath("/Mesh"));
        pxr::VtArray<pxr::GfVec3f> usdPoints = BuildUsdPoints(points);
        pxr::VtArray<int> usdFaceVertexCounts = BuildUsdIntArray(faceVertexCounts);
        pxr::VtArray<int> usdFaceVertexIndices = BuildUsdIntArray(faceVertexIndices);

        mesh.CreatePointsAttr().Set(usdPoints);
        mesh.CreateFaceVertexCountsAttr().Set(usdFaceVertexCounts);
        mesh.CreateFaceVertexIndicesAttr().Set(usdFaceVertexIndices);

        stage->Save();

        return true;
    }
    catch (const std::exception& e) {
        errorMessage = "Exception during USD writing: " + std::string(e.what());
        std::cerr << "[ERROR] " << errorMessage << std::endl;
        return false;
    }
    catch (...) {
        errorMessage = "Unknown exception during USD writing";
        std::cerr << "[ERROR] " << errorMessage << std::endl;
        return false;
    }
}

bool WriteUsdMeshHierarchical(const std::vector<MeshPart>& parts,
                              const std::string& filePath,
                              std::string& errorMessage) {
    try {
        if (parts.empty()) {
            errorMessage = "No parts data provided";
            return false;
        }

        pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateNew(filePath);

        if (!stage) {
            errorMessage = "Failed to create USD stage";
            return false;
        }

        for (size_t i = 0; i < parts.size(); ++i) {
            const MeshPart& part = parts[i];
            
            if (part.points.empty()) {
                continue;
            }

            std::string cleanPath = part.path;
            if (cleanPath.empty()) {
                cleanPath = "/Part_" + std::to_string(i);
            }
            
            if (cleanPath[0] != '/') {
                cleanPath = "/" + cleanPath;
            }

            std::string usdModelPath = MakeUsdModelPath(cleanPath);
            pxr::SdfPath meshPath(usdModelPath + "/mesh");
            pxr::SdfPath xformPath(usdModelPath);

            EnsureParentNodes(stage, xformPath);

            pxr::UsdGeomXform xform = pxr::UsdGeomXform::Define(stage, xformPath);

            if (part.hasTransform) {
                pxr::GfMatrix4d transformMatrix(
                    part.transform[0], part.transform[1], part.transform[2], part.transform[3],
                    part.transform[4], part.transform[5], part.transform[6], part.transform[7],
                    part.transform[8], part.transform[9], part.transform[10], part.transform[11],
                    part.transform[12], part.transform[13], part.transform[14], part.transform[15]
                );
                pxr::UsdGeomXformOp xformOp = xform.AddTransformOp();
                xformOp.Set(transformMatrix);
            }
            
            pxr::UsdGeomMesh mesh = pxr::UsdGeomMesh::Define(stage, meshPath);
            pxr::VtArray<pxr::GfVec3f> usdPoints = BuildUsdPoints(part.points);
            pxr::VtArray<int> usdFaceVertexCounts = BuildUsdIntArray(part.faceVertexCounts);
            pxr::VtArray<int> usdFaceVertexIndices = BuildUsdIntArray(part.faceVertexIndices);

            mesh.CreatePointsAttr().Set(usdPoints);
            mesh.CreateFaceVertexCountsAttr().Set(usdFaceVertexCounts);
            mesh.CreateFaceVertexIndicesAttr().Set(usdFaceVertexIndices);
        }

        stage->Save();

        return true;
    }
    catch (const std::exception& e) {
        errorMessage = "Exception during USD hierarchical writing: " + std::string(e.what());
        std::cerr << "[ERROR] " << errorMessage << std::endl;
        return false;
    }
    catch (...) {
        errorMessage = "Unknown exception during USD hierarchical writing";
        std::cerr << "[ERROR] " << errorMessage << std::endl;
        return false;
    }
}

bool WriteUsdAssemblyScene(const std::vector<std::string>& assemblyGroupLogicalPaths,
                             const std::vector<MeshPart>& meshParts,
                             const std::string& filePath,
                             std::string& errorMessage) {
    try {
        pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateNew(filePath);
        if (!stage) {
            errorMessage = "Failed to create USD stage";
            return false;
        }

        std::vector<std::string> groupPaths = assemblyGroupLogicalPaths;
        std::sort(groupPaths.begin(), groupPaths.end(), [](const std::string& a, const std::string& b) {
            const auto depth = [](const std::string& s) {
                return static_cast<int>(std::count(s.begin(), s.end(), '/'));
            };
            return depth(a) < depth(b);
        });

        for (std::string p : groupPaths) {
            if (p.empty()) {
                continue;
            }
            if (p[0] != '/') {
                p = "/" + p;
            }
            const std::string usdModelPath = MakeUsdModelPath(p);
            pxr::SdfPath xformPath(usdModelPath);
            EnsureParentNodes(stage, xformPath);
            pxr::UsdGeomXform::Define(stage, xformPath);
        }

        if (meshParts.empty()) {
            errorMessage = "No mesh geometry in assembly scene";
            return false;
        }

        std::unordered_map<std::string, pxr::SdfPath> previewMaterialCache;

        for (size_t i = 0; i < meshParts.size(); ++i) {
            const MeshPart& part = meshParts[i];

            std::string cleanPath = part.path;
            if (cleanPath.empty()) {
                cleanPath = "/Part_" + std::to_string(i);
            }
            if (!cleanPath.empty() && cleanPath[0] != '/') {
                cleanPath = "/" + cleanPath;
            }

            const std::string usdModelPath = MakeUsdModelPath(cleanPath);
            pxr::SdfPath meshPath(usdModelPath + "/mesh");
            pxr::SdfPath xformPath(usdModelPath);

            EnsureParentNodes(stage, xformPath);

            pxr::UsdGeomXform xform = pxr::UsdGeomXform::Define(stage, xformPath);

            if (part.hasTransform) {
                pxr::GfMatrix4d transformMatrix(
                    part.transform[0], part.transform[1], part.transform[2], part.transform[3],
                    part.transform[4], part.transform[5], part.transform[6], part.transform[7],
                    part.transform[8], part.transform[9], part.transform[10], part.transform[11],
                    part.transform[12], part.transform[13], part.transform[14], part.transform[15]);
                pxr::UsdGeomXformOp xformOp = xform.AddTransformOp();
                xformOp.Set(transformMatrix);
            }

            pxr::UsdGeomMesh mesh = pxr::UsdGeomMesh::Define(stage, meshPath);

            pxr::VtArray<pxr::GfVec3f> usdPoints = BuildUsdPoints(part.points);
            pxr::VtArray<int> usdFaceVertexCounts = BuildUsdIntArray(part.faceVertexCounts);
            pxr::VtArray<int> usdFaceVertexIndices = BuildUsdIntArray(part.faceVertexIndices);

            mesh.CreatePointsAttr().Set(usdPoints);
            mesh.CreateFaceVertexCountsAttr().Set(usdFaceVertexCounts);
            mesh.CreateFaceVertexIndicesAttr().Set(usdFaceVertexIndices);

            if (part.bindUsdPreviewMaterial) {
                const std::string matKey =
                    part.previewMaterialKey.empty() ? ("unique_" + std::to_string(i)) : part.previewMaterialKey;
                const pxr::SdfPath matPath =
                    EnsureUsdPreviewMaterial(stage, previewMaterialCache, matKey, part);
                pxr::UsdShadeMaterialBindingAPI::Apply(mesh.GetPrim());
                pxr::UsdShadeMaterialBindingAPI(mesh.GetPrim())
                    .Bind(pxr::UsdShadeMaterial(stage->GetPrimAtPath(matPath)));
            } else if (part.hasDisplayColor) {
                pxr::UsdGeomPrimvarsAPI pv(mesh);
                pxr::UsdGeomPrimvar dc =
                    pv.CreatePrimvar(pxr::TfToken("displayColor"), pxr::SdfValueTypeNames->Color3fArray,
                                     pxr::UsdGeomTokens->constant, 1);
                pxr::VtArray<pxr::GfVec3f> cols;
                cols.push_back(
                    pxr::GfVec3f(part.displayColor[0], part.displayColor[1], part.displayColor[2]));
                dc.Set(cols);
            }
        }

        stage->Save();
        return true;
    }
    catch (const std::exception& e) {
        errorMessage = "Exception during USD assembly scene writing: " + std::string(e.what());
        std::cerr << "[ERROR] " << errorMessage << std::endl;
        return false;
    }
    catch (...) {
        errorMessage = "Unknown exception during USD assembly scene writing";
        std::cerr << "[ERROR] " << errorMessage << std::endl;
        return false;
    }
}