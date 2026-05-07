#pragma once

#include <TopoDS_Shape.hxx>
#include <string>

class StlWriter {
public:
    StlWriter() = default;
    ~StlWriter() = default;

    bool Write(const TopoDS_Shape& shape, const std::string& filePath, bool binary = true);
    bool WriteAscii(const TopoDS_Shape& shape, const std::string& filePath);
    bool WriteBinary(const TopoDS_Shape& shape, const std::string& filePath);
    const std::string& GetLastError() const { return m_lastError; }

private:
    std::string m_lastError;
};