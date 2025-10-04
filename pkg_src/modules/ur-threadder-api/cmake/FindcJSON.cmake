# FindcJSON.cmake - Find cJSON library
#
# This module defines the following variables:
#  cJSON_FOUND - True if library found
#  cJSON_INCLUDE_DIRS - Include directories
#  cJSON_LIBRARIES - Libraries needed to use cJSON
#  cJSON_VERSION - Version of cJSON found

# Try to find the cJSON library
find_path(cJSON_INCLUDE_DIR
    NAMES cJSON.h
    PATH_SUFFIXES include
)

find_library(cJSON_LIBRARY
    NAMES cjson cJSON
    PATH_SUFFIXES lib
)

# Handle the QUIETLY and REQUIRED arguments and set cJSON_FOUND to TRUE if all variables are found
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(cJSON DEFAULT_MSG cJSON_LIBRARY cJSON_INCLUDE_DIR)

# In case we can't find the system library, use the local copy
if(NOT cJSON_FOUND)
    message(STATUS "Using bundled cJSON library")
    set(cJSON_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/thirdparty)
    set(cJSON_LIBRARY "")
    set(cJSON_FOUND TRUE)
endif()

# Hide these variables from CMake GUI
mark_as_advanced(cJSON_INCLUDE_DIR cJSON_LIBRARY)

# Set returned variables
set(cJSON_INCLUDE_DIRS ${cJSON_INCLUDE_DIR})
set(cJSON_LIBRARIES ${cJSON_LIBRARY})