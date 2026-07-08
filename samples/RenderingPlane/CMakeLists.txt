function(varjoxr_configure_rendering_plane_sample target backend_define)
    target_link_libraries(${target} PRIVATE VarjoXR::VarjoXR)
    target_compile_features(${target} PRIVATE cxx_std_17)
    target_compile_definitions(${target} PRIVATE ${backend_define})
    if(MSVC)
        target_compile_options(${target} PRIVATE /utf-8 /EHsc)
    endif()
endfunction()

function(varjoxr_add_rendering_plane_sample sample_id sample_name source_file)
    if(VARJOXR_ENABLE_D3D11)
        set(target RenderingPlane_${sample_id}_${sample_name}_D3D11)
        add_executable(${target}
            ${source_file}
            Common/RenderingPlaneSampleCommon.hpp)
        varjoxr_configure_rendering_plane_sample(${target} RENDERING_PLANE_SAMPLE_D3D11)
    endif()

    if(VARJOXR_ENABLE_D3D12)
        set(target RenderingPlane_${sample_id}_${sample_name}_D3D12)
        add_executable(${target}
            ${source_file}
            Common/RenderingPlaneSampleCommon.hpp)
        varjoxr_configure_rendering_plane_sample(${target} RENDERING_PLANE_SAMPLE_D3D12)
    endif()
endfunction()

# Legacy minimal samples kept for compatibility.
if(VARJOXR_ENABLE_D3D11)
    add_executable(RenderingPlane_D3D11
        RenderingPlane_D3D11.cpp)
    target_link_libraries(RenderingPlane_D3D11 PRIVATE VarjoXR::VarjoXR)
    target_compile_features(RenderingPlane_D3D11 PRIVATE cxx_std_17)
    if(MSVC)
        target_compile_options(RenderingPlane_D3D11 PRIVATE /utf-8 /EHsc)
    endif()
endif()

if(VARJOXR_ENABLE_D3D12)
    add_executable(RenderingPlane_D3D12
        RenderingPlane_D3D12.cpp)
    target_link_libraries(RenderingPlane_D3D12 PRIVATE VarjoXR::VarjoXR)
    target_compile_features(RenderingPlane_D3D12 PRIVATE cxx_std_17)
    if(MSVC)
        target_compile_options(RenderingPlane_D3D12 PRIVATE /utf-8 /EHsc)
    endif()
endif()

varjoxr_add_rendering_plane_sample(01 SinglePlane 01_SinglePlane/SinglePlane.cpp)
varjoxr_add_rendering_plane_sample(02 HeadRelativePlane 02_HeadRelativePlane/HeadRelativePlane.cpp)
varjoxr_add_rendering_plane_sample(03 StereoPlane 03_StereoPlane/StereoPlane.cpp)
varjoxr_add_rendering_plane_sample(04 ShaderPlane 04_ShaderPlane/ShaderPlane.cpp)
varjoxr_add_rendering_plane_sample(05 MultiplePlanes 05_MultiplePlanes/MultiplePlanes.cpp)
varjoxr_add_rendering_plane_sample(06 ProcessingPlane 06_ProcessingPlane/ProcessingPlane.cpp)
