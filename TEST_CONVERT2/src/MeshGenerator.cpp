#include "MeshGenerator.h"
#include "Logger.h"
#include <BRepMesh_IncrementalMesh.hxx>
#include <ShapeFix_ShapeTolerance.hxx>
#include <ShapeFix_Shape.hxx>
#include <TopoDS_Compound.hxx>
#include <BRep_Builder.hxx>
#include <TopExp_Explorer.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <Message_ProgressIndicator.hxx>

void MeshGenerator::SetSettings(const MeshSettings& settings) {
    m_settings = settings;
}

bool MeshGenerator::FixShape(TopoDS_Shape& shape) {
    Logger& logger = Logger::GetInstance();
    try {
        logger.StartTimer("Shape fixing");
        
        ShapeFix_Shape fixer;
        fixer.Init(shape);
        fixer.Perform();
        shape = fixer.Shape();
        
        if (shape.IsNull()) {
            m_lastError = "Shape became null after fixing";
            logger.LogError(m_lastError);
            logger.EndTimer("Shape fixing");
            return false;
        }

        logger.EndTimer("Shape fixing");
        return true;
    }
    catch (const Standard_Failure& e) {
        m_lastError = "OCCT exception during shape fixing: " + std::string(e.GetMessageString());
        logger.LogError(m_lastError);
        return false;
    }
    catch (const std::exception& e) {
        m_lastError = "Exception during shape fixing: " + std::string(e.what());
        logger.LogError(m_lastError);
        return false;
    }
}

bool MeshGenerator::Generate(const TopoDS_Shape& inputShape, TopoDS_Shape& meshShape) {
    m_lastError.clear();
    Logger& logger = Logger::GetInstance();

    if (inputShape.IsNull()) {
        m_lastError = "Input shape is null";
        logger.LogError(m_lastError);
        return false;
    }

    try {
        logger.StartTimer("Mesh generation");
        
        TopoDS_Shape workingShape = inputShape;

        BRepMesh_IncrementalMesh mesher(workingShape, m_settings.deflection, m_settings.relative, m_settings.angle, m_settings.adaptive);
        mesher.Perform();

        if (!mesher.IsDone()) {
            m_lastError = "Mesh generation failed";
            logger.LogError(m_lastError);
            logger.EndTimer("Mesh generation");
            return false;
        }

        meshShape = workingShape;
        logger.EndTimer("Mesh generation");
        return true;
    }
    catch (const Standard_Failure& e) {
        m_lastError = "OCCT exception during mesh generation: " + std::string(e.GetMessageString());
        logger.LogError(m_lastError);
        return false;
    }
    catch (const std::bad_alloc& e) {
        m_lastError = "Memory allocation failed during mesh generation: " + std::string(e.what());
        logger.LogError(m_lastError);
        return false;
    }
    catch (const std::exception& e) {
        m_lastError = "Exception during mesh generation: " + std::string(e.what());
        logger.LogError(m_lastError);
        return false;
    }
    catch (...) {
        m_lastError = "Unknown exception during mesh generation";
        logger.LogError(m_lastError);
        return false;
    }
}

bool MeshGenerator::RepairAndMesh(const TopoDS_Shape& inputShape, TopoDS_Shape& meshShape) {
    m_lastError.clear();
    Logger& logger = Logger::GetInstance();

    if (inputShape.IsNull()) {
        m_lastError = "Input shape is null";
        logger.LogError(m_lastError);
        return false;
    }

    try {
        TopoDS_Shape workingShape = inputShape;

        bool fixSuccess = FixShape(workingShape);
        if (!fixSuccess) {
            logger.LogWarning("Shape fixing failed, attempting mesh without repair");
            workingShape = inputShape;
        }
        
        return Generate(workingShape, meshShape);
    }
    catch (const Standard_Failure& e) {
        m_lastError = "OCCT exception during repair and mesh: " + std::string(e.GetMessageString());
        logger.LogError(m_lastError);
        return false;
    }
    catch (const std::bad_alloc& e) {
        m_lastError = "Memory allocation failed during repair and mesh: " + std::string(e.what());
        logger.LogError(m_lastError);
        return false;
    }
    catch (const std::exception& e) {
        m_lastError = "Standard exception during repair and mesh: " + std::string(e.what());
        logger.LogError(m_lastError);
        return false;
    }
    catch (...) {
        m_lastError = "Unknown exception during repair and mesh";
        logger.LogError(m_lastError);
        return false;
    }
}