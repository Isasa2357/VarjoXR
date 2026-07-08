set(VARJOXR_PROGRAMMABLE_PROCESSING_HLSL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Shaders")
set(VARJOXR_HLSL_DIR "${PROJECT_SOURCE_DIR}/hlsl")
set(VARJOXR_PROGRAMMABLE_PROCESSING_HLSL
    Shaders/CircleDarkenPreprocess.hlsl)
set_source_files_properties(${VARJOXR_PROGRAMMABLE_PROCESSING_HLSL} PROPERTIES HEADER_FILE_ONLY TRUE)

if(VARJOXR_ENABLE_D3D11)
    add_executable(ProgrammableProcessing_D3D11
        ProgrammableProcessing_D3D11.cpp
        ProgrammableProcessingSampleCommon.hpp
        ${VARJOXR_PROGRAMMABLE_PROCESSING_HLSL})
    target_link_libraries(ProgrammableProcessing_D3D11 PRIVATE VarjoXR::VarjoXR)
    target_compile_features(ProgrammableProcessing_D3D11 PRIVATE cxx_std_17)
    target_compile_definitions(ProgrammableProcessing_D3D11 PRIVATE
        VARJOXR_PROGRAMMABLE_PROCESSING_HLSL_DIR="${VARJOXR_PROGRAMMABLE_PROCESSING_HLSL_DIR}"
        VARJOXR_HLSL_DIR="${VARJOXR_HLSL_DIR}")
    if(MSVC)
        target_compile_options(ProgrammableProcessing_D3D11 PRIVATE /utf-8 /EHsc)
    endif()
endif()

if(VARJOXR_ENABLE_D3D12)
    add_executable(ProgrammableProcessing_D3D12
        ProgrammableProcessing_D3D12.cpp
        ProgrammableProcessingSampleCommon.hpp
        ${VARJOXR_PROGRAMMABLE_PROCESSING_HLSL})
    target_link_libraries(ProgrammableProcessing_D3D12 PRIVATE VarjoXR::VarjoXR)
    target_compile_features(ProgrammableProcessing_D3D12 PRIVATE cxx_std_17)
    target_compile_definitions(ProgrammableProcessing_D3D12 PRIVATE
        VARJOXR_PROGRAMMABLE_PROCESSING_HLSL_DIR="${VARJOXR_PROGRAMMABLE_PROCESSING_HLSL_DIR}"
        VARJOXR_HLSL_DIR="${VARJOXR_HLSL_DIR}")
    if(MSVC)
        target_compile_options(ProgrammableProcessing_D3D12 PRIVATE /utf-8 /EHsc)
    endif()
endif()
