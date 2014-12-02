if(APPLE)
	SET( GCC_COVERAGE_LINK_FLAGS    "-framework JavaVM" )
endif()

if(UNIX)
	SET( GCC_COVERAGE_LINK_FLAGS    "-ljvm" )
endif()

SET( CMAKE_ASM_FLAGS "-g" )
SET( CMAKE_C_FLAGS "-g -O3 -Wextra -Wall -Werror -fno-strict-aliasing" )
SET( CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} ${GCC_COVERAGE_LINK_FLAGS}" )

add_library( javavmloader javavmloader.cpp javavmloader.h )

add_library( b bprovider.cpp bprovider.h javavmloader.cpp javavmloader.h )

install( TARGETS b javavmloader DESTINATION bin )
install( FILES probwrapperconfig.h.in ltsmin.h bprovider.h javavmloader.h DESTINATION include )
