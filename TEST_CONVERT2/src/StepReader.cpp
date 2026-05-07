#include "StepReader.h"
#include "ErrorHandler.h"
#include "Logger.h"
#include <STEPControl_Reader.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <TopExp_Explorer.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Solid.hxx>
#include <TopoDS_CompSolid.hxx>
#include <TCollection_AsciiString.hxx>
#include <TCollection_ExtendedString.hxx>
#include <TDF_Label.hxx>
#include <TDF_ChildIterator.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <XCAFDoc_ColorTool.hxx>
#include <XCAFDoc_ColorType.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_MaterialTool.hxx>
#include <XCAFDoc_VisMaterial.hxx>
#include <XCAFDoc_VisMaterialTool.hxx>
#include <TDataStd_Name.hxx>
#include <TDocStd_Document.hxx>
#include <STEPCAFControl_Reader.hxx>
#include <TopLoc_Location.hxx>
#include <gp_Trsf.hxx>
#include <Quantity_Color.hxx>
#include <Quantity_ColorRGBA.hxx>
#include <BRep_Builder.hxx>
#include <filesystem>
#include <map>
#include <set>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <cctype>
#include <TDF_LabelSequence.hxx>

namespace {

static bool MatchExtCi(const std::string& s, size_t dot, const char* lit, size_t n) {
    if (dot + n > s.size()) {
        return false;
    }
    for (size_t i = 0; i < n; ++i) {
        if (std::tolower((unsigned char)s[dot + i]) != std::tolower((unsigned char)lit[i])) {
            return false;
        }
    }
    return true;
}

//! OCCT stores assembly component names as Unicode (TDataStd_Name).
static std::string NameAttrToUtf8(const Handle(TDataStd_Name)& nameAttr) {
    if (nameAttr.IsNull()) {
        return {};
    }
    TCollection_ExtendedString ext = nameAttr->Get();
    Standard_Integer nb = ext.LengthOfCString();
    if (nb <= 0) {
        return {};
    }
    std::vector<char> buf(static_cast<size_t>(nb) + 2, '\0');
    Standard_PCharacter pch = buf.data();
    Standard_Integer w = ext.ToUTF8CString(pch);
    if (w <= 0) {
        return {};
    }
    return std::string(buf.data());
}

static std::string StripTrailingCadAnnotations(std::string s) {
    const char* markers[] = {" (Default)", " (默认)", "\t(Default)", "(Default)"};
    for (const char* m : markers) {
        const size_t p = s.find(m);
        if (p != std::string::npos) {
            s.resize(p);
        }
    }
    while (!s.empty() && std::isspace((unsigned char)s.back())) {
        s.pop_back();
    }
    while (!s.empty() && std::isspace((unsigned char)s.front())) {
        s.erase(s.begin());
    }
    return s;
}

//! SolidWorks-style: "(固定) M02142A71000.stp<2>(Default)..." -> stem "M02142A71000_2"
static std::string ParseStepFileStemFromLabelName(const std::string& raw) {
    std::string best;
    for (size_t dot = 0; dot < raw.size(); ++dot) {
        if (raw[dot] != '.') {
            continue;
        }
        size_t extLen = 0;
        if (MatchExtCi(raw, dot, ".step", 5)) {
            extLen = 5;
        } else if (MatchExtCi(raw, dot, ".stp", 4)) {
            extLen = 4;
        } else {
            continue;
        }

        size_t extEnd = dot + extLen;
        std::string instDigits;
        if (extEnd < raw.size() && raw[extEnd] == '<') {
            const size_t gt = raw.find('>', extEnd);
            if (gt != std::string::npos) {
                for (size_t k = extEnd + 1; k < gt; ++k) {
                    if (std::isdigit((unsigned char)raw[k])) {
                        instDigits.push_back(raw[k]);
                    }
                }
                extEnd = gt + 1;
            }
        }

        size_t nameStart = 0;
        const size_t rp = raw.rfind(')', dot);
        if (rp != std::string::npos && rp < dot) {
            nameStart = rp + 1;
            while (nameStart < dot && std::isspace((unsigned char)raw[nameStart])) {
                nameStart++;
            }
        } else {
            nameStart = dot;
            while (nameStart > 0) {
                const unsigned char c = (unsigned char)raw[nameStart - 1];
                if (c == '(' || c == '<') {
                    break;
                }
                nameStart--;
            }
        }

        if (nameStart > dot) {
            continue;
        }

        std::string withExt = raw.substr(nameStart, dot + extLen - nameStart);
        while (!withExt.empty() && std::isspace((unsigned char)withExt.back())) {
            withExt.pop_back();
        }
        if (withExt.empty()) {
            continue;
        }

        std::filesystem::path fp(withExt);
        std::string stem = fp.filename().stem().string();
        if (!instDigits.empty()) {
            stem += "_" + instDigits;
        }
        best = std::move(stem);
    }
    return best;
}

//! USD prim segment rules (subset compatible with pxr TfMakeValidIdentifier).
static std::string ToUsdPrimSegment(std::string logicalName) {
    logicalName = StripTrailingCadAnnotations(std::move(logicalName));
    std::string out;
    out.reserve(logicalName.size());
    for (unsigned char uc : logicalName) {
        if (std::isalnum(uc)) {
            out += static_cast<char>(uc);
        } else if (uc == '_' || uc == '-' || uc == '.' || uc == '/' || uc == '\\') {
            out += '_';
        } else if (std::isspace(uc)) {
            out += '_';
        } else {
            out += '_';
        }
    }
    std::string collapsed;
    for (char c : out) {
        if (c == '_' && !collapsed.empty() && collapsed.back() == '_') {
            continue;
        }
        collapsed += c;
    }
    out = collapsed;
    while (!out.empty() && out.front() == '_') {
        out.erase(out.begin());
    }
    while (!out.empty() && out.back() == '_') {
        out.pop_back();
    }
    if (out.empty()) {
        out = "part";
    }
    if (std::isdigit((unsigned char)out[0])) {
        out = "_" + out;
    }
    return out;
}

//! OCCT XCAF appearance: VisMaterial (PBR/Common) preferred; else ColorTool RGB/RGBA; optional manufacturing density on label.
//! Texture maps from STEP are not exported to USD here (no UV assumed).
static void FillShapeAppearance(const TDF_Label& shapeLabel,
                                const TopoDS_Shape& shape,
                                const Handle(XCAFDoc_ColorTool)& colorTool,
                                const Handle(XCAFDoc_VisMaterialTool)& visMatTool,
                                const Handle(XCAFDoc_MaterialTool)& matTool,
                                NamedShape& ns) {
    Handle(XCAFDoc_VisMaterial) vis;
    if (!visMatTool.IsNull()) {
        vis = XCAFDoc_VisMaterialTool::GetShapeMaterial(shapeLabel);
        if (vis.IsNull() && !shape.IsNull()) {
            vis = visMatTool->GetShapeMaterial(shape);
        }
    }

    if (!vis.IsNull() && !vis->IsEmpty()) {
        if (vis->HasPbrMaterial()) {
            const XCAFDoc_VisMaterialPBR& pbr = vis->PbrMaterial();
            const Quantity_Color rgb = pbr.BaseColor.GetRGB();
            ns.diffuseColor[0] = static_cast<float>(rgb.Red());
            ns.diffuseColor[1] = static_cast<float>(rgb.Green());
            ns.diffuseColor[2] = static_cast<float>(rgb.Blue());
            ns.opacity = static_cast<float>(pbr.BaseColor.Alpha());
            ns.metallic = static_cast<float>(pbr.Metallic);
            ns.roughness = static_cast<float>(pbr.Roughness);
            ns.emissive[0] = pbr.EmissiveFactor.x();
            ns.emissive[1] = pbr.EmissiveFactor.y();
            ns.emissive[2] = pbr.EmissiveFactor.z();
            ns.hasDiffuseColor = true;
            ns.bindUsdPreviewMaterial = true;
            return;
        }
        if (vis->HasCommonMaterial()) {
            const XCAFDoc_VisMaterialCommon& cm = vis->CommonMaterial();
            ns.diffuseColor[0] = static_cast<float>(cm.DiffuseColor.Red());
            ns.diffuseColor[1] = static_cast<float>(cm.DiffuseColor.Green());
            ns.diffuseColor[2] = static_cast<float>(cm.DiffuseColor.Blue());
            ns.opacity = std::clamp(1.f - static_cast<float>(cm.Transparency), 0.f, 1.f);
            ns.metallic = 0.f;
            ns.roughness = std::clamp(1.f - cm.Shininess / 128.f, 0.f, 1.f);
            ns.emissive[0] = static_cast<float>(cm.EmissiveColor.Red());
            ns.emissive[1] = static_cast<float>(cm.EmissiveColor.Green());
            ns.emissive[2] = static_cast<float>(cm.EmissiveColor.Blue());
            ns.hasDiffuseColor = true;
            ns.bindUsdPreviewMaterial = true;
            return;
        }
        const Quantity_ColorRGBA ba = vis->BaseColor();
        const Quantity_Color rgb = ba.GetRGB();
        ns.diffuseColor[0] = static_cast<float>(rgb.Red());
        ns.diffuseColor[1] = static_cast<float>(rgb.Green());
        ns.diffuseColor[2] = static_cast<float>(rgb.Blue());
        ns.opacity = ba.Alpha();
        ns.hasDiffuseColor = true;
        ns.bindUsdPreviewMaterial = true;
        return;
    }

    Quantity_ColorRGBA rgba;
    bool gotRgba = false;
    if (!colorTool.IsNull()) {
        if (colorTool->GetColor(shapeLabel, XCAFDoc_ColorSurf, rgba)) {
            gotRgba = true;
        } else if (colorTool->GetColor(shapeLabel, XCAFDoc_ColorGen, rgba)) {
            gotRgba = true;
        } else if (!shape.IsNull() && colorTool->GetColor(shape, XCAFDoc_ColorSurf, rgba)) {
            gotRgba = true;
        } else if (!shape.IsNull() && colorTool->GetColor(shape, XCAFDoc_ColorGen, rgba)) {
            gotRgba = true;
        }
    }
    if (gotRgba) {
        const Quantity_Color rgb = rgba.GetRGB();
        ns.diffuseColor[0] = static_cast<float>(rgb.Red());
        ns.diffuseColor[1] = static_cast<float>(rgb.Green());
        ns.diffuseColor[2] = static_cast<float>(rgb.Blue());
        ns.opacity = rgba.Alpha();
        ns.hasDiffuseColor = true;
        ns.bindUsdPreviewMaterial = false;
        return;
    }

    Quantity_Color qc;
    bool gotRgb = false;
    if (!colorTool.IsNull()) {
        if (colorTool->GetColor(shapeLabel, XCAFDoc_ColorSurf, qc)) {
            gotRgb = true;
        } else if (colorTool->GetColor(shapeLabel, XCAFDoc_ColorGen, qc)) {
            gotRgb = true;
        } else if (!shape.IsNull() && colorTool->GetColor(shape, XCAFDoc_ColorSurf, qc)) {
            gotRgb = true;
        } else if (!shape.IsNull() && colorTool->GetColor(shape, XCAFDoc_ColorGen, qc)) {
            gotRgb = true;
        }
    }
    if (gotRgb) {
        ns.diffuseColor[0] = static_cast<float>(qc.Red());
        ns.diffuseColor[1] = static_cast<float>(qc.Green());
        ns.diffuseColor[2] = static_cast<float>(qc.Blue());
        ns.opacity = 1.f;
        ns.hasDiffuseColor = true;
        ns.bindUsdPreviewMaterial = false;
    }

    if (!matTool.IsNull()) {
        const Standard_Real rho = XCAFDoc_MaterialTool::GetDensityForShape(shapeLabel);
        if (rho > 1e-12) {
            ns.physicalMaterialDensity = static_cast<double>(rho);
        }
    }
}

static void FindBestAssemblyLevel(const TopoDS_Shape& shape, int currentDepth, int maxDepth,
                                   std::map<int, std::vector<TopoDS_Shape>>& candidates) {
    if (currentDepth >= maxDepth) return;
    
    if (shape.ShapeType() != TopAbs_COMPOUND) return;
    
    int compSolidCount = 0;
    for (TopExp_Explorer exp(shape, TopAbs_COMPSOLID, TopAbs_COMPOUND); exp.More(); exp.Next()) {
        compSolidCount++;
    }
    
    int compoundCount = 0;
    for (TopExp_Explorer exp(shape, TopAbs_COMPOUND, TopAbs_COMPOUND); exp.More(); exp.Next()) {
        compoundCount++;
    }
    
    int solidCount = 0;
    for (TopExp_Explorer exp(shape, TopAbs_SOLID, TopAbs_COMPOUND); exp.More(); exp.Next()) {
        solidCount++;
    }
    
    Logger& logger = Logger::GetInstance();
    logger.LogInfo("    Depth " + std::to_string(currentDepth) + ": " + 
                  std::to_string(compoundCount) + " COMPOUNDs, " +
                  std::to_string(compSolidCount) + " COMPSOLIDs, " +
                  std::to_string(solidCount) + " SOLIDs");
    
    if (compSolidCount >= 5 && compSolidCount <= 100) {
        std::vector<TopoDS_Shape> children;
        for (TopExp_Explorer exp(shape, TopAbs_COMPSOLID, TopAbs_COMPOUND); exp.More(); exp.Next()) {
            children.push_back(exp.Current());
        }
        candidates[currentDepth] = children;
        return;
    }
    
    if (compoundCount >= 5 && compoundCount <= 100) {
        std::vector<TopoDS_Shape> children;
        for (TopExp_Explorer exp(shape, TopAbs_COMPOUND, TopAbs_COMPOUND); exp.More(); exp.Next()) {
            children.push_back(exp.Current());
        }
        candidates[currentDepth] = children;
        return;
    }
    
    if (compoundCount == 1) {
        TopExp_Explorer exp(shape, TopAbs_COMPOUND, TopAbs_COMPOUND);
        if (exp.More()) {
            FindBestAssemblyLevel(exp.Current(), currentDepth + 1, maxDepth, candidates);
        }
    } else if (compSolidCount == 1) {
        TopExp_Explorer exp(shape, TopAbs_COMPSOLID, TopAbs_COMPOUND);
        if (exp.More()) {
            FindBestAssemblyLevel(exp.Current(), currentDepth + 1, maxDepth, candidates);
        }
    }
}

static void ExtractAssemblyComponents(const TopoDS_Shape& shape, const std::string& parentPath,
                                      std::vector<NamedShape>& namedShapes, int& counter,
                                      int currentDepth, int maxDepth) {
    Logger& logger = Logger::GetInstance();
    
    if (currentDepth >= maxDepth) return;
    
    TopAbs_ShapeEnum type = shape.ShapeType();
    
    if (type == TopAbs_COMPOUND) {
        std::map<int, std::vector<TopoDS_Shape>> candidates;
        FindBestAssemblyLevel(shape, 0, 10, candidates);
        
        logger.LogInfo("  Searching for best assembly level, found " + std::to_string(candidates.size()) + " candidate levels");
        for (const auto& pair : candidates) {
            logger.LogInfo("    Depth " + std::to_string(pair.first) + ": " + std::to_string(pair.second.size()) + " components");
        }
        
        int bestDepth = -1;
        int bestCount = 0;
        for (const auto& pair : candidates) {
            int depth = pair.first;
            int count = pair.second.size();
            
            if (count >= 5 && count <= 100) {
                if (bestDepth == -1 || count > bestCount) {
                    bestDepth = depth;
                    bestCount = count;
                }
            }
        }
        
        if (bestDepth >= 0) {
            logger.LogInfo("  Selected depth " + std::to_string(bestDepth) + " with " + std::to_string(bestCount) + " components");
            
            const std::vector<TopoDS_Shape>& selectedComponents = candidates[bestDepth];
            
            TopoDS_Shape currentShape = shape;
            for (int d = 0; d < bestDepth; d++) {
                TopExp_Explorer exp(currentShape, TopAbs_COMPOUND);
                if (exp.More()) {
                    currentShape = exp.Current();
                } else {
                    break;
                }
            }
            
            int index = 0;
            for (TopExp_Explorer exp(currentShape, TopAbs_COMPOUND); exp.More(); exp.Next(), index++) {
                const TopoDS_Shape& child = exp.Current();
                std::string partName = "Part_" + std::to_string(counter);
                std::string partPath = parentPath + "/" + partName;
                
                NamedShape namedShape;
                namedShape.shape = child;
                namedShape.name = partName;
                namedShape.path = partPath;
                namedShapes.push_back(namedShape);
                
                logger.LogInfo("    Added component: " + partName);
                counter++;
            }
            return;
        }
        
        int compoundCount = 0;
        for (TopExp_Explorer exp(shape, TopAbs_COMPOUND); exp.More(); exp.Next()) {
            compoundCount++;
        }
        
        int solidCount = 0;
        for (TopExp_Explorer exp(shape, TopAbs_SOLID); exp.More(); exp.Next()) {
            solidCount++;
        }
        
        if (compoundCount > 1 && compoundCount <= 100) {
            logger.LogInfo("  Found assembly level at depth " + std::to_string(currentDepth) + " with " + std::to_string(compoundCount) + " components");
            
            int index = 0;
            for (TopExp_Explorer exp(shape, TopAbs_COMPOUND); exp.More(); exp.Next(), index++) {
                const TopoDS_Shape& child = exp.Current();
                std::string partName = "Part_" + std::to_string(counter);
                std::string partPath = parentPath + "/" + partName;
                
                NamedShape namedShape;
                namedShape.shape = child;
                namedShape.name = partName;
                namedShape.path = partPath;
                namedShapes.push_back(namedShape);
                
                logger.LogInfo("    Added component: " + partName);
                counter++;
            }
            return;
        }
        
        if (compoundCount == 1) {
            TopExp_Explorer exp(shape, TopAbs_COMPOUND);
            if (exp.More()) {
                ExtractAssemblyComponents(exp.Current(), parentPath, namedShapes, counter, currentDepth + 1, maxDepth);
            }
            return;
        }
        
        if (solidCount > 0) {
            std::string partName = "Part_" + std::to_string(counter);
            std::string partPath = parentPath + "/" + partName;
            
            NamedShape namedShape;
            namedShape.shape = shape;
            namedShape.name = partName;
            namedShape.path = partPath;
            namedShapes.push_back(namedShape);
            
            logger.LogInfo("  Added leaf part: " + partName + " with " + std::to_string(solidCount) + " SOLIDs");
            counter++;
            return;
        }
    }
    
    if (type == TopAbs_SOLID || type == TopAbs_COMPSOLID) {
        std::string partName = "Part_" + std::to_string(counter);
        std::string partPath = parentPath + "/" + partName;
        
        NamedShape namedShape;
        namedShape.shape = shape;
        namedShape.name = partName;
        namedShape.path = partPath;
        namedShapes.push_back(namedShape);
        
        logger.LogInfo("  Added primitive part: " + partName);
        counter++;
    }
}

//! Walk XCAF assembly tree only (free shapes + components). Avoids iterating every
//! shape/subshape label under doc root — which produced tens of thousands of duplicate meshes.
static void CollectXcafPart(const TDF_Label& label,
                            const Handle(XCAFDoc_ShapeTool)& shapeTool,
                            const Handle(XCAFDoc_ColorTool)& colorTool,
                            const Handle(XCAFDoc_VisMaterialTool)& visMatTool,
                            const Handle(XCAFDoc_MaterialTool)& matTool,
                            const std::string& parentPath,
                            const TopLoc_Location& parentWorldLoc,
                            std::vector<NamedShape>& namedShapes,
                            std::set<std::string>& usedPaths,
                            int& counter,
                            int currentDepth,
                            int maxDepth,
                            Logger& logger) {
    if (currentDepth > maxDepth) {
        return;
    }
    if (!shapeTool->IsShape(label)) {
        return;
    }

    const TopLoc_Location ownLoc = shapeTool->GetLocation(label);
    const TopLoc_Location worldLoc = parentWorldLoc * ownLoc;

    std::string currentNodeName;
    Handle(TDataStd_Name) nameAttr;
    if (label.FindAttribute(TDataStd_Name::GetID(), nameAttr)) {
        const std::string utf8 = NameAttrToUtf8(nameAttr);
        const std::string fromStep = ParseStepFileStemFromLabelName(utf8);
        if (!fromStep.empty()) {
            currentNodeName = ToUsdPrimSegment(fromStep);
        } else if (!utf8.empty()) {
            currentNodeName = ToUsdPrimSegment(utf8);
        }
    }
    if (currentNodeName.empty()) {
        currentNodeName = "Part_" + std::to_string(counter++);
    }

    // Keep full STEP/XCAF depth; duplicate segment names are disambiguated below (do not merge into parent).
    const std::string basePath = parentPath + "/" + currentNodeName;
    std::string currentPath = basePath;
    int dup = 2;
    while (usedPaths.count(currentPath)) {
        currentPath = basePath + "_" + std::to_string(dup++);
    }
    usedPaths.insert(currentPath);

    const size_t pslash = parentPath.find_last_of('/');
    std::string parentNameSeg;
    if (pslash != std::string::npos && pslash + 1 < parentPath.size()) {
        parentNameSeg = parentPath.substr(pslash + 1);
    }

    if (shapeTool->IsAssembly(label)) {
        TDF_LabelSequence components;
        shapeTool->GetComponents(label, components, Standard_False);
        if (components.Length() > 0) {
            NamedShape groupNs;
            groupNs.path = currentPath;
            groupNs.name = currentNodeName;
            groupNs.parentName = parentNameSeg;
            groupNs.isAssemblyGroup = true;
            groupNs.shape = TopoDS_Shape();
            namedShapes.push_back(groupNs);
            logger.LogInfo("  Assembly group (USD Xform): " + currentPath);

            for (Standard_Integer i = 1; i <= components.Length(); ++i) {
                CollectXcafPart(components.Value(i), shapeTool, colorTool, visMatTool, matTool, currentPath,
                                worldLoc, namedShapes, usedPaths, counter, currentDepth + 1, maxDepth, logger);
            }
            return;
        }
    }

    // OCCT XCAFDoc_ShapeTool.hxx: "For component, returns new shape with correct location"
    // Implementation uses TNaming + S.Move() along instance chain — same basis as XCAF visualization.
    TopoDS_Shape shape = shapeTool->GetShape(label);
    if (shape.IsNull()) {
        return;
    }

    NamedShape namedShape;
    namedShape.shape = shape;
    const size_t leafSlash = currentPath.find_last_of('/');
    namedShape.name = (leafSlash != std::string::npos) ? currentPath.substr(leafSlash + 1) : currentPath;
    namedShape.path = currentPath;
    namedShape.parentName = parentNameSeg;
    FillShapeAppearance(label, shape, colorTool, visMatTool, matTool, namedShape);
    namedShapes.push_back(namedShape);

    logger.LogInfo("  Leaf part: " + currentPath);
}

} // namespace

static bool ReadStepIntoXcafDocument(const std::string& filePath, const Handle(TDocStd_Document)& doc,
                                     std::string& errorOut) {
    STEPCAFControl_Reader reader;
    reader.SetColorMode(true);
    reader.SetNameMode(true);
    reader.SetLayerMode(true);
    reader.SetMatMode(true);
    reader.SetPropsMode(true);

    IFSelect_ReturnStatus status = reader.ReadFile(filePath.c_str());
    if (status != IFSelect_RetDone) {
        errorOut = "Failed to read STEP file: " + filePath;
        return false;
    }
    if (!reader.Transfer(doc)) {
        errorOut = "Failed to transfer STEP file to XCAF document";
        return false;
    }
    Handle(XCAFDoc_ShapeTool) shapeTool = XCAFDoc_DocumentTool::ShapeTool(doc->Main());
    shapeTool->UpdateAssemblies();
    return true;
}

bool StepReader::Read(const std::string& filePath, TopoDS_Shape& shape) {
    m_lastError.clear();
    Logger& logger = Logger::GetInstance();

    if (!ErrorHandler::CheckFileExists(filePath)) {
        m_lastError = ErrorHandler::GetErrorMessage(ErrorCode::FileNotFound) + ": " + filePath;
        logger.LogError(m_lastError);
        return false;
    }

    try {
        logger.StartTimer("STEP file reading (STEPCAF XCAF)");

        Handle(TDocStd_Document) doc = new TDocStd_Document("MDTV-CAF");
        std::string xferErr;
        if (!ReadStepIntoXcafDocument(filePath, doc, xferErr)) {
            logger.LogWarning(xferErr + "; falling back to STEPControl_Reader.");

            STEPControl_Reader reader;
            IFSelect_ReturnStatus status = reader.ReadFile(filePath.c_str());
            if (status != IFSelect_RetDone) {
                m_lastError = "Failed to read STEP file: " + filePath;
                logger.LogError(m_lastError);
                logger.EndTimer("STEP file reading (STEPCAF XCAF)");
                return false;
            }
            if (!reader.TransferRoots()) {
                m_lastError = "No roots found in STEP file";
                logger.LogError(m_lastError);
                logger.EndTimer("STEP file reading (STEPCAF XCAF)");
                return false;
            }
            shape = reader.OneShape();
            if (shape.IsNull()) {
                m_lastError = "Empty shape after transfer";
                logger.LogError(m_lastError);
                logger.EndTimer("STEP file reading (STEPCAF XCAF)");
                return false;
            }
            logger.EndTimer("STEP file reading (STEPCAF XCAF)");
            return true;
        }

        Handle(XCAFDoc_ShapeTool) shapeTool = XCAFDoc_DocumentTool::ShapeTool(doc->Main());
        TDF_LabelSequence freeShapes;
        shapeTool->GetFreeShapes(freeShapes);
        if (freeShapes.Length() == 0) {
            m_lastError = "No free shapes in XCAF document";
            logger.LogError(m_lastError);
            logger.EndTimer("STEP file reading (STEPCAF XCAF)");
            return false;
        }

        if (freeShapes.Length() == 1) {
            shape = shapeTool->GetShape(freeShapes.Value(1));
        } else {
            BRep_Builder bb;
            TopoDS_Compound compound;
            bb.MakeCompound(compound);
            for (Standard_Integer i = 1; i <= freeShapes.Length(); ++i) {
                TopoDS_Shape s = shapeTool->GetShape(freeShapes.Value(i));
                if (!s.IsNull()) {
                    bb.Add(compound, s);
                }
            }
            shape = compound;
        }

        if (shape.IsNull()) {
            m_lastError = "Empty shape after XCAF transfer";
            logger.LogError(m_lastError);
            logger.EndTimer("STEP file reading (STEPCAF XCAF)");
            return false;
        }

        logger.EndTimer("STEP file reading (STEPCAF XCAF)");
        return true;
    }
    catch (const std::exception& e) {
        m_lastError = "Exception during STEP reading: " + std::string(e.what());
        logger.LogError(m_lastError);
        return false;
    }
    catch (...) {
        m_lastError = "Unknown exception during STEP reading";
        logger.LogError(m_lastError);
        return false;
    }
}

