# Helpers for locating/copying the DirectX Shader Compiler runtime.
# dxcompiler.dll is required at process startup when a target links against dxcompiler.lib.
# dxil.dll should usually be copied together with dxcompiler.dll.

set(VARJOXR_DXC_RUNTIME_DIR "" CACHE PATH "Directory that contains dxcompiler.dll and dxil.dll")
set(VARJOXR_DXC_NUGET_PACKAGE_ID "Microsoft.Direct3D.DXC" CACHE STRING "NuGet package ID used to download the DXC runtime when local search fails")
set(VARJOXR_DXC_NUGET_VERSION "" CACHE STRING "DXC NuGet package version. Empty means latest listed by NuGet flat container")
option(VARJOXR_COPY_DXC_RUNTIME "Copy dxcompiler.dll and dxil.dll next to D3D12 executables" ON)
option(VARJOXR_FETCH_DXC_RUNTIME "Download Microsoft.Direct3D.DXC with CMake when DXC runtime DLLs are not found locally" ON)

function(varjoxr_find_dxc_runtime_in_dirs out_dxcompiler out_dxil)
    set(_candidate_dirs ${ARGN})
    list(REMOVE_DUPLICATES _candidate_dirs)

    unset(_varjoxr_found_dxcompiler CACHE)
    unset(_varjoxr_found_dxil CACHE)

    find_file(_varjoxr_found_dxcompiler
        NAMES dxcompiler.dll
        PATHS ${_candidate_dirs}
        NO_DEFAULT_PATH)

    find_file(_varjoxr_found_dxil
        NAMES dxil.dll
        PATHS ${_candidate_dirs}
        NO_DEFAULT_PATH)

    set(${out_dxcompiler} "${_varjoxr_found_dxcompiler}" PARENT_SCOPE)
    set(${out_dxil} "${_varjoxr_found_dxil}" PARENT_SCOPE)
endfunction()

function(varjoxr_download_dxc_runtime out_dir)
    if(NOT VARJOXR_FETCH_DXC_RUNTIME)
        set(${out_dir} "" PARENT_SCOPE)
        return()
    endif()

    string(TOLOWER "${VARJOXR_DXC_NUGET_PACKAGE_ID}" _package_id_lower)
    set(_package_root "${CMAKE_BINARY_DIR}/packages/${VARJOXR_DXC_NUGET_PACKAGE_ID}")
    file(MAKE_DIRECTORY "${_package_root}")

    set(_version "${VARJOXR_DXC_NUGET_VERSION}")
    if(NOT _version)
        set(_index_file "${_package_root}/index.json")
        set(_index_url "https://api.nuget.org/v3-flatcontainer/${_package_id_lower}/index.json")
        message(STATUS "VarjoXR: downloading DXC NuGet version index: ${_index_url}")
        file(DOWNLOAD
            "${_index_url}"
            "${_index_file}"
            STATUS _download_status
            TLS_VERIFY ON)
        list(GET _download_status 0 _download_code)
        list(GET _download_status 1 _download_message)
        if(NOT _download_code EQUAL 0)
            message(WARNING "VarjoXR: failed to download DXC NuGet index: ${_download_message}")
            set(${out_dir} "" PARENT_SCOPE)
            return()
        endif()

        file(READ "${_index_file}" _index_json)
        string(JSON _version_count ERROR_VARIABLE _json_error LENGTH "${_index_json}" versions)
        if(_json_error OR _version_count LESS 1)
            message(WARNING "VarjoXR: failed to parse DXC NuGet version index: ${_json_error}")
            set(${out_dir} "" PARENT_SCOPE)
            return()
        endif()
        math(EXPR _last_version_index "${_version_count} - 1")
        string(JSON _version GET "${_index_json}" versions ${_last_version_index})
    endif()

    set(_package_dir "${_package_root}/${_version}")
    set(_package_file "${_package_root}/${_package_id_lower}.${_version}.nupkg")
    set(_package_url "https://api.nuget.org/v3-flatcontainer/${_package_id_lower}/${_version}/${_package_id_lower}.${_version}.nupkg")

    if(NOT EXISTS "${_package_dir}/.varjoxr_dxc_extracted")
        message(STATUS "VarjoXR: downloading DXC NuGet package ${VARJOXR_DXC_NUGET_PACKAGE_ID} ${_version}")
        file(DOWNLOAD
            "${_package_url}"
            "${_package_file}"
            STATUS _package_download_status
            TLS_VERIFY ON)
        list(GET _package_download_status 0 _package_download_code)
        list(GET _package_download_status 1 _package_download_message)
        if(NOT _package_download_code EQUAL 0)
            message(WARNING "VarjoXR: failed to download DXC NuGet package: ${_package_download_message}")
            set(${out_dir} "" PARENT_SCOPE)
            return()
        endif()

        file(REMOVE_RECURSE "${_package_dir}")
        file(MAKE_DIRECTORY "${_package_dir}")
        file(ARCHIVE_EXTRACT INPUT "${_package_file}" DESTINATION "${_package_dir}")
        file(WRITE "${_package_dir}/.varjoxr_dxc_extracted" "${_version}\n")
    endif()

    message(STATUS "VarjoXR: using DXC NuGet package directory: ${_package_dir}")
    set(${out_dir} "${_package_dir}" PARENT_SCOPE)
