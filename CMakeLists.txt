cmake_minimum_required(VERSION 4.0)
project(hlink LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Debug)

find_package(spdlog REQUIRED)

include_directories(${PROJECT_SOURCE_DIR}/core/include)
include_directories(${PROJECT_SOURCE_DIR}/net/include)
add_executable(hlink main.cpp)
target_link_libraries(hlink PRIVATE hlink_core hlink_net)
enable_testing()
add_subdirectory(core)
add_subdirectory(net)
add_subdirectory(tests)

