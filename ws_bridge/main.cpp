// engine_ws: connects OUT to the chess backend over WebSocket, registers the
// engine, and answers move requests (application protocol v2).
//
//   engine_ws                       local backend  ws://127.0.0.1:3000/ws/bot
//   engine_ws --public              public backend wss://chess.nqnja.dev/ws/bot
//   engine_ws --help                all options
//
// The auth key comes from BOBETTE_AUTH_KEY or a key file, never from argv.

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>

#include <ixwebsocket/IXNetSystem.h>

#include "bot_client.hpp"
#include "config.hpp"

namespace {
std::atomic<bool> g_interrupted{false};
void onSignal(int) { g_interrupted = true; }
} // namespace

int main(int argc, char** argv) {
    std::string message;
    auto config = bot::loadConfig(argc, argv, message);
    if (!config) {
        std::cerr << message << "\n";
        return message.rfind("Usage:", 0) == 0 ? 0 : 2;
    }

    ix::initNetSystem(); // WSAStartup on Windows

    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);

    int code = 0;
    {
        bot::BotClient client(std::move(*config));

        // Ctrl+C watcher (signal handlers may only set a flag).
        std::atomic<bool> finished{false};
        std::thread watcher([&] {
            while (!finished) {
                if (g_interrupted) {
                    client.requestStop();
                    return;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        });

        code = client.run();
        finished = true;
        watcher.join();
    }

    ix::uninitNetSystem();
    return code;
}
