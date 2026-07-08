add_executable(VarjoXRCoreTests
    VarjoXRCoreTests.cpp)

target_link_libraries(VarjoXRCoreTests PRIVATE VarjoXR::VarjoXR)

target_compile_features(VarjoXRCoreTests PRIVATE cxx_std_17)

if(MSVC AND VARJOXR_ENABLE_WARNINGS)
    target_compile_options(VarjoXRCoreTests PRIVATE /utf-8 /W4 /permissive-)
endif()

varjoxr_copy_dxc_runtime(VarjoXRCoreTests)

add_test(NAME VarjoXRCoreTests COMMAND VarjoXRCoreTests)

add_executable(VarjoXRBackendSmokeTests
    VarjoXRBackendSmokeTests.cpp)

target_link_libraries(VarjoXRBackendSmokeTests PRIVATE VarjoXR::VarjoXR)

target_compile_features(VarjoXRBackendSmokeTests PRIVATE cxx_std_17)

if(MSVC AND VARJOXR_ENABLE_WARNINGS)
    target_compile_options(VarjoXRBackendSmokeTests PRIVATE /utf-8 /W4 /permissive-)
endif()

varjoxr_copy_dxc_runtime(VarjoXRBackendSmokeTests)

add_test(NAME VarjoXRBackendSmokeTests COMMAND VarjoXRBackendSmokeTests)

function(varjoxr_add_target_build_smoke_test target)
    if(TARGET ${target})
        add_test(
            NAME Build_${target}
            COMMAND ${CMAKE_COMMAND} --build "${CMAKE_BINARY_DIR}" --config $<CONFIG> --target ${target})
        set_tests_properties(Build_${target} PROPERTIES RUN_SERIAL TRUE)
    endif()
endfunction()

if(VARJOXR_BUILD_SAMPLES)
    varjoxr_add_target_build_smoke_test(RenderingPlane_D3D11)
    varjoxr_add_target_build_smoke_test(RenderingPlane_D3D12)

    varjoxr_add_target_build_smoke_test(RenderingPlane_01_SinglePlane_D3D11)
    varjoxr_add_target_build_smoke_test(RenderingPlane_02_HeadRelativePlane_D3D11)
    varjoxr_add_target_build_smoke_test(RenderingPlane_03_StereoPlane_D3D11)
    varjoxr_add_target_build_smoke_test(RenderingPlane_04_ShaderPlane_D3D11)
    varjoxr_add_target_build_smoke_test(RenderingPlane_05_MultiplePlanes_D3D11)
    varjoxr_add_target_build_smoke_test(RenderingPlane_06_ProcessingPlane_D3D11)

    varjoxr_add_target_build_smoke_test(RenderingPlane_01_SinglePlane_D3D12)
    varjoxr_add_target_build_smoke_test(RenderingPlane_02_HeadRelativePlane_D3D12)
    varjoxr_add_target_build_smoke_test(RenderingPlane_03_StereoPlane_D3D12)
    varjoxr_add_target_build_smoke_test(RenderingPlane_04_ShaderPlane_D3D12)
    varjoxr_add_target_build_smoke_test(RenderingPlane_05_MultiplePlanes_D3D12)
    varjoxr_add_target_build_smoke_test(RenderingPlane_06_ProcessingPlane_D3D12)

    varjoxr_add_target_build_smoke_test(ProgrammableProcessing_D3D11)
    varjoxr_add_target_build_smoke_test(ProgrammableProcessing_D3D12)
endif()
