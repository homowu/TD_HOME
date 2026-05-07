#include <iostream>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/xform.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/vt/array.h>

int main() {
    try {
        std::cout << "Creating USD stage..." << std::endl;
        
        pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateNew("test_basic.usdc");
        
        if (!stage) {
            std::cerr << "Failed to create USD stage" << std::endl;
            return 1;
        }
        
        std::cout << "Creating Xform..." << std::endl;
        pxr::UsdGeomXform xform = pxr::UsdGeomXform::Define(stage, pxr::SdfPath("/TestMesh"));
        
        std::cout << "Creating Mesh..." << std::endl;
        pxr::UsdGeomMesh mesh = pxr::UsdGeomMesh::Define(stage, pxr::SdfPath("/TestMesh/mesh"));
        
        pxr::VtArray<pxr::GfVec3f> points;
        points.push_back(pxr::GfVec3f(0, 0, 0));
        points.push_back(pxr::GfVec3f(1, 0, 0));
        points.push_back(pxr::GfVec3f(0, 1, 0));
        
        pxr::VtArray<int> faceVertexCounts = {3};
        pxr::VtArray<int> faceVertexIndices = {0, 1, 2};
        
        mesh.CreatePointsAttr().Set(points);
        mesh.CreateFaceVertexCountsAttr().Set(faceVertexCounts);
        mesh.CreateFaceVertexIndicesAttr().Set(faceVertexIndices);
        
        std::cout << "Saving USD stage..." << std::endl;
        stage->Save();
        
        std::cout << "USD file created successfully!" << std::endl;
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
    catch (...) {
        std::cerr << "Unknown exception" << std::endl;
        return 1;
    }
}
