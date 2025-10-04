cmake_minimum_required(VERSION 3.10)
project(ThreadManager VERSION 1.0 LANGUAGES C)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Add custom cmake modules path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Compiler flags
add_compile_options(-Wall -Wextra -Wpedantic)

# Define POSIX source for better compatibility
add_definitions(-D_POSIX_C_SOURCE=200809L)

# Threads dependency
find_package(Threads REQUIRED)

# Find or use bundled cJSON
find_package(cJSON)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${cJSON_INCLUDE_DIRS}
)

# Source files for the library
set(LIBRARY_SOURCES
    src/thread_manager.c
    src/json_config.c
    src/utils.c
)

# Add cJSON source if using bundled version
if(NOT cJSON_LIBRARIES)
    list(APPEND LIBRARY_SOURCES thirdparty/cJSON.c)
endif()

# Create static library
add_library(threadmanager STATIC ${LIBRARY_SOURCES})
target_link_libraries(threadmanager PRIVATE Threads::Threads ${cJSON_LIBRARIES})

option(BUILD_EXAMPLES "Build examples" OFF)
if(BUILD_EXAMPLES)
	# Examples
	add_executable(example example/example.c)
	target_link_libraries(example PRIVATE threadmanager Threads::Threads)

	# C registration example
	add_executable(c_registration_example examples/c_registration_example.c)
	target_link_libraries(c_registration_example PRIVATE threadmanager Threads::Threads)
endif()

add_subdirectory(cpp)

# Installation rules
install(TARGETS threadmanager DESTINATION lib)
install(FILES include/thread_manager.h include/json_config.h include/utils.h 
        DESTINATION include/threadmanager)


# Export target names as variables for backward compatibility (optional)
set(THREADDER_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/cpp/include
    ${cJSON_INCLUDE_DIRS}
    CACHE INTERNAL "Threadder include directories"
)

set(THREADDER_LIBS
    threadmanager
    threadmanager_cpp
    CACHE INTERNAL "Threadder libraries"
)
