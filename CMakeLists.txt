cmake_minimum_required(VERSION 3.20)
project(VarjoXR LANGUAGES CXX)

option(VARJOXR_ENABLE_D3D11 "Build VarjoXR D3D11 backend" ON)
option(VARJOXR_ENABLE_D3D12 "Build VarjoXR D3D12 backend" ON)
option(VARJOXR_ENABLE_WARNINGS "Enable warnings for VarjoXR" ON)

set(VARJOXR_VARJO_INCLUDE_DIR "" CACHE PATH "Directory containing Varjo.h / Varjo_layers.h")
set(VARJOXR_VARJO_LIBRARY "" CACHE FILEPATH "Path to VarjoLib import library")

add_library(VarjoXR STATIC)
add_library(VarjoXR::VarjoXR ALIAS VarjoXR)

target_compile_features(VarjoXR PUBLIC cxx_std_17)
target_include_directories(VarjoXR PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

if(VARJOXR_VARJO_INCLUDE_DIR)
    target_include_directories(VarjoXR PUBLIC "${VARJOXR_VARJO_INCLUDE_DIR}")
endif()

if(VARJOXR_VARJO_LIBRARY)
    target_link_libraries(VarjoXR PUBLIC "${VARJOXR_VARJO_LIBRARY}")
else()
    target_link_libraries(VarjoXR PUBLIC VarjoLib)
endif()

target_sources(VarjoXR PRIVATE
    src/Transform.cpp
    src/XRObject.cpp
    src/XRPlane.cpp
    src/XRSpace.cpp
    src/VarjoSession.cpp
    src/Detail/VarjoLayerSubmit.cpp)

if(VARJOXR_ENABLE_D3D11)
    find_package(D3D11Helper CONFIG QUIET)
    if(NOT TARGET D3D11Helper::D3D11Helper)
        message(FATAL_ERROR "VARJOXR_ENABLE_D3D11=ON requires target D3D11Helper::D3D11Helper. Add D3D11Helper by find_package() or add_subdirectory().")
    endif()
    target_compile_definitions(VarjoXR PUBLIC VARJOXR_ENABLE_D3D11=1)
    target_link_libraries(VarjoXR PUBLIC D3D11Helper::D3D11Helper)
    target_sources(VarjoXR PRIVATE src/Backends/D3D11/D3D11Backend.cpp)
endif()

if(VARJOXR_ENABLE_D3D12)
    find_package(D3D12Helper CONFIG QUIET)
    if(NOT TARGET D3D12Helper::D3D12Helper)
        message(FATAL_ERROR "VARJOXR_ENABLE_D3D12=ON requires target D3D12Helper::D3D12Helper. Add D3D12Helper by find_package() or add_subdirectory().")
    endif()
    target_compile_definitions(VarjoXR PUBLIC VARJOXR_ENABLE_D3D12=1)
    target_link_libraries(VarjoXR PUBLIC D3D12Helper::D3D12Helper)
    target_sources(VarjoXR PRIVATE src/Backends/D3D12/D3D12Backend.cpp)
endif()

if(MSVC AND VARJOXR_ENABLE_WARNINGS)
    target_compile_options(VarjoXR PRIVATE /W4 /permissive-)
endif()

# This library intentionally does not include or compile Varjo SDK examples/Common.
# Only Varjo Native SDK headers/import library and the user's DirectX helper libraries are used.
