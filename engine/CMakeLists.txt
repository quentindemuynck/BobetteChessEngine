# engine_core: the pure engine. No networking, no I/O protocol code.
add_library(engine_core STATIC
    src/search.cpp
    src/evaluate.cpp
    src/time_manager.cpp
)

target_include_directories(engine_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include)

# chess-library types appear in our public headers, so link it PUBLIC.
target_link_libraries(engine_core PUBLIC chess::chess_library)

target_compile_features(engine_core PUBLIC cxx_std_20)
engine_set_warnings(engine_core)