bool StepReader::ReadAssembly(const std::string& filePath, std::vector<TopoDS_Shape>& shapes) {
    m_lastError.clear();
    shapes.clear();

    if (!ErrorHandler::CheckFileExists(filePath)) {
        m_lastError = ErrorHandler::GetErrorMessage(ErrorCode::FileNotFound) + ": " + filePath;
        return false;
    }

    try {
        Handle(TDocStd_Document) doc = new TDocStd_Document("MDTV-CAF");
        std::string xferErr;
        if (!ReadStepIntoXcafDocument(filePath, doc, xferErr)) {
            m_lastError = xferErr;
            STEPControl_Reader reader;
            IFSelect_ReturnStatus status = reader.ReadFile(filePath.c_str());
            if (status != IFSelect_RetDone) {
                return false;
            }
            int nbRoots = reader.NbRootsForTransfer();
            if (nbRoots == 0) {
                return false;
            }
            for (int i = 1; i <= nbRoots; ++i) {
                reader.TransferRoot(i);
                TopoDS_Shape shape = reader.Shape(i);
                if (!shape.IsNull()) {
                    shapes.push_back(shape);
                }
            }
            if (shapes.empty()) {
                m_lastError = "No valid shapes extracted from assembly";
            }
            return !shapes.empty();
        }

        Logger& logger = Logger::GetInstance();
        Handle(XCAFDoc_ShapeTool) shapeTool = XCAFDoc_DocumentTool::ShapeTool(doc->Main());
        TDF_LabelSequence freeShapes;
        shapeTool->GetFreeShapes(freeShapes);
        logger.LogInfo("ReadAssembly (XCAF): " + std::to_string(freeShapes.Length()) + " free shape root(s)");

        for (Standard_Integer i = 1; i <= freeShapes.Length(); ++i) {
            TopoDS_Shape s = shapeTool->GetShape(freeShapes.Value(i));
            if (!s.IsNull()) {
                shapes.push_back(s);
            }
        }

        if (shapes.empty()) {
            m_lastError = "No valid shapes extracted from XCAF document";
            return false;
        }
        return true;
    }
    catch (const std::exception& e) {
        m_lastError = "Exception during STEP reading: " + std::string(e.what());
        return false;
    }
    catch (...) {
        m_lastError = "Unknown exception during STEP reading";
        return false;
    }
}

