# engine_tests: doctest unit tests, registered with CTest.
add_executable(engine_tests
    test_main.cpp
    test_perft.cpp
)

target_link_libraries(engine_tests PRIVATE engine_core doctest::doctest)

# Protocol / resulting-FEN tests (no network involved).
if(TARGET bot_protocol)
    target_sources(engine_tests PRIVATE test_protocol.cpp)
    target_link_libraries(engine_tests PRIVATE bot_protocol)
endif()

engine_set_warnings(engine_tests)

# Registers each TEST_CASE as its own CTest test (shows up individually in
# Visual Studio's Test Explorer).
include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
doctest_discover_tests(engine_tests)
