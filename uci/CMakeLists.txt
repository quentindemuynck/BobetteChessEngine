# engine_uci: UCI protocol over stdin/stdout (for GUIs like Cute Chess / Arena).
add_executable(engine_uci main.cpp)

target_link_libraries(engine_uci PRIVATE engine_core)
engine_set_warnings(engine_uci)