bool StepReader::ReadAssemblyWithNames(const std::string& filePath, std::vector<NamedShape>& namedShapes) {
    m_lastError.clear();
    namedShapes.clear();

    if (!ErrorHandler::CheckFileExists(filePath)) {
        m_lastError = ErrorHandler::GetErrorMessage(ErrorCode::FileNotFound) + ": " + filePath;
        return false;
    }

    try {
        Logger& logger = Logger::GetInstance();
        
        std::filesystem::path stepPath(filePath);
        std::string sourceFileName = stepPath.stem().string();
        
        std::replace(sourceFileName.begin(), sourceFileName.end(), ' ', '_');
        std::replace(sourceFileName.begin(), sourceFileName.end(), '/', '_');
        std::replace(sourceFileName.begin(), sourceFileName.end(), '\\', '_');
        std::replace(sourceFileName.begin(), sourceFileName.end(), '.', '_');
        std::replace(sourceFileName.begin(), sourceFileName.end(), '-', '_');
        if (!sourceFileName.empty() && isdigit(sourceFileName[0])) {
            sourceFileName = "_" + sourceFileName;
        }
        
        Handle(TDocStd_Document) doc = new TDocStd_Document("MDTV-CAF");
        std::string xferErr;
        if (!ReadStepIntoXcafDocument(filePath, doc, xferErr)) {
            m_lastError = xferErr;
            return false;
        }

        Handle(XCAFDoc_ShapeTool) shapeTool = XCAFDoc_DocumentTool::ShapeTool(doc->Main());
        Handle(XCAFDoc_ColorTool) colorTool = XCAFDoc_DocumentTool::ColorTool(doc->Main());
        Handle(XCAFDoc_VisMaterialTool) visMatTool = XCAFDoc_DocumentTool::VisMaterialTool(doc->Main());
        Handle(XCAFDoc_MaterialTool) matTool = XCAFDoc_DocumentTool::MaterialTool(doc->Main());

        logger.LogInfo("ReadAssemblyWithNames: Using XCAF document to extract named parts");
        logger.LogInfo("Source file name: " + sourceFileName);
        
        int counter = 0;
        std::set<std::string> usedPaths;
        TopLoc_Location identityLoc;

        TDF_LabelSequence freeShapes;
        shapeTool->GetFreeShapes(freeShapes);
        logger.LogInfo("ReadAssemblyWithNames: Found " + std::to_string(freeShapes.Length()) + " free shape root(s)");

        const std::string rootPath = "/" + sourceFileName;
        for (Standard_Integer ri = 1; ri <= freeShapes.Length(); ++ri) {
            CollectXcafPart(freeShapes.Value(ri), shapeTool, colorTool, visMatTool, matTool, rootPath, identityLoc,
                            namedShapes, usedPaths, counter, 0, 64, logger);
        }
        
        logger.LogInfo("ReadAssemblyWithNames: Extracted " + std::to_string(namedShapes.size()) + " named parts from XCAF");

        if (namedShapes.empty()) {
            logger.LogWarning("No named parts found in XCAF, falling back to simple extraction");
            
            STEPControl_Reader simpleReader;
            IFSelect_ReturnStatus status = simpleReader.ReadFile(filePath.c_str());
            if (status != IFSelect_RetDone) {
                m_lastError = "Failed to read STEP file: " + filePath;
                return false;
            }

            int nbRoots = simpleReader.NbRootsForTransfer();
            if (nbRoots == 0) {
                m_lastError = "No roots found in STEP file";
                return false;
            }

            logger.LogInfo("ReadAssemblyWithNames: Found " + std::to_string(nbRoots) + " root shapes");

            for (int i = 1; i <= nbRoots; ++i) {
                simpleReader.TransferRoot(i);
                TopoDS_Shape shape = simpleReader.Shape(i);
                if (!shape.IsNull()) {
                    logger.LogInfo("  Root " + std::to_string(i) + " type: " + std::to_string((int)shape.ShapeType()));
                    
                    std::string assemblyName = "Assembly_" + std::to_string(i);
                    std::string assemblyPath = "/" + assemblyName;
                    
                    counter = 0;
                    ExtractAssemblyComponents(shape, assemblyPath, namedShapes, counter, 0, 10);
                }
            }
        }

        if (namedShapes.empty()) {
            logger.LogWarning("No components found, falling back to simple extraction");
            
            STEPControl_Reader simpleReader;
            IFSelect_ReturnStatus status = simpleReader.ReadFile(filePath.c_str());
            if (status != IFSelect_RetDone) {
                m_lastError = "Failed to read STEP file: " + filePath;
                return false;
            }
            
            int nbRoots = simpleReader.NbRootsForTransfer();
            for (int i = 1; i <= nbRoots; ++i) {
                simpleReader.TransferRoot(i);
                TopoDS_Shape shape = simpleReader.Shape(i);
                if (!shape.IsNull()) {
                    std::string partName = "Assembly_" + std::to_string(i);
                    NamedShape namedShape;
                    namedShape.shape = shape;
                    namedShape.name = partName;
                    namedShape.path = "/" + partName;
                    namedShapes.push_back(namedShape);
                }
            }
        }

        if (namedShapes.empty()) {
            m_lastError = "No valid shapes extracted from assembly";
            return false;
        }

        logger.LogInfo("ReadAssemblyWithNames: Successfully extracted " + std::to_string(namedShapes.size()) + " components");
        
        for (size_t j = 0; j < namedShapes.size() && j < 20; ++j) {
            logger.LogInfo("  Component " + std::to_string(j) + ": " + namedShapes[j].name + ", path: " + namedShapes[j].path);
        }
        
        return true;
    }
    catch (const std::exception& e) {
        m_lastError = "Exception during STEP reading: " + std::string(e.what());
        Logger::GetInstance().LogError(m_lastError);
        return false;
    }
    catch (...) {
        m_lastError = "Unknown exception during STEP reading";
        Logger::GetInstance().LogError(m_lastError);
        return false;
    }
}