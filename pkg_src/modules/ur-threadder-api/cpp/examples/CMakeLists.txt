
# Examples CMakeLists.txt

# Basic thread example
add_executable(basic_example basic_example.cpp)
target_link_libraries(basic_example PRIVATE ThreadManager::ThreadManager)

# Process example
add_executable(process_example process_example.cpp)
target_link_libraries(process_example PRIVATE ThreadManager::ThreadManager)

# RAII wrapper example
add_executable(raii_example raii_example.cpp)
target_link_libraries(raii_example PRIVATE ThreadManager::ThreadManager)

# Thread registration example
add_executable(registration_example registration_example.cpp)
target_link_libraries(registration_example PRIVATE ThreadManager::ThreadManager)

# Install examples (optional)
if(INSTALL_EXAMPLES)
    install(TARGETS 
        basic_example 
        process_example 
        raii_example 
        registration_example
        DESTINATION ${CMAKE_INSTALL_BINDIR}/threadmanager-examples
    )
endif()
