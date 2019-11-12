cmake_minimum_required(VERSION 3.5.0)
project(libawr)

set (CMAKE_BUILD_TYPE Release)
set (CMAKE_CXX_STANDARD 11)
set (CMAKE_C_STANDARD 11)
set (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

set( CMAKE_VERBOSE_MAKEFILE on )


###########
# Library #
###########
file(GLOB SOURCES "awr.c")
option(CRAY "Cray compilation" ON)

if(CRAY)
	SET_PROPERTY(GLOBAL PROPERTY TARGET_SUPPORTS_SHARED_LIBS TRUE)
	set(PAPI_LIBRARIES_DYNAMIC)
	set(PAPI_LIBRARIES_STATIC)
	set(MPI_C_LIBRARIES)
	set(MPI_C_LINK_FLAGS)
	set(MPI_Fortran_LIBRARIES)
	set(MPI_Fortran_LINK_FLAGS)
else()
	find_package(MPI)
	find_package(PAPI)
	include_directories(SYSTEM ${MPI_INCLUDE_PATH})
	SET(CMAKE_CXX_COMPILER ${MPI_CXX_COMPILER})
	SET(CMAKE_C_COMPILER ${MPI_C_COMPILER})
endif(CRAY)


add_library(awr SHARED ${SOURCES})
add_library(awr_static STATIC ${SOURCES})

if(CRAY)
	target_compile_options(awr PUBLIC -Wall -dynamic -O3
	$ENV{CRAY_GNI_HEADERS_INCLUDE_OPTS})
        target_compile_options(awr_static PUBLIC -Wall -dynamic -O3
	$ENV{CRAY_GNI_HEADERS_INCLUDE_OPTS})
	set(DMAPP_INCS)
	string(REPLACE " " ";" DMAPP_INCS $ENV{CRAY_DMAPP_INCLUDE_OPTS})
	foreach(DINC ${DMAPP_INCS})
		     target_compile_options(awr PUBLIC ${DINC})
                     target_compile_options(awr_static PUBLIC ${DINC})
	endforeach()
endif(CRAY)


