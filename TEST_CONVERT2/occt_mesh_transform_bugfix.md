# 问题名称
模型导出后位置错误（局部坐标未正确转换到全局坐标）

## 现象
1) 导出的网格模型在查看器中整体偏移。  
2) 装配体中的子部件相对位置出现错位。  
3) 同一模型在 CAD 软件与目标查看器中的位置不一致。  
4) 个别样例伴随错面、拉丝、飞点等渲染异常。  

## 根因
1) 在 B-Rep 转三角网格过程中，节点使用 `triangulation->Node(i)` 后直接写入 `points`，未应用 `TopLoc_Location` 的变换。  
2) 面与面拼接时若索引偏移管理不严谨，会导致三角形引用错误顶点。  
3) 输出数据中若存在越界索引或 NaN/Inf 顶点，会放大位置与渲染异常问题。  

## 修复
1) **核心修复（位置）**  
   - 从 `BRep_Tool::Triangulation(face, loc)` 获取 `loc`。  
   - 提取 `const gp_Trsf& trsf = loc.Transformation();`。  
   - 在写入顶点前，对每个节点执行：`pnt.Transform(trsf)`（仅 `loc` 非 Identity 时）。  

2) **索引修复（连接）**  
   - 统一维护 `vertexOffset`，每个面的三角形索引写入时加偏移。  
   - 保证跨面索引始终指向正确顶点范围。  

3) **数据清洗（稳定性）**  
   - 增加 `SanitizeMeshBuffers`：过滤越界索引、过滤 NaN/Inf 顶点。  
   - 剔除非法面，防止脏数据进入下游渲染链路。  

## 最终代码
修复文件：`src/MeshExtractor.cpp`

关键代码片段如下：

```cpp
TopLoc_Location loc;
const Handle(Poly_Triangulation)& triangulation = BRep_Tool::Triangulation(face, loc);
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
```

```cpp
faceVertexIndices.push_back(v1 - 1 + vertexOffset);
faceVertexIndices.push_back(v2 - 1 + vertexOffset);
faceVertexIndices.push_back(v3 - 1 + vertexOffset);
vertexOffset += nbNodes;
```

```cpp
SanitizeMeshBuffers(points, faceVertexCounts, faceVertexIndices);
```

## 注意事项
1) 位置错误的首要排查点应是“坐标系转换是否完整”，尤其是 `TopLoc_Location`。  
2) 仅修复坐标不够，需同时检查索引偏移，否则会出现几何连接错误。  
3) 数据导出前建议统一做合法性校验（索引范围、finite 检查）。  
4) 回归验证时应覆盖：单体模型、装配体模型、含嵌套实例模型。  
5) 若后续支持法线/UV 导出，也要保证其坐标变换策略与顶点一致。  

## Tags
mesh-export, OpenCascade, TopLoc_Location, coordinate-transform, vertexOffset, sanitize, bugfix, CAD
