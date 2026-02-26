cmake_minimum_required(VERSION 3.30)
project(tio VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(cmake/DetectPlatform.cmake)

option(TIO_BUILD_TESTS "Build unit tests" ON)
option(TIO_BUILD_EXAMPLES "Build examples" ON)

add_subdirectory(src/tio)

if (TIO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif ()

if (TIO_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif ()
