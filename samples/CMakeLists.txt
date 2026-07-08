add_subdirectory(RenderingPlane)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ProgrammableProcessing/CMakeLists.txt")
    add_subdirectory(ProgrammableProcessing)
endif()
