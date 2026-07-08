# Helpers for locating/copying the Varjo runtime DLL.
# VarjoLib.dll must be next to executables or reachable through PATH at runtime.

set(VARJOXR_VARJO_RUNTIME_DIR "" CACHE PATH "Directory that contains VarjoLib.dll")
option(VARJOXR_COPY_VARJO_RUNTIME "Copy VarjoLib.dll next to VarjoXR executables" ON)

function(varjoxr_collect_varjo_runtime_candidate_dirs out_dirs)
    set(_candidate_dirs)

    if(VARJOXR_VARJO_RUNTIME_DIR)
        file(TO_CMAKE_PATH "${VARJOXR_VARJO_RUNTIME_DIR}" _varjoxr_varjo_runtime_dir)
        list(APPEND _candidate_dirs "${_varjoxr_varjo_runtime_dir}")
    endif()

    if(VARJO_SDK_ROOT)
        file(TO_CMAKE_PATH "${VARJO_SDK_ROOT}" _varjo_sdk_root)
        list(APPEND _candidate_dirs
            "${_varjo_sdk_root}/bin"
            "${_varjo_sdk_root}/bin/x64"
            "${_varjo_sdk_root}/bin/win64"
            "${_varjo_sdk_root}/Bin"
            "${_varjo_sdk_root}/Bin/x64"
            "${_varjo_sdk_root}/Bin/win64")
    endif()

    if(DEFINED ENV{VARJO_SDK_ROOT})
        file(TO_CMAKE_PATH "$ENV{VARJO_SDK_ROOT}" _env_varjo_sdk_root)
        list(APPEND _candidate_dirs
            "${_env_varjo_sdk_root}/bin"
            "${_env_varjo_sdk_root}/bin/x64"
            "${_env_varjo_sdk_root}/bin/win64"
            "${_env_varjo_sdk_root}/Bin"
            "${_env_varjo_sdk_root}/Bin/x64"
            "${_env_varjo_sdk_root}/Bin/win64")
    endif()

    if(VARJO_LIBRARY)
        get_filename_component(_varjo_library_dir "${VARJO_LIBRARY}" DIRECTORY)
        get_filename_component(_varjo_library_parent "${_varjo_library_dir}" DIRECTORY)
        list(APPEND _candidate_dirs
            "${_varjo_library_dir}"
            "${_varjo_library_parent}/bin"
            "${_varjo_library_parent}/bin/x64"
            "${_varjo_library_parent}/bin/win64"
            "${_varjo_library_parent}/Bin"
            "${_varjo_library_parent}/Bin/x64"
            "${_varjo_library_parent}/Bin/win64")
    endif()

    if(DEFINED ENV{PATH})
        file(TO_CMAKE_PATH "$ENV{PATH}" _env_path_dirs)
        list(APPEND _candidate_dirs ${_env_path_dirs})
    endif()

    list(REMOVE_DUPLICATES _candidate_dirs)
    set(${out_dirs} ${_candidate_dirs} PARENT_SCOPE)
endfunction()

function(varjoxr_find_varjo_runtime)
    if(DEFINED VARJOXR_VARJOLIB_DLL AND EXISTS "${VARJOXR_VARJOLIB_DLL}")
        return()
    endif()

    varjoxr_collect_varjo_runtime_candidate_dirs(_candidate_dirs)
    unset(_varjoxr_found_varjolib CACHE)
    find_file(_varjoxr_found_varjolib
        NAMES VarjoLib.dll varjo.dll
        PATHS ${_candidate_dirs}
        NO_DEFAULT_PATH)

    if(_varjoxr_found_varjolib)
        set(VARJOXR_VARJOLIB_DLL "${_varjoxr_found_varjolib}" CACHE FILEPATH "Path to VarjoLib.dll" FORCE)
        message(STATUS "VarjoXR: found Varjo runtime DLL: ${VARJOXR_VARJOLIB_DLL}")
    else()
        message(WARNING
            "VarjoXR: VarjoLib.dll was not found. Executables may fail at runtime. "
            "Pass -DVARJOXR_VARJO_RUNTIME_DIR=<dir containing VarjoLib.dll> or set VARJO_SDK_ROOT.")
    endif()
endfunction()

function(varjoxr_copy_varjo_runtime target_name)
    if(NOT VARJOXR_COPY_VARJO_RUNTIME)
        return()
    endif()
    if(NOT TARGET ${target_name})
        return()
    endif()

    varjoxr_find_varjo_runtime()

    if(VARJOXR_VARJOLIB_DLL)
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${VARJOXR_VARJOLIB_DLL}"
                    "$<TARGET_FILE_DIR:${target_name}>"
            VERBATIM)
    endif()
endfunction()