endfunction()

function(varjoxr_collect_dxc_candidate_dirs out_dirs)
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

    file(GLOB _local_package_dirs
        "${PROJECT_SOURCE_DIR}/packages/Microsoft.Direct3D.DXC*/bin/${_dxc_arch}"
        "${PROJECT_SOURCE_DIR}/packages/microsoft.direct3d.dxc*/bin/${_dxc_arch}"
        "${PROJECT_SOURCE_DIR}/packages/Microsoft.Direct3D.DXC*/build/native/bin/${_dxc_arch}"
        "${PROJECT_SOURCE_DIR}/packages/microsoft.direct3d.dxc*/build/native/bin/${_dxc_arch}"
        "${CMAKE_BINARY_DIR}/packages/Microsoft.Direct3D.DXC*/bin/${_dxc_arch}"
        "${CMAKE_BINARY_DIR}/packages/microsoft.direct3d.dxc*/bin/${_dxc_arch}"
        "${CMAKE_BINARY_DIR}/packages/Microsoft.Direct3D.DXC*/build/native/bin/${_dxc_arch}"
        "${CMAKE_BINARY_DIR}/packages/microsoft.direct3d.dxc*/build/native/bin/${_dxc_arch}"
        "${CMAKE_BINARY_DIR}/packages/Microsoft.Direct3D.DXC*/*/bin/${_dxc_arch}"
        "${CMAKE_BINARY_DIR}/packages/Microsoft.Direct3D.DXC*/*/build/native/bin/${_dxc_arch}"
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

    if(DEFINED ENV{PATH})
        file(TO_CMAKE_PATH "$ENV{PATH}" _env_path_dirs)
        list(APPEND _dxc_candidate_dirs ${_env_path_dirs})
    endif()

    list(REMOVE_DUPLICATES _dxc_candidate_dirs)
    set(${out_dirs} ${_dxc_candidate_dirs} PARENT_SCOPE)
endfunction()

function(varjoxr_find_dxc_runtime)
    if(DEFINED VARJOXR_DXCOMPILER_DLL AND EXISTS "${VARJOXR_DXCOMPILER_DLL}")
        if(DEFINED VARJOXR_DXIL_DLL AND EXISTS "${VARJOXR_DXIL_DLL}")
            return()
        endif()
    endif()

    varjoxr_collect_dxc_candidate_dirs(_dxc_candidate_dirs)
    varjoxr_find_dxc_runtime_in_dirs(_dxcompiler _dxil ${_dxc_candidate_dirs})

    if(NOT _dxcompiler)
        varjoxr_download_dxc_runtime(_downloaded_dxc_dir)
        if(_downloaded_dxc_dir)
            if(CMAKE_SIZEOF_VOID_P EQUAL 8)
                set(_dxc_arch "x64")
            else()
                set(_dxc_arch "x86")
            endif()
            file(GLOB _downloaded_dxc_candidate_dirs
                "${_downloaded_dxc_dir}/bin/${_dxc_arch}"
                "${_downloaded_dxc_dir}/build/native/bin/${_dxc_arch}"
                "${_downloaded_dxc_dir}/*/bin/${_dxc_arch}"
                "${_downloaded_dxc_dir}/*/build/native/bin/${_dxc_arch}"
            )
            list(APPEND _dxc_candidate_dirs ${_downloaded_dxc_candidate_dirs})
            varjoxr_find_dxc_runtime_in_dirs(_dxcompiler _dxil ${_dxc_candidate_dirs})
        endif()
    endif()

    if(_dxcompiler)
        set(VARJOXR_DXCOMPILER_DLL "${_dxcompiler}" CACHE FILEPATH "Path to dxcompiler.dll" FORCE)
    endif()
    if(_dxil)
        set(VARJOXR_DXIL_DLL "${_dxil}" CACHE FILEPATH "Path to dxil.dll" FORCE)
    endif()

    if(VARJOXR_DXCOMPILER_DLL)
        message(STATUS "VarjoXR: found dxcompiler.dll: ${VARJOXR_DXCOMPILER_DLL}")
        if(VARJOXR_DXIL_DLL)
            message(STATUS "VarjoXR: found dxil.dll: ${VARJOXR_DXIL_DLL}")
        else()
            message(WARNING "VarjoXR: dxil.dll was not found. Some DXC flows may fail at runtime.")
        endif()
    else()
        message(WARNING
            "VarjoXR: dxcompiler.dll was not found locally and could not be downloaded. "
            "Pass -DVARJOXR_DXC_RUNTIME_DIR=<dir containing dxcompiler.dll>, "
            "or set -DVARJOXR_FETCH_DXC_RUNTIME=ON on a machine with internet access.")
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
