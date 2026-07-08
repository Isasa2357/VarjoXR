include(FetchContent)

option(VARJOXR_FETCH_DEPENDENCIES "Automatically fetch non-Varjo-SDK dependencies" ON)

set(VARJOXR_VARJO_SDK_ROOT "" CACHE PATH "Path to the Varjo Native SDK root directory")
set(VARJOXR_VARJO_INCLUDE_DIR "" CACHE PATH "Varjo Native SDK include directory")
set(VARJOXR_VARJO_LIBRARY "" CACHE FILEPATH "Path to VarjoLib.lib")

set(VARJOXR_VARJOTOOLKIT_GIT_REPOSITORY "https://github.com/Isasa2357/VarjoToolkit.git" CACHE STRING "VarjoToolkit git repository")
set(VARJOXR_VARJOTOOLKIT_GIT_TAG "90fd18c941b32440e5e0f4b067c262593f99c3a0" CACHE STRING "Pinned VarjoToolkit git commit/tag")

set(VARJOXR_D3D11HELPER_GIT_REPOSITORY "https://github.com/Isasa2357/D3D11Helper.git" CACHE STRING "D3D11Helper git repository")
set(VARJOXR_D3D11HELPER_GIT_TAG "fdb76fd36668bff6a094645897939c11b93369b0" CACHE STRING "Pinned D3D11Helper git commit/tag")

set(VARJOXR_D3D12HELPER_GIT_REPOSITORY "https://github.com/Isasa2357/D3D12Helper.git" CACHE STRING "D3D12Helper git repository")
set(VARJOXR_D3D12HELPER_GIT_TAG "af136edb5985af06dbce1a57ffa50b8bc9f167de" CACHE STRING "Pinned D3D12Helper git commit/tag")

set(VARJOXR_GLM_GIT_REPOSITORY "https://github.com/g-truc/glm.git" CACHE STRING "glm git repository")
set(VARJOXR_GLM_GIT_TAG "1.0.3" CACHE STRING "Pinned glm git tag")

set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "Do not update already-populated FetchContent dependencies automatically" FORCE)

function(varjoxr_forward_varjo_sdk_cache_variables)
    if(VARJOXR_VARJO_SDK_ROOT)
        set(VARJO_SDK_ROOT "${VARJOXR_VARJO_SDK_ROOT}" CACHE PATH "Path to the Varjo Native SDK root directory" FORCE)
    endif()
    if(VARJOXR_VARJO_INCLUDE_DIR)
        set(VARJO_INCLUDE_DIR "${VARJOXR_VARJO_INCLUDE_DIR}" CACHE PATH "Varjo Native SDK include directory" FORCE)
    endif()
    if(VARJOXR_VARJO_LIBRARY)
        set(VARJO_LIBRARY "${VARJOXR_VARJO_LIBRARY}" CACHE FILEPATH "Path to VarjoLib.lib" FORCE)
    endif()
endfunction()

function(varjoxr_require_target target_name package_hint)
    if(NOT TARGET ${target_name})
        message(FATAL_ERROR
            "Required dependency target '${target_name}' was not found. "
            "${package_hint} "
            "If this machine has no internet access, add the dependency with add_subdirectory() or install its CMake package. "
            "Varjo Native SDK is never auto-fetched; set VARJOXR_VARJO_SDK_ROOT or VARJOXR_VARJO_INCLUDE_DIR / VARJOXR_VARJO_LIBRARY.")
    endif()
endfunction()

