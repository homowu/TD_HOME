# TD_HOME

这是一个包含多个3D模型转换工具和相关资源的项目仓库。

## 📁 项目结构

```
TD_HOME/
├── SLDPRT_convert/     # SolidWorks SLDPRT文件批量转换器
├── TEST_CONVERT2/      # STEP到STL/USD格式转换器
└── HDA_TOOLS/          # Houdini数字资产(HDA)
```

## 🛠️ 项目说明

### 1. SLDPRT_convert

**SolidWorks批量转换器** - 基于C# .NET Framework开发的Windows应用程序

**功能特性:**
- 批量转换SolidWorks SLDPRT文件
- 支持转换为多种3D格式（OBJ, STL等）
- 使用Assimp库进行3D模型处理

**技术栈:**
- C# .NET Framework 4.8
- AssimpNet (3D模型导入库)
- Windows Forms

**使用方式:**
```bash
# 使用Visual Studio打开项目
# SolidWorksBatchConverter.csproj
```

---

### 2. TEST_CONVERT2

**STEP到STL/USD转换器** - 基于C++和OpenCASCADE的3D模型转换工具

**功能特性:**
- STEP文件读取和解析
- 网格生成和优化
- 支持输出STL和USD格式
- 批量处理模式

**技术栈:**
- C++17
- OpenCASCADE (CAD几何库)
- OpenUSD (可选，USD格式支持)
- CMake

**构建方式:**
```bash
# 创建构建目录
mkdir build && cd build

# 配置CMake（需指定依赖路径）
cmake .. -DOpenCASCADE_INSTALL_PREFIX=/path/to/opencascade -DUSD_INSTALL_PREFIX=/path/to/openusd

# 编译
cmake --build . --config Release
```

**命令行使用:**
```bash
# 单文件转换
StepToStlConverter input.step output.stl

# 批量转换
StepToStlConverter -batch ./step_files ./stl_output

# 自定义网格参数
StepToStlConverter -d 0.01 -a 0.3 input.step output.stl
```

**参数说明:**
- `-d, --deflection` - 网格偏转值（默认: 0.1）
- `-a, --angle` - 角度容差（弧度，默认: 0.5）
- `-r, --relative` - 使用相对偏转
- `-f, --fixed` - 使用固定偏转
- `-max, --max-triangles` - 最大三角形数量
- `-batch` - 批量处理模式
- `-test` - 生成测试STEP文件

---

### 3. HDA_TOOLS

**Houdini数字资产:**
- `lop_sop_uvsamplecamv2.3.4.hda` - UV采样相机工具
- `sop_geo_damage_beta.1.11.hda` - 几何体损坏效果工具

---

## 📋 安装要求

### SLDPRT_convert
- Windows 10/11
- .NET Framework 4.8 或更高版本
- Visual Studio 2019+

### TEST_CONVERT2
- Windows 10/11 (64位)
- CMake 3.15+
- Visual Studio 2019+
- **依赖:**
  - OpenCASCADE 7.8+ (必需)
  - OpenUSD 23.08+ (可选，USD输出支持)

---

## 🚀 快速开始

```bash
# 克隆仓库
git clone https://github.com/homowu/TD_HOME.git
cd TD_HOME

# 对于 SLDPRT_convert
# 使用Visual Studio打开 SLDPRT_convert/SolidWorksBatchConverter.csproj

# 对于 TEST_CONVERT2
mkdir build && cd build
cmake .. -DOpenCASCADE_INSTALL_PREFIX=path/to/occt
cmake --build . --config Release
```

---

## 📄 许可证

- **OpenCASCADE:** LGPL 2.1 (见 `TEST_CONVERT2/occt_install/LICENSE_LGPL_21.txt`)
- **项目源代码:** MIT License

---

## 🤝 贡献

欢迎提交Issue和Pull Request！

---

## 📧 联系方式

如有问题或建议，请通过GitHub Issues联系。
