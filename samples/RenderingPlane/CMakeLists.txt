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
