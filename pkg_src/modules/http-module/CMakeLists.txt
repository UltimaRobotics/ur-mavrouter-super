
cmake_minimum_required(VERSION 3.10)
project(HttpModule VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find libmicrohttpd
find_package(PkgConfig REQUIRED)
pkg_check_modules(MHD REQUIRED libmicrohttpd)

# Get threadder include directories from parent scope
set(THREADDER_API_INCLUDE_DIRS 
    ${CMAKE_CURRENT_SOURCE_DIR}/../ur-threadder-api/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../ur-threadder-api/cpp/include
)

# Source files
set(HTTP_MODULE_SOURCES
    src/HttpServer.cpp
)

# Header files
set(HTTP_MODULE_HEADERS
    include/HttpServer.hpp
)

# Create library
add_library(http_module STATIC ${HTTP_MODULE_SOURCES})

target_include_directories(http_module
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${MHD_INCLUDE_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/rpc-mechanisms
        ${CMAKE_CURRENT_SOURCE_DIR}/../../modules/mavlink_c_library_v2/ardupilotmega
        ${THREADDER_API_INCLUDE_DIRS}
)

target_link_libraries(http_module
    PUBLIC
        ${MHD_LIBRARIES}
)

# Export variables for parent project
set(HTTP_MODULE_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include PARENT_SCOPE)
set(HTTP_MODULE_LIBS http_module ${MHD_LIBRARIES} PARENT_SCOPE)