function(varjoxr_fetch_varjotoolkit)
    varjoxr_forward_varjo_sdk_cache_variables()

    set(VARJOTOOLKIT_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
    set(VARJOTOOLKIT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(VARJOTOOLKIT_BUILD_HMD_TESTS OFF CACHE BOOL "" FORCE)
    set(VARJOTOOLKIT_ENABLE_EXPERIMENTAL_MR_POSTPROCESS OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        VarjoToolkit
        GIT_REPOSITORY ${VARJOXR_VARJOTOOLKIT_GIT_REPOSITORY}
        GIT_TAG        ${VARJOXR_VARJOTOOLKIT_GIT_TAG}
        GIT_SHALLOW    FALSE)
    FetchContent_MakeAvailable(VarjoToolkit)
endfunction()

function(varjoxr_fetch_d3d11helper)
    set(D3D11HELPER_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
    set(D3D11HELPER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(D3D11HELPER_INSTALL OFF CACHE BOOL "" FORCE)
    set(D3D11HELPER_ENABLE_PACKAGE_SMOKE_TESTS OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        D3D11Helper
        GIT_REPOSITORY ${VARJOXR_D3D11HELPER_GIT_REPOSITORY}
        GIT_TAG        ${VARJOXR_D3D11HELPER_GIT_TAG}
        GIT_SHALLOW    FALSE)
    FetchContent_MakeAvailable(D3D11Helper)
endfunction()

function(varjoxr_fetch_d3d12helper)
    set(D3D12HELPER_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
    set(D3D12HELPER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(D3D12HELPER_INSTALL OFF CACHE BOOL "" FORCE)
    set(D3D12HELPER_ENABLE_PACKAGE_SMOKE_TESTS OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        D3D12Helper
        GIT_REPOSITORY ${VARJOXR_D3D12HELPER_GIT_REPOSITORY}
        GIT_TAG        ${VARJOXR_D3D12HELPER_GIT_TAG}
        GIT_SHALLOW    FALSE)
    FetchContent_MakeAvailable(D3D12Helper)
endfunction()

function(varjoxr_fetch_glm)
    set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLM_BUILD_LIBRARY OFF CACHE BOOL "" FORCE)
    set(GLM_ENABLE_CXX_17 ON CACHE BOOL "" FORCE)

    FetchContent_Declare(
        glm
        GIT_REPOSITORY ${VARJOXR_GLM_GIT_REPOSITORY}
        GIT_TAG        ${VARJOXR_GLM_GIT_TAG}
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(glm)
endfunction()

function(varjoxr_resolve_dependencies)
    varjoxr_forward_varjo_sdk_cache_variables()

    if(NOT TARGET VarjoToolkit::VarjoToolkit)
        find_package(VarjoToolkit 0.2.0 EXACT CONFIG QUIET)
    endif()
    if(NOT TARGET VarjoToolkit::VarjoToolkit AND VARJOXR_FETCH_DEPENDENCIES)
        varjoxr_fetch_varjotoolkit()
    endif()
    varjoxr_require_target(VarjoToolkit::VarjoToolkit "Expected VarjoToolkit 0.2.0 or the pinned FetchContent commit ${VARJOXR_VARJOTOOLKIT_GIT_TAG}.")

    if(NOT TARGET glm::glm)
        find_package(glm 1.0.3 EXACT CONFIG QUIET)
    endif()
    if(NOT TARGET glm::glm AND VARJOXR_FETCH_DEPENDENCIES)
        varjoxr_fetch_glm()
    endif()
    varjoxr_require_target(glm::glm "Expected glm 1.0.3 or the pinned FetchContent tag ${VARJOXR_GLM_GIT_TAG}.")

    if(VARJOXR_ENABLE_D3D11)
        if(NOT TARGET D3D11Helper::D3D11Helper)
            find_package(D3D11Helper CONFIG QUIET)
        endif()
        if(NOT TARGET D3D11Helper::D3D11Helper AND VARJOXR_FETCH_DEPENDENCIES)
            varjoxr_fetch_d3d11helper()
        endif()
        varjoxr_require_target(D3D11Helper::D3D11Helper "Expected D3D11Helper pinned commit ${VARJOXR_D3D11HELPER_GIT_TAG}.")
    endif()

    if(VARJOXR_ENABLE_D3D12)
        if(NOT TARGET D3D12Helper::D3D12Helper)
            find_package(D3D12Helper CONFIG QUIET)
        endif()
        if(NOT TARGET D3D12Helper::D3D12Helper AND VARJOXR_FETCH_DEPENDENCIES)
            varjoxr_fetch_d3d12helper()
        endif()
        varjoxr_require_target(D3D12Helper::D3D12Helper "Expected D3D12Helper pinned commit ${VARJOXR_D3D12HELPER_GIT_TAG}.")
    endif()
endfunction()
