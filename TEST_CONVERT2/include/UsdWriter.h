#pragma once

#include <TopoDS_Shape.hxx>
#include <string>
#include <vector>
#include "StepReader.h"

class UsdWriter {
public:
    UsdWriter() = default;
    ~UsdWriter() = default;

    bool Write(const TopoDS_Shape& shape, const std::string& filePath);
    bool WriteAssembly(const std::vector<TopoDS_Shape>& shapes, const std::string& filePath);
    bool WriteAssemblyWithNames(const std::vector<NamedShape>& namedShapes, const std::string& filePath);
    const std::string& GetLastError() const { return m_lastError; }

private:
    std::string m_lastError;
};
