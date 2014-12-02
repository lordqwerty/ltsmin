cmake_minimum_required (VERSION 2.6)
project (probwrapper)

include (CTest)

find_package(Java REQUIRED)
find_package(JNI REQUIRED)

# OS X 10.10 Work around
if(APPLE)
	include_directories("$ENV{JAVA_HOME}/include")
endif()

# Linux builders
if(UNIX)
	include_directories("$ENV{JAVA_HOME}/include")
	include_directories("$ENV{JAVA_HOME}/include/linux")
endif()

include(UseJava)

# The version number.
set (ProBWrapper_VERSION_MAJOR 1)
set (ProBWrapper_VERSION_MINOR 0)

add_subdirectory (src)
add_subdirectory (test)

configure_file (
	"${PROJECT_SOURCE_DIR}/src/probwrapperconfig.h.in"
	"${PROJECT_BINARY_DIR}/src/probwrapperconfig.h"
	)

enable_testing ()

IF(PROJECT_OS_LINUX)
	OPTION(BUILD_CPACK "Build an RPM or DEB using CPack" OFF)
ENDIF(PROJECT_OS_LINUX)

# LTSmin is not Windows compatible
IF(NOT UNIX)
	MESSAGE(FATAL_ERROR "ProB Wrapper is not supported on this platform")
ENDIF()

# Install targets
# install (TARGETS ProBWrapper DESTINATION bin)
set (CMAKE_INSTALL_PREFIX .)

install (FILES "${PROJECT_BINARY_DIR}/src/probwrapperconfig.h"        
			DESTINATION include)

# build a CPack driven installer package
include (InstallRequiredSystemLibraries)
set (CPACK_RESOURCE_FILE_LICENSE  
	     "${CMAKE_CURRENT_SOURCE_DIR}/License.txt")
set (CPACK_PACKAGE_VERSION_MAJOR "${ProBWrapper_VERSION_MAJOR}")
set (CPACK_PACKAGE_VERSION_MINOR "${ProBWrapper_VERSION_MINOR}")
include (CPack)
