#include "MeshExtractor.h"
#include <TopoDS_Shape.hxx>
#include <TopExp_Explorer.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS.hxx>
#include <BRep_Tool.hxx>
#include <Poly_Triangulation.hxx>
#include <TopLoc_Location.hxx>
#include <gp_Pnt.hxx>
#include <Poly_Triangle.hxx>
#include <gp_Trsf.hxx>
#include <cmath>

static void SanitizeMeshBuffers(std::vector<float>& points, std::vector<int>& faceVertexCounts,
                                std::vector<int>& faceVertexIndices) {
    const size_t nverts = points.size() / 3;
    std::vector<int> newCounts;
    std::vector<int> newIdx;
    newCounts.reserve(faceVertexCounts.size());
    newIdx.reserve(faceVertexIndices.size());
    size_t walk = 0;
    for (size_t fi = 0; fi < faceVertexCounts.size(); ++fi) {
        const int cnt = faceVertexCounts[fi];
        bool ok = cnt > 0;
        if (ok) {
            for (int k = 0; k < cnt; ++k) {
                const int vi = faceVertexIndices[walk + k];
                if (vi < 0 || static_cast<size_t>(vi) >= nverts) {
                    ok = false;
                    break;
                }
                const float x = points[static_cast<size_t>(vi) * 3];
                const float y = points[static_cast<size_t>(vi) * 3 + 1];
                const float z = points[static_cast<size_t>(vi) * 3 + 2];
                if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) {
                    ok = false;
                    break;
                }
            }
        }
        if (ok) {
            newCounts.push_back(cnt);
            for (int k = 0; k < cnt; ++k) {
                newIdx.push_back(faceVertexIndices[walk + k]);
            }
        }
        walk += static_cast<size_t>(cnt);
    }
    faceVertexCounts = std::move(newCounts);
    faceVertexIndices = std::move(newIdx);
}

bool ExtractMeshData(const TopoDS_Shape& shape,
                     std::vector<float>& points,
                     std::vector<int>& faceVertexCounts,
                     std::vector<int>& faceVertexIndices) {
    points.clear();
    faceVertexCounts.clear();
    faceVertexIndices.clear();

    if (shape.IsNull()) {
        return false;
    }

    TopExp_Explorer faceExplorer(shape, TopAbs_FACE);
    int vertexOffset = 0;

    while (faceExplorer.More()) {
        const TopoDS_Face face = TopoDS::Face(faceExplorer.Current());
        TopLoc_Location loc;
        const Handle(Poly_Triangulation)& triangulation = BRep_Tool::Triangulation(face, loc);

        if (!triangulation.IsNull()) {
            Standard_Integer nbNodes = triangulation->NbNodes();
            Standard_Integer nbTriangles = triangulation->NbTriangles();

            const gp_Trsf& trsf = loc.Transformation();

            for (Standard_Integer i = 1; i <= nbNodes; ++i) {
                gp_Pnt pnt = triangulation->Node(i);
                if (!loc.IsIdentity()) {
                    pnt.Transform(trsf);
                }
                points.push_back((float)pnt.X());
                points.push_back((float)pnt.Y());
                points.push_back((float)pnt.Z());
            }

            for (Standard_Integer i = 1; i <= nbTriangles; ++i) {
                const Poly_Triangle& tri = triangulation->Triangle(i);
                Standard_Integer v1, v2, v3;
                tri.Get(v1, v2, v3);

                faceVertexCounts.push_back(3);
                faceVertexIndices.push_back(v1 - 1 + vertexOffset);
                faceVertexIndices.push_back(v2 - 1 + vertexOffset);
                faceVertexIndices.push_back(v3 - 1 + vertexOffset);
            }

            vertexOffset += nbNodes;
        }

        faceExplorer.Next();
    }

    SanitizeMeshBuffers(points, faceVertexCounts, faceVertexIndices);
    return !points.empty() && !faceVertexIndices.empty();
}
