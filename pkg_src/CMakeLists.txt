cmake_minimum_required(VERSION 3.10)
project(ur-mavrouter)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Option to enable HTTP server build
option(_BUILD_HTTP "Build with HTTP server support" ON)

add_definitions(-DPACKAGE="mavlink-routerd")
add_definitions(-Wno-address-of-packed-member)
add_definitions(-Wno-return-type)
add_definitions(-Wno-unused-result)
add_definitions(-O2)
add_definitions(-g)

# Add the ur-threadder-api subdirectory
add_subdirectory(modules/ur-threadder-api)
message(STATUS "LIBS = ${THREADDER_LIBS}")
message(STATUS "INCLUDE_DIRS = ${THREADDER_INCLUDE_DIRS}")

# Conditionally add HTTP module
if(_BUILD_HTTP)
    message(STATUS "HTTP server support enabled")
    add_definitions(-D_BUILD_HTTP)

    # Add the http-module subdirectory
    add_subdirectory(modules/http-module)
    message(STATUS "HTTP_MODULE_LIBS = ${HTTP_MODULE_LIBS}")
    message(STATUS "HTTP_MODULE_INCLUDE_DIRS = ${HTTP_MODULE_INCLUDE_DIRS}")

    # Find libmicrohttpd for main executable
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(MHD REQUIRED libmicrohttpd)
else()
    message(STATUS "HTTP server support disabled")
    set(HTTP_MODULE_LIBS "")
    set(HTTP_MODULE_INCLUDE_DIRS "")
    set(MHD_INCLUDE_DIRS "")
endif()

include_directories(
    .
    modules/mavlink_c_library_v2/ardupilotmega
    ${THREADDER_INCLUDE_DIRS}
    ${HTTP_MODULE_INCLUDE_DIRS}
    ${MHD_INCLUDE_DIRS}
    src
    src/rpc-mechanisms
    src/mavlink-extensions
)

set(SRC_FILES
    src/autolog.cpp
    src/binlog.cpp
    src/dedup.cpp
    src/endpoint.cpp
    src/endpoint_stats.cpp
    src/git_version.cpp
    src/logendpoint.cpp
    src/main.cpp
    src/mainloop.cpp
    src/pollable.cpp
    src/timeout.cpp
    src/ulog.cpp
    src/tlog.cpp
    src/common/conf_file.cpp
    src/common/json_config.cpp
    src/common/log.cpp
    src/common/util.cpp
    src/common/xtermios.cpp
    src/rpc-mechanisms/RpcController.cpp
    src/mavlink-extensions/ExtensionManager.cpp
)

configure_file(src/version.h.in src/git_version.h)
add_executable(${PROJECT_NAME} ${SRC_FILES} src/git_version.h)
target_link_libraries(${PROJECT_NAME} rt m pthread ${THREADDER_LIBS} ${HTTP_MODULE_LIBS})
