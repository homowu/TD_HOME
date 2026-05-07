#pragma once

#include <TopoDS_Shape.hxx>
#include <gp_Trsf.hxx>
#include <string>
#include <memory>
#include <vector>

struct NamedShape {
    TopoDS_Shape shape;
    std::string name;
    std::string path;
    std::string parentName;
    gp_Trsf transformation;
    //! True for XCAF assembly nodes: USD gets an empty Xform at `path` (no mesh), mirrors STEP hierarchy.
    bool isAssemblyGroup{false};

    //! From XCAF (optional): STEP often supplies RGB/RGBA via ColorTool; extended styling via VisMaterial (PBR/Common).
    //! Physical/material-table density when STEPCAF reads manufacturing materials (MatMode).
    bool hasDiffuseColor{false};
    float diffuseColor[3]{1.f, 1.f, 1.f};
    float opacity{1.f};
    bool bindUsdPreviewMaterial{false};
    float metallic{0.f};
    float roughness{0.5f};
    float emissive[3]{0.f, 0.f, 0.f};
    double physicalMaterialDensity{0.};
    std::string physicalMaterialName;
};

class StepReader {
public:
    StepReader() = default;
    ~StepReader() = default;

    bool Read(const std::string& filePath, TopoDS_Shape& shape);
    bool ReadAssembly(const std::string& filePath, std::vector<TopoDS_Shape>& shapes);
    bool ReadAssemblyWithNames(const std::string& filePath, std::vector<NamedShape>& namedShapes);
    const std::string& GetLastError() const { return m_lastError; }

private:
    std::string m_lastError;
};