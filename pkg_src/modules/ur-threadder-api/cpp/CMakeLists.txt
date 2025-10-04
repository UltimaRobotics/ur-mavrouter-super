
cmake_minimum_required(VERSION 3.16)
project(ThreadManagerWrapper VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

# Build options
option(BUILD_SHARED_LIBS "Build shared libraries instead of static" OFF)
option(ENABLE_SANITIZERS "Enable AddressSanitizer and UBSan in debug builds" OFF)


# Compiler-specific options
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra -Wpedantic")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

    if(ENABLE_SANITIZERS AND CMAKE_BUILD_TYPE MATCHES Debug)
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address -fsanitize=undefined -fPIC")
        set(CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fsanitize=address -fsanitize=undefined -fPIC")
    endif()
endif()

# Find required packages
find_package(Threads REQUIRED)
find_package(PkgConfig QUIET)


# Define directories
set(INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

# C++ wrapper library
set(CPP_WRAPPER_SOURCES
    ${SRC_DIR}/ThreadManager.cpp
)

set(CPP_WRAPPER_HEADERS
    ${INCLUDE_DIR}/ThreadManager.hpp
    ${INCLUDE_DIR}/ThreadManager.tpp
)


# Create the C++ wrapper library
if(BUILD_SHARED_LIBS)
    add_library(threadmanager_cpp SHARED ${CPP_WRAPPER_SOURCES})
else()
    add_library(threadmanager_cpp STATIC ${CPP_WRAPPER_SOURCES})
endif()

# Set library properties
set_target_properties(threadmanager_cpp PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    OUTPUT_NAME "threadmanager_cpp"
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
)

target_include_directories(threadmanager_cpp 
    PUBLIC 
        $<BUILD_INTERFACE:${INCLUDE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${SRC_DIR}
)

target_link_libraries(threadmanager_cpp 
    PUBLIC 
        threadmanager
        Threads::Threads
)

# Add alias for easier consumption
add_library(ThreadManager::ThreadManager ALIAS threadmanager_cpp)

option(BUILD_EXAMPLES "Build examples" OFF)
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()