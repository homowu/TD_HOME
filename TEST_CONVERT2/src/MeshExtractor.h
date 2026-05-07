#ifndef MESH_EXTRACTOR_H
#define MESH_EXTRACTOR_H

#include <TopoDS_Shape.hxx>
#include <string>
#include <vector>

bool ExtractMeshData(const TopoDS_Shape& shape,
                     std::vector<float>& points,
                     std::vector<int>& faceVertexCounts,
                     std::vector<int>& faceVertexIndices);

#endif
