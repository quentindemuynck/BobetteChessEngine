#pragma once
// Outbound WebSocket client that registers with the backend and answers
// predict_move requests (protocol v2, see PROTOCOL.md).
//
// Threads:
//   * IXWebSocket's own thread runs the network loop and calls onMessage().
//     It only does cheap work: decode, send `received` + `pong`, hand off.
//   * One worker thread runs the engine, so transport Pings/Close keep being
//     serviced during long searches.
// All application writes go through mutex_, and every search result is tagged
// with the connection generation it belongs to, so a result from a dropped
// connection is never sent on a new one.

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

#include <ixwebsocket/IXWebSocket.h>

#include "config.hpp"
#include "protocol.hpp"

namespace bot {

class BotClient {
public:
    explicit BotClient(Config config);
    ~BotClient();

    BotClient(const BotClient&) = delete;
    BotClient& operator=(const BotClient&) = delete;

    /// Connects (and reconnects with backoff) until requestStop() is called or a
    /// non-recoverable error occurs (bad credentials, rejected registration).
    /// Returns a process exit code.
    int run();

    /// Thread-safe; may be called from a signal-handling context via a flag poll.
    void requestStop();

private:
    enum class State { Disconnected, AwaitingRegistered, Idle, Searching, AwaitingAccept };

    struct Job {
        std::uint64_t generation = 0;
        std::uint64_t requestId = 0;
        std::string mode;
        std::string fen;
    };

    void onMessage(const ix::WebSocketMessagePtr& msg);
    void onOpen();
    void onClose(const ix::WebSocketMessagePtr& msg);
    void onError(const ix::WebSocketMessagePtr& msg);
    void onBinary(const std::string& data, std::uint64_t receivedUnixUs);

    void workerLoop();
    void finish(int exitCode);
    void closeConnection(const std::string& why);

    Config config_;
    ix::WebSocket ws_;

    // Connection state + all application sends. Never call ws_.close()/stop()
    // while holding this (IXWebSocket may call back into onMessage).
    std::mutex mutex_;
    State state_ = State::Disconnected;
    std::uint64_t generation_ = 0;     // bumped on every open/close
    std::uint64_t currentRequest_ = 0;

    // Backoff after a connection that opened and then closed. IXWebSocket only
    // backs off failed *connects*; without this a server that keeps kicking us
    // would be hammered with instant reconnects.
    std::chrono::steady_clock::time_point openedAt_{};
    unsigned quickCloses_ = 0;

    // Worker hand-off
    std::mutex jobMutex_;
    std::condition_variable jobCv_;
    std::optional<Job> job_;
    bool stopWorker_ = false;
    std::thread worker_;

    // Shutdown
    std::mutex doneMutex_;
    std::condition_variable doneCv_;
    bool done_ = false;
    int exitCode_ = 0;
};

} // namespace bot
