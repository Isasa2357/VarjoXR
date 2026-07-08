add_executable(VarjoXRCoreTests
    VarjoXRCoreTests.cpp)

target_link_libraries(VarjoXRCoreTests PRIVATE VarjoXR::VarjoXR)

target_compile_features(VarjoXRCoreTests PRIVATE cxx_std_17)

if(MSVC AND VARJOXR_ENABLE_WARNINGS)
    target_compile_options(VarjoXRCoreTests PRIVATE /utf-8 /W4 /permissive-)
endif()

add_test(NAME VarjoXRCoreTests COMMAND VarjoXRCoreTests)
