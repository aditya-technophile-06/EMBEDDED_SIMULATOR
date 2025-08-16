cmake_minimum_required(VERSION 3.16)
project(EmbeddedSystemsSimulator VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable testing
enable_testing()

# Build type configuration
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# Compiler-specific options
if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Werror -pthread)
    
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-g -O0 -DDEBUG)
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
            add_compile_options(-fsanitize=address -fsanitize=undefined)
            add_link_options(-fsanitize=address -fsanitize=undefined)
        endif()
    elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(-O2 -DNDEBUG -march=native)
    endif()
endif()

# Include directories
include_directories(include)

# Find required packages
find_package(Threads REQUIRED)

# Source files
file(GLOB_RECURSE SDK_SOURCES "src/sdk/*.cpp")
file(GLOB_RECURSE DRIVER_SOURCES "src/drivers/*.cpp")
file(GLOB_RECURSE RTOS_SOURCES "src/rtos/*.cpp")
file(GLOB_RECURSE PROTOCOL_SOURCES "src/protocol/*.cpp")
file(GLOB_RECURSE COMMON_SOURCES "src/common/*.cpp")

set(LIB_SOURCES
    ${SDK_SOURCES}
    ${DRIVER_SOURCES}
    ${RTOS_SOURCES}
    ${PROTOCOL_SOURCES}
    ${COMMON_SOURCES}
)

# Create static library
add_library(embedded_sim STATIC ${LIB_SOURCES})
target_link_libraries(embedded_sim Threads::Threads)

# Main executable
add_executable(embedded_simulator src/main.cpp)
target_link_libraries(embedded_simulator embedded_sim Threads::Threads)

# Examples
file(GLOB EXAMPLE_SOURCES "examples/*.cpp")
foreach(EXAMPLE_SOURCE ${EXAMPLE_SOURCES})
    get_filename_component(EXAMPLE_NAME ${EXAMPLE_SOURCE} NAME_WE)
    add_executable(${EXAMPLE_NAME} ${EXAMPLE_SOURCE})
    target_link_libraries(${EXAMPLE_NAME} embedded_sim Threads::Threads)
endforeach()

# Tests (if available)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    file(GLOB TEST_SOURCES "tests/*.cpp")
    if(TEST_SOURCES)
        add_executable(test_runner ${TEST_SOURCES})
        target_link_libraries(test_runner embedded_sim Threads::Threads)
        add_test(NAME unit_tests COMMAND test_runner)
    endif()
endif()

# Installation
install(TARGETS embedded_simulator
        RUNTIME DESTINATION bin)

install(TARGETS embedded_sim
        ARCHIVE DESTINATION lib)

install(DIRECTORY include/
        DESTINATION include/embedded_sim
        FILES_MATCHING PATTERN "*.h")

# Packaging
set(CPACK_PACKAGE_NAME "EmbeddedSystemsSimulator")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Virtual Embedded Systems Simulator")
set(CPACK_PACKAGE_VENDOR "Embedded Systems Developer")
set(CPACK_PACKAGE_CONTACT "developer@example.com")

if(WIN32)
    set(CPACK_GENERATOR "NSIS")
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
else()
    set(CPACK_GENERATOR "TGZ;DEB")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6, libstdc++6, libgcc1")
endif()

include(CPack)

# Custom targets
add_custom_target(docs
    COMMAND doxygen ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Generating documentation"
    VERBATIM)

add_custom_target(format
    COMMAND find ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/include 
            -name "*.cpp" -o -name "*.h" | xargs clang-format -i
    COMMENT "Formatting code"
    VERBATIM)

# Print build information
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "Found ${CMAKE_THREAD_LIBS_INIT} threading library")