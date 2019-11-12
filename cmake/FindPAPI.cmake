# Try to find PAPI headers and libraries.
#
# Usage of this module as follows:
#
#     find_package(PAPI)
#
# Variables used by this module, they can change the default behaviour and need
# to be set before calling find_package:
#
#  PAPI_PREFIX         Set this variable to the root installation of
#                      libpapi if the module has problems finding the
#                      proper installation path.
#
# Variables defined by this module:
#
#  PAPI_FOUND              System has PAPI libraries and headers
#  PAPI_LIBRARIES_STATIC   The static PAPI library
#  PAPI_LIBRARIES_DYNAMIC  The dynamic PAPI library
#  PAPI_INCLUDE_DIRS       The location of PAPI headers

find_path(PAPI_PREFIX
    NAMES include/papi.h
)

SET(CMAKE_FIND_LIBRARY_SUFFIXES ".so")
find_library(PAPI_LIBRARIES_DYNAMIC
    NAMES papi
    HINTS ${PAPI_PREFIX}/lib ${HILTIDEPS}/lib
)

SET(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
find_library(PAPI_LIBRARIES_STATIC
    NAMES papi
    HINTS ${PAPI_PREFIX}/lib ${HILTIDEPS}/lib
)


find_path(PAPI_INCLUDE_DIRS
    NAMES papi.h
    HINTS ${PAPI_PREFIX}/include ${HILTIDEPS}/include
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PAPI DEFAULT_MSG
    PAPI_LIBRARIES_STATIC
    PAPI_LIBRARIES_DYNAMIC
    PAPI_INCLUDE_DIRS
)

mark_as_advanced(
    PAPI_PREFIX_DIRS
    PAPI_LIBRARIES_STATIC
    PAPI_LIBRARIES_DYNAMIC
    PAPI_INCLUDE_DIRS
)
