cmake_minimum_required(VERSION 3.12)
project(VarjoXR)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (POLICY CMP0111)
  cmake_policy(SET CMP0111 OLD)
endif()

# Expects to sit next to the Varjo SDK's "include" and "examples" directories, e.g.:
#   VarjoSDK/include/...
#   VarjoSDK/examples/Common/...
#   VarjoSDK/examples/vendor/GLM/...
#   VarjoSDK/VarjoXR/            <- this CMakeLists.txt
set(VARJO_SDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")

# --- Varjo Native SDK (imported, mirrors examples/CMakeLists.txt) ---
add_library(VarjoLib SHARED IMPORTED GLOBAL)
set_target_properties(VarjoLib PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${VARJO_SDK_ROOT}/include"
    IMPORTED_LOCATION "${VARJO_SDK_ROOT}/examples/bin/VarjoLib.dll"
    IMPORTED_IMPLIB "${VARJO_SDK_ROOT}/examples/lib/VarjoLib.lib"
)

# --- GLM (header only, vendored with the SDK examples) ---
add_library(GLM INTERFACE)
target_include_directories(GLM INTERFACE
    "${VARJO_SDK_ROOT}/examples/vendor/GLM/include"
)

# --- Varjo example "Common" framework (Session/MultiLayerView/D3D11Renderer/Scene) ---
# VarjoXR's L1/L2 layers are thin wrappers around these, so they are compiled directly into the
# VarjoXR static library instead of being duplicated.
set(_common_dir "${VARJO_SDK_ROOT}/examples/Common")
set(_sources_common
    "${_common_dir}/Globals.hpp"
    "${_common_dir}/Globals.cpp"
    "${_common_dir}/Session.hpp"
    "${_common_dir}/Session.cpp"
    "${_common_dir}/SyncView.hpp"
    "${_common_dir}/SyncView.cpp"
    "${_common_dir}/Scene.hpp"
    "${_common_dir}/Scene.cpp"
    "${_common_dir}/Renderer.hpp"
    "${_common_dir}/Renderer.cpp"
    "${_common_dir}/ExampleShaders.hpp"
    "${_common_dir}/MultiLayerView.hpp"
    "${_common_dir}/MultiLayerView.cpp"
    "${_common_dir}/D3D11Renderer.hpp"
    "${_common_dir}/D3D11Renderer.cpp"
    "${_common_dir}/D3D11Shaders.hpp"
    "${_common_dir}/D3D11Shaders.cpp"
    "${_common_dir}/D3D11MultiLayerView.hpp"
    "${_common_dir}/D3D11MultiLayerView.cpp"
)

set(_include_dir "${CMAKE_CURRENT_SOURCE_DIR}/include/VarjoXR")
set(_src_dir "${CMAKE_CURRENT_SOURCE_DIR}/src")

set(_sources_varjoxr
    "${_include_dir}/Eye.hpp"
    "${_include_dir}/Transform.hpp"
    "${_include_dir}/Material.hpp"
    "${_include_dir}/ResourceManager.hpp"
    "${_src_dir}/ResourceManager.cpp"
    "${_include_dir}/XRObject.hpp"
    "${_src_dir}/XRObject.cpp"
    "${_include_dir}/XRPlane.hpp"
    "${_src_dir}/XRPlane.cpp"
    "${_include_dir}/XRView.hpp"
    "${_src_dir}/XRView.cpp"
    "${_include_dir}/XRSession.hpp"
    "${_src_dir}/XRSession.cpp"
    "${_include_dir}/XRScene.hpp"
    "${_src_dir}/XRScene.cpp"
    "${_include_dir}/XRSpace.hpp"
    "${_src_dir}/XRSpace.cpp"
)

source_group("Common" FILES ${_sources_common})
source_group("VarjoXR" FILES ${_sources_varjoxr})

add_library(VarjoXR STATIC ${_sources_common} ${_sources_varjoxr})

target_include_directories(VarjoXR
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE
        "${_common_dir}"
)

target_compile_definitions(VarjoXR PUBLIC -D_UNICODE -DUNICODE -DNOMINMAX)

target_link_libraries(VarjoXR
    PUBLIC VarjoLib GLM
    PRIVATE d3d11 dxgi d3dcompiler windowscodecs
)

set_property(TARGET VarjoXR PROPERTY FOLDER "Libraries")
