# bot_protocol: wire format + FEN-in/FEN-out glue around engine_core.
# No sockets here, so the unit tests can link it too.
add_library(bot_protocol STATIC
    protocol.cpp
    prediction.cpp
)
target_include_directories(bot_protocol PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(bot_protocol PUBLIC engine_core)
engine_set_warnings(bot_protocol)

# engine_ws: outbound WebSocket client for the chess backend.
# The only target that links IXWebSocket.
add_executable(engine_ws
    main.cpp
    config.cpp
    bot_client.cpp
)

target_link_libraries(engine_ws PRIVATE bot_protocol ixwebsocket::ixwebsocket)

if(WIN32)
    target_link_libraries(engine_ws PRIVATE ws2_32)
    # Keep <windows.h> (pulled in by the socket headers) from defining
    # min/max macros that break std::min/std::max in chess.hpp.
    target_compile_definitions(engine_ws PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

engine_set_warnings(engine_ws)
