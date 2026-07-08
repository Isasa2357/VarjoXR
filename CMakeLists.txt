cmake_minimum_required(VERSION 3.20)

project(VarjoXR
    VERSION 0.1.0
    LANGUAGES CXX
)

option(VARJOXR_ENABLE_D3D11 "Build VarjoXR D3D11 backend" ON)
option(VARJOXR_ENABLE_D3D12 "Build VarjoXR D3D12 backend" ON)
option(VARJOXR_BUILD_SAMPLES "Build VarjoXR samples" ON)
option(VARJOXR_BUILD_TESTS "Build VarjoXR tests" ON)
option(VARJOXR_ENABLE_WARNINGS "Enable compiler warnings for VarjoXR" ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT WIN32)
    message(WARNING "VarjoXR targets Varjo Native SDK and Direct3D. Non-Windows builds only support configure-time inspection.")
endif()

if(NOT TARGET VarjoToolkit::VarjoToolkit)
    find_package(VarjoToolkit 0.2.0 EXACT CONFIG REQUIRED)
endif()

find_package(glm CONFIG REQUIRED)

if(VARJOXR_ENABLE_D3D11 AND NOT TARGET D3D11Helper::D3D11Helper)
    find_package(D3D11Helper 1.12.0 EXACT CONFIG REQUIRED)
endif()

if(VARJOXR_ENABLE_D3D12 AND NOT TARGET D3D12Helper::D3D12Helper)
    find_package(D3D12Helper 1.12.0 EXACT CONFIG REQUIRED)
endif()

set(VARJOXR_PUBLIC_HEADERS
    include/VarjoXR/VarjoXR.hpp
    include/VarjoXR/Foundation/BackendType.hpp
    include/VarjoXR/Foundation/Eye.hpp
    include/VarjoXR/Foundation/PlacementMode.hpp
    include/VarjoXR/Foundation/Transform.hpp
    include/VarjoXR/Foundation/FrameContext.hpp
    include/VarjoXR/Core/XRTexture.hpp
    include/VarjoXR/Core/XRMaterial.hpp
    include/VarjoXR/Core/XRPlane.hpp
    include/VarjoXR/Core/XRSpace.hpp
    include/VarjoXR/Backends/IRenderBackend.hpp
)

set(VARJOXR_SOURCES
    src/Foundation/Transform.cpp
    src/Core/XRPlane.cpp
    src/Core/XRSpace.cpp
)

if(VARJOXR_ENABLE_D3D11)
    list(APPEND VARJOXR_PUBLIC_HEADERS
        include/VarjoXR/Backends/D3D11/D3D11Backend.hpp)
    list(APPEND VARJOXR_SOURCES
        src/Backends/D3D11/D3D11Backend.cpp)
endif()

if(VARJOXR_ENABLE_D3D12)
    list(APPEND VARJOXR_PUBLIC_HEADERS
        include/VarjoXR/Backends/D3D12/D3D12Backend.hpp)
    list(APPEND VARJOXR_SOURCES
        src/Backends/D3D12/D3D12Backend.cpp)
endif()

add_library(VarjoXR STATIC ${VARJOXR_SOURCES} ${VARJOXR_PUBLIC_HEADERS})
add_library(VarjoXR::VarjoXR ALIAS VarjoXR)

target_compile_features(VarjoXR PUBLIC cxx_std_17)

target_include_directories(VarjoXR
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_definitions(VarjoXR
    PUBLIC
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        VARJOXR_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
        VARJOXR_VERSION_MINOR=${PROJECT_VERSION_MINOR}
        VARJOXR_VERSION_PATCH=${PROJECT_VERSION_PATCH}
        VARJOXR_VERSION_STRING="${PROJECT_VERSION}"
)

target_link_libraries(VarjoXR
    PUBLIC
        VarjoToolkit::VarjoToolkit
        glm::glm
)

if(VARJOXR_ENABLE_D3D11)
    target_compile_definitions(VarjoXR PUBLIC VARJOXR_ENABLE_D3D11=1)
    target_link_libraries(VarjoXR PUBLIC D3D11Helper::D3D11Helper)
endif()

if(VARJOXR_ENABLE_D3D12)
    target_compile_definitions(VarjoXR PUBLIC VARJOXR_ENABLE_D3D12=1)
    target_link_libraries(VarjoXR PUBLIC D3D12Helper::D3D12Helper)
endif()

if(MSVC)
    target_compile_options(VarjoXR PRIVATE /utf-8 /EHsc)
    if(VARJOXR_ENABLE_WARNINGS)
        target_compile_options(VarjoXR PRIVATE /W4 /permissive-)
    endif()
endif()

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/include" PREFIX "Header" FILES ${VARJOXR_PUBLIC_HEADERS})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" PREFIX "Source" FILES ${VARJOXR_SOURCES})

if(VARJOXR_BUILD_SAMPLES)
    add_subdirectory(samples)
endif()

if(VARJOXR_BUILD_TESTS)
    include(CTest)
    enable_testing()
    add_subdirectory(tests)
endif()
