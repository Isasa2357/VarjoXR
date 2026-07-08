# Helpers for locating/copying the DirectX Shader Compiler runtime.
# dxcompiler.dll is required at process startup when a target links against dxcompiler.lib.
# dxil.dll should usually be copied together with dxcompiler.dll.

set(VARJOXR_DXC_RUNTIME_DIR "" CACHE PATH "Directory that contains dxcompiler.dll and dxil.dll")
option(VARJOXR_COPY_DXC_RUNTIME "Copy dxcompiler.dll and dxil.dll next to D3D12 executables" ON)

function(varjoxr_find_dxc_runtime)
    if(DEFINED VARJOXR_DXCOMPILER_DLL AND EXISTS "${VARJOXR_DXCOMPILER_DLL}")
        return()
    endif()

    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(_dxc_arch "x64")
    else()
        set(_dxc_arch "x86")
    endif()

    set(_dxc_candidate_dirs)

    if(VARJOXR_DXC_RUNTIME_DIR)
        file(TO_CMAKE_PATH "${VARJOXR_DXC_RUNTIME_DIR}" _varjoxr_dxc_runtime_dir)
        list(APPEND _dxc_candidate_dirs "${_varjoxr_dxc_runtime_dir}")
    endif()

    if(D3D12HELPER_DXC_RUNTIME_DIR)
        file(TO_CMAKE_PATH "${D3D12HELPER_DXC_RUNTIME_DIR}" _d3d12helper_dxc_runtime_dir)
        list(APPEND _dxc_candidate_dirs "${_d3d12helper_dxc_runtime_dir}")
    endif()

    if(DEFINED D3D12HELPER_DXCOMPILER_DLL AND EXISTS "${D3D12HELPER_DXCOMPILER_DLL}")
        get_filename_component(_d3d12helper_dxc_dir "${D3D12HELPER_DXCOMPILER_DLL}" DIRECTORY)
        list(APPEND _dxc_candidate_dirs "${_d3d12helper_dxc_dir}")
    endif()

    # Local NuGet restore locations.
    file(GLOB _local_package_dirs
        "${PROJECT_SOURCE_DIR}/packages/Microsoft.Direct3D.DXC*/bin/${_dxc_arch}"
        "${PROJECT_SOURCE_DIR}/packages/microsoft.direct3d.dxc*/bin/${_dxc_arch}"
        "${PROJECT_SOURCE_DIR}/packages/Microsoft.Direct3D.DXC*/build/native/bin/${_dxc_arch}"
        "${PROJECT_SOURCE_DIR}/packages/microsoft.direct3d.dxc*/build/native/bin/${_dxc_arch}"
        "${CMAKE_BINARY_DIR}/packages/Microsoft.Direct3D.DXC*/bin/${_dxc_arch}"
        "${CMAKE_BINARY_DIR}/packages/microsoft.direct3d.dxc*/bin/${_dxc_arch}"
        "${CMAKE_BINARY_DIR}/packages/Microsoft.Direct3D.DXC*/build/native/bin/${_dxc_arch}"
        "${CMAKE_BINARY_DIR}/packages/microsoft.direct3d.dxc*/build/native/bin/${_dxc_arch}"
    )
    list(APPEND _dxc_candidate_dirs ${_local_package_dirs})

    if(DEFINED ENV{USERPROFILE})
        file(TO_CMAKE_PATH "$ENV{USERPROFILE}" _user_profile)
        file(GLOB _global_package_dirs
            "${_user_profile}/.nuget/packages/microsoft.direct3d.dxc/*/bin/${_dxc_arch}"
            "${_user_profile}/.nuget/packages/microsoft.direct3d.dxc/*/build/native/bin/${_dxc_arch}"
        )
        list(APPEND _dxc_candidate_dirs ${_global_package_dirs})
    endif()

    # Windows SDK locations. These often contain dxc.exe/dxcompiler.dll/dxil.dll.
    if(DEFINED ENV{WindowsSdkDir})
        file(TO_CMAKE_PATH "$ENV{WindowsSdkDir}" _windows_sdk_dir)
        if(DEFINED ENV{WindowsSDKVersion})
            file(TO_CMAKE_PATH "$ENV{WindowsSDKVersion}" _windows_sdk_version)
            list(APPEND _dxc_candidate_dirs
                "${_windows_sdk_dir}/bin/${_windows_sdk_version}/${_dxc_arch}")
        endif()
        file(GLOB _windows_sdk_versioned_dirs
            "${_windows_sdk_dir}/bin/*/${_dxc_arch}"
            "${_windows_sdk_dir}/bin/${_dxc_arch}"
        )
        list(APPEND _dxc_candidate_dirs ${_windows_sdk_versioned_dirs})
    endif()

    file(GLOB _default_windows_sdk_dirs
        "C:/Program Files (x86)/Windows Kits/10/bin/*/${_dxc_arch}"
        "C:/Program Files (x86)/Windows Kits/10/bin/${_dxc_arch}"
    )
    list(APPEND _dxc_candidate_dirs ${_default_windows_sdk_dirs})

    list(REMOVE_DUPLICATES _dxc_candidate_dirs)

    find_file(VARJOXR_DXCOMPILER_DLL
        NAMES dxcompiler.dll
        PATHS ${_dxc_candidate_dirs}
        NO_DEFAULT_PATH)

    find_file(VARJOXR_DXIL_DLL
        NAMES dxil.dll
        PATHS ${_dxc_candidate_dirs}
        NO_DEFAULT_PATH)

    if(VARJOXR_DXCOMPILER_DLL)
        message(STATUS "VarjoXR: found dxcompiler.dll: ${VARJOXR_DXCOMPILER_DLL}")
        if(VARJOXR_DXIL_DLL)
            message(STATUS "VarjoXR: found dxil.dll: ${VARJOXR_DXIL_DLL}")
        else()
            message(WARNING "VarjoXR: dxil.dll was not found. Some DXC flows may fail at runtime.")
        endif()
    else()
        message(WARNING
            "VarjoXR: dxcompiler.dll was not found. D3D12 executables may fail at runtime. "
            "Install/restore Microsoft.Direct3D.DXC, install a Windows SDK with DXC, or pass "
            "-DVARJOXR_DXC_RUNTIME_DIR=<dir containing dxcompiler.dll>.")
    endif()
endfunction()

function(varjoxr_copy_dxc_runtime target_name)
    if(NOT VARJOXR_COPY_DXC_RUNTIME)
        return()
    endif()
    if(NOT VARJOXR_ENABLE_D3D12)
        return()
    endif()
    if(NOT TARGET ${target_name})
        return()
    endif()

    varjoxr_find_dxc_runtime()

    if(VARJOXR_DXCOMPILER_DLL)
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${VARJOXR_DXCOMPILER_DLL}"
                    "$<TARGET_FILE_DIR:${target_name}>"
            VERBATIM)
    endif()

    if(VARJOXR_DXIL_DLL)
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${VARJOXR_DXIL_DLL}"
                    "$<TARGET_FILE_DIR:${target_name}>"
            VERBATIM)
    endif()
endfunction()
