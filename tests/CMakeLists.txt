add_executable(VarjoXRCoreTests
    VarjoXRCoreTests.cpp)

target_link_libraries(VarjoXRCoreTests PRIVATE VarjoXR::Core)

target_compile_features(VarjoXRCoreTests PRIVATE cxx_std_17)

if(MSVC AND VARJOXR_ENABLE_WARNINGS)
    target_compile_options(VarjoXRCoreTests PRIVATE /W4 /permissive-)
endif()

if(VARJOXR_ENABLE_COVERAGE AND NOT MSVC)
    target_compile_options(VarjoXRCoreTests PRIVATE -O0 -g --coverage)
    target_link_options(VarjoXRCoreTests PRIVATE --coverage)
endif()

add_test(NAME VarjoXRCoreTests COMMAND VarjoXRCoreTests)

if(VARJOXR_ENABLE_COVERAGE AND MSVC)
    find_program(OPENCPPCOVERAGE_EXE OpenCppCoverage)
    if(OPENCPPCOVERAGE_EXE)
        add_custom_target(VarjoXRCoverage
            COMMAND "${OPENCPPCOVERAGE_EXE}"
                --sources "${PROJECT_SOURCE_DIR}/include/VarjoXR"
                --sources "${PROJECT_SOURCE_DIR}/src/Transform.cpp"
                --sources "${PROJECT_SOURCE_DIR}/src/XRObject.cpp"
                --sources "${PROJECT_SOURCE_DIR}/src/XRPlane.cpp"
                --excluded_sources "${PROJECT_SOURCE_DIR}/src/Backends"
                --excluded_sources "${PROJECT_SOURCE_DIR}/src/Detail/VarjoLayerSubmit.cpp"
                --excluded_sources "${PROJECT_SOURCE_DIR}/src/VarjoSession.cpp"
                --excluded_sources "${PROJECT_SOURCE_DIR}/src/XRSpace.cpp"
                --export_type html:"${CMAKE_BINARY_DIR}/coverage/html"
                --export_type cobertura:"${CMAKE_BINARY_DIR}/coverage/VarjoXRCoreCoverage.xml"
                -- "$<TARGET_FILE:VarjoXRCoreTests>"
            DEPENDS VarjoXRCoreTests
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
            COMMENT "Running OpenCppCoverage for VarjoXR core tests")
    else()
        message(WARNING "VARJOXR_ENABLE_COVERAGE=ON but OpenCppCoverage was not found in PATH. Coverage target will not be generated.")
    endif()
endif()
