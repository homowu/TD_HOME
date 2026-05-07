#pragma once

#include "MeshSettings.h"
#include <TopoDS_Shape.hxx>
#include <string>

class MeshGenerator {
public:
    MeshGenerator() = default;
    ~MeshGenerator() = default;

    void SetSettings(const MeshSettings& settings);
    const MeshSettings& GetSettings() const { return m_settings; }

    bool Generate(const TopoDS_Shape& inputShape, TopoDS_Shape& meshShape);
    bool RepairAndMesh(const TopoDS_Shape& inputShape, TopoDS_Shape& meshShape);
    const std::string& GetLastError() const { return m_lastError; }

private:
    MeshSettings m_settings;
    std::string m_lastError;

    bool FixShape(TopoDS_Shape& shape);
};