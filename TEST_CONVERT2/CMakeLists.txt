cmake_minimum_required(VERSION 3.15)
project(StepToStlConverter LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set these paths according to your OpenCASCADE and OpenUSD installation
set(OpenCASCADE_INSTALL_PREFIX "" CACHE PATH "Path to OpenCASCADE installation")
set(USD_INSTALL_PREFIX "" CACHE PATH "Path to OpenUSD installation")

if(OpenCASCADE_INSTALL_PREFIX)
    set(OpenCASCADE_INCLUDE_DIR "${OpenCASCADE_INSTALL_PREFIX}/inc")
    set(OpenCASCADE_LIBRARY_DIR "${OpenCASCADE_INSTALL_PREFIX}/win64/vc14/lib")
    message(STATUS "Found OpenCASCADE SDK: ${OpenCASCADE_INSTALL_PREFIX}")
endif()

if(USD_INSTALL_PREFIX)
    set(USD_INCLUDE_DIR "${USD_INSTALL_PREFIX}/include")
    set(USD_LIBRARY_DIR "${USD_INSTALL_PREFIX}/lib")
    set(USD_BINARY_DIR "${USD_INSTALL_PREFIX}/bin")
    set(USD_PYTHON_INCLUDE_DIR "${USD_INSTALL_PREFIX}/python/include")
    set(USD_PYTHON_LIBRARY_DIR "${USD_INSTALL_PREFIX}/python/libs")
    set(USD_FOUND ON)
    message(STATUS "Found OpenUSD SDK: ${USD_INSTALL_PREFIX}")
    message(STATUS "Python include: ${USD_PYTHON_INCLUDE_DIR}")
else()
    set(USD_FOUND OFF)
    message(STATUS "OpenUSD not found, USD output will be disabled")
endif()

add_executable(StepToStlConverter
  src/main.cpp
  src/StepReader.cpp
  src/MeshGenerator.cpp
  src/MeshExtractor.cpp
  src/StlWriter.cpp
  src/UsdWriter.cpp
  src/UsdWriterCore.cpp
  src/Converter.cpp
  src/ErrorHandler.cpp
  src/Logger.cpp
)

target_include_directories(StepToStlConverter PRIVATE
  ${OpenCASCADE_INCLUDE_DIR}
  ${PROJECT_SOURCE_DIR}/include
)

if(USD_FOUND)
    target_include_directories(StepToStlConverter PRIVATE
        ${USD_INCLUDE_DIR}
        ${USD_PYTHON_INCLUDE_DIR}
    )
    set(USD_FALLBACK_PLUGIN_ROOT "${USD_INSTALL_PREFIX}/lib/usd")
    target_compile_definitions(StepToStlConverter PRIVATE 
        USD_FOUND
        "USD_FALLBACK_PLUGIN_ROOT=\"${USD_FALLBACK_PLUGIN_ROOT}\""
    )
endif()

target_compile_definitions(StepToStlConverter PRIVATE 
  NOMINMAX
)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL$<$<CONFIG:Debug>:DebugDLL>")

target_link_directories(StepToStlConverter PRIVATE
  ${OpenCASCADE_LIBRARY_DIR}
)

if(USD_FOUND)
    target_link_directories(StepToStlConverter PRIVATE
        ${USD_LIBRARY_DIR}
        ${USD_PYTHON_LIBRARY_DIR}
    )
endif()

target_link_libraries(StepToStlConverter PRIVATE
  TKDESTEP
  TKDESTL
  TKMesh
  TKBRep
  TKGeomAlgo
  TKGeomBase
  TKTopAlgo
  TKBO
  TKBool
  TKXSBase
  TKDE
  TKG3d
  TKG2d
  TKMath
  TKernel
  TKShHealing
  TKPrim
  TKXCAF
  TKLCAF
  TKXmlL
  TKXml
  TKBinL
  TKBin
  TKCDF
)

if(USD_FOUND)
    target_link_libraries(StepToStlConverter PRIVATE
        usd_tf
        usd_sdf
        usd_vt
        usd_gf
        usd_usd
        usd_usdGeom
        usd_usdShade
        usd_ar
        usd_plug
        usd_pcp
        usd_arch
        usd_kind
        usd_python
        usd_boost
        usd_sdr
        usd_ts
        usd_work
        usd_trace
    )
endif()

install(TARGETS StepToStlConverter
  RUNTIME DESTINATION bin
)