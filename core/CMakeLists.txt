include_directories(include)
aux_source_directory(src ALL_SRCS)
add_library(hlink_core ${ALL_SRCS})
target_link_libraries(hlink_core PRIVATE spdlog::spdlog)
target_compile_definitions(hlink_core PRIVATE
        $<$<CONFIG:Debug>:DEBUG_MODE>
)