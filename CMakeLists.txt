cmake_minimum_required(VERSION 3.20)
project(asymmetric-leap-scanner VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

add_executable(asymmetric-leap-scanner
    src/main.cpp
    src/black_scholes.cpp
)

target_include_directories(asymmetric-leap-scanner PRIVATE include)
target_link_libraries(asymmetric-leap-scanner PRIVATE Threads::Threads)

if(WIN32)
    target_link_libraries(asymmetric-leap-scanner PRIVATE ws2_32)
endif()

if(MSVC)
    target_compile_options(asymmetric-leap-scanner PRIVATE /W4 /permissive-)
else()
    target_compile_options(asymmetric-leap-scanner PRIVATE -Wall -Wextra -Wpedantic)
endif()