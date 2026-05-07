#include "StlWriter.h"
#include "ErrorHandler.h"
#include "Logger.h"
#include <RWStl.hxx>
#include <StlAPI_Writer.hxx>
#include <TopoDS_Shape.hxx>
#include <TopExp_Explorer.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <Message_ProgressIndicator.hxx>

bool StlWriter::Write(const TopoDS_Shape& shape, const std::string& filePath, bool binary) {
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

    if (binary) {
        return WriteBinary(shape, filePath);
    } else {
        return WriteAscii(shape, filePath);
    }
}

bool StlWriter::WriteAscii(const TopoDS_Shape& shape, const std::string& filePath) {
    m_lastError.clear();
    Logger& logger = Logger::GetInstance();

    try {
        logger.StartTimer("STL ASCII writing");
        
        StlAPI_Writer writer;
        writer.ASCIIMode() = true;
        writer.Write(shape, filePath.c_str());
        
        logger.EndTimer("STL ASCII writing");
        return true;
    }
    catch (const std::exception& e) {
        m_lastError = "Exception during ASCII STL writing: " + std::string(e.what());
        logger.LogError(m_lastError);
        return false;
    }
    catch (...) {
        m_lastError = "Unknown exception during ASCII STL writing";
        logger.LogError(m_lastError);
        return false;
    }
}

bool StlWriter::WriteBinary(const TopoDS_Shape& shape, const std::string& filePath) {
    m_lastError.clear();
    Logger& logger = Logger::GetInstance();

    try {
        logger.StartTimer("STL binary writing");
        
        StlAPI_Writer writer;
        writer.ASCIIMode() = false;
        writer.Write(shape, filePath.c_str());
        
        logger.EndTimer("STL binary writing");
        return true;
    }
    catch (const std::exception& e) {
        m_lastError = "Exception during binary STL writing: " + std::string(e.what());
        logger.LogError(m_lastError);
        return false;
    }
    catch (...) {
        m_lastError = "Unknown exception during binary STL writing";
        logger.LogError(m_lastError);
        return false;
    }
}