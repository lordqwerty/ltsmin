if(APPLE)
	SET(GCC_COVERAGE_LINK_FLAGS    "-framework JavaVM")
endif()

if(UNIX)
	SET(GCC_COVERAGE_LINK_FLAGS    "-ljvm")
endif()

SET( CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} ${GCC_COVERAGE_LINK_FLAGS}" )

include_directories (${PROJECT_SOURCE_DIR}/src)

add_executable (Test test.cpp)
target_link_libraries (Test b javavmloader)
