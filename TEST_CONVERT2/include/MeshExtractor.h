#pragma once

#include <vector>

class TopoDS_Shape;

bool ExtractMeshData(const TopoDS_Shape& shape, 
                     std::vector<float>& points, 
                     std::vector<int>& faceVertexCounts, 
                     std::vector<int>& faceVertexIndices);
