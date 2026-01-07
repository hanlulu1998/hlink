cmake_minimum_required(VERSION 4.0)
project(hlink LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Debug)

find_package(spdlog REQUIRED)


add_library(hlink INTERFACE)
target_link_libraries(hlink INTERFACE hlink_core hlink_net)
target_include_directories(hlink
        INTERFACE
        ${PROJECT_SOURCE_DIR}/core/include
        ${PROJECT_SOURCE_DIR}/net/include
)

enable_testing()
add_subdirectory(core)
add_subdirectory(net)
add_subdirectory(tests)

