include_directories(include)
include_directories(${PROJECT_SOURCE_DIR}/core/include)
file(GLOB_RECURSE ALL_SRCS "src/*.cpp" "src/*.c")
add_library(hlink_net ${ALL_SRCS})
target_link_libraries(hlink_net PRIVATE hlink_core)

include(CheckFunctionExists)
check_function_exists(accept4 HAVE_ACCEPT4)
if(NOT HAVE_ACCEPT4)
    target_compile_definitions(hlink_net PRIVATE
            $<$<CONFIG:Debug>:NO_ACCEPT4>
    )
endif()