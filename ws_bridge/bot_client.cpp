#include "bot_client.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <random>
#include <sstream>
#include <type_traits>
#include <variant>

#include <ixwebsocket/IXSocketTLSOptions.h>

#include "prediction.hpp"

namespace bot {

namespace {

std::mutex g_logMutex;

/// Logs with a stage tag so TLS / auth / upgrade / protocol / engine problems
/// are easy to tell apart. Never pass the auth key in here.
void log(std::string_view stage, const std::string& text) {
    // UTC time of day, HH:MM:SS.mmm
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count() % 86400000;
    char stamp[32];
    std::snprintf(stamp, sizeof stamp, "%02d:%02d:%02d.%03d", static_cast<int>(ms / 3600000),
                  static_cast<int>(ms / 60000 % 60), static_cast<int>(ms / 1000 % 60),
                  static_cast<int>(ms % 1000));
    std::lock_guard lock(g_logMutex);
    std::cerr << stamp << "Z [" << stage << "] " << text << std::endl;
}

std::uint64_t unixMicros() {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
}

std::string joinModes(const std::vector<std::string>& modes) {
    std::string out;
    for (const auto& m : modes) {
        if (!out.empty()) out += ", ";
        out += m;
    }
    return out;
}

} // namespace

BotClient::BotClient(Config config) : config_(std::move(config)) {
    ws_.setUrl(config_.url);

    if (!config_.authKey.empty()) {
        ix::WebSocketHttpHeaders headers;
        headers["Authorization"] = "Bearer " + config_.authKey;
        ws_.setExtraHeaders(headers);
    }

    ix::SocketTLSOptions tls;
    tls.caFile = config_.caFile; // "SYSTEM" = OS root store; verification stays ON
    ws_.setTLSOptions(tls);

    ws_.disablePerMessageDeflate();           // not needed; keeps frames simple
    ws_.setPingInterval(config_.pingIntervalSecs); // transport Ping heartbeat
    ws_.setHandshakeTimeout(10);

    // Reconnect with capped exponential backoff (1 s .. 30 s).
    ws_.enableAutomaticReconnection();
    ws_.setMinWaitBetweenReconnectionRetries(1000);
    ws_.setMaxWaitBetweenReconnectionRetries(30000);

    // Install the receive handler before connecting: a queued prediction can
    // arrive right after `registered`.
    ws_.setOnMessageCallback([this](const ix::WebSocketMessagePtr& msg) { onMessage(msg); });
}

BotClient::~BotClient() {
    ws_.stop();
    {
        std::lock_guard lock(jobMutex_);
        stopWorker_ = true;
    }
    jobCv_.notify_all();
    if (worker_.joinable()) worker_.join();
}

int BotClient::run() {
    if (auto err = validateRegistration(config_.identity, config_.name, config_.modes)) {
        log("config", "invalid registration: " + *err);
        return 2;
    }

    log("config", "url=" + config_.url + " identity=" + config_.identity + " name=\"" +
                      config_.name + "\" modes=[" + joinModes(config_.modes) + "] auth=" +
                      (config_.authKey.empty() ? "none" : "key loaded from " + config_.authKeySource + " (" +
                       std::to_string(config_.authKey.size()) + " chars)"));

    worker_ = std::thread([this] { workerLoop(); });
    ws_.start();
    log("connect", "connecting...");

    std::unique_lock lock(doneMutex_);
    doneCv_.wait(lock, [this] { return done_; });
    const int code = exitCode_;
    lock.unlock();

    ws_.stop();
    log("shutdown", "stopped");
    return code;
}

void BotClient::requestStop() { finish(0); }

void BotClient::finish(int exitCode) {
    {
        std::lock_guard lock(doneMutex_);
        if (done_) return;
        done_ = true;
        exitCode_ = exitCode;
    }
    doneCv_.notify_all();
}

void BotClient::closeConnection(const std::string& why) {
    log("connect", "closing connection: " + why);
    ws_.close(); // auto-reconnect will follow
}

// ---------------------------------------------------------------------------
// Network thread
// ---------------------------------------------------------------------------

void BotClient::onMessage(const ix::WebSocketMessagePtr& msg) {
    // Timestamp first, before any parsing (protocol step 1).
    const std::uint64_t receivedUs = unixMicros();

    switch (msg->type) {
        case ix::WebSocketMessageType::Open: onOpen(); break;
        case ix::WebSocketMessageType::Close: onClose(msg); break;
        case ix::WebSocketMessageType::Error: onError(msg); break;
        case ix::WebSocketMessageType::Message:
            if (!msg->binary) {
                closeConnection("server sent a text message; protocol v2 is binary only");
                break;
            }
            onBinary(msg->str, receivedUs);
            break;
        case ix::WebSocketMessageType::Ping:
        case ix::WebSocketMessageType::Pong:
        case ix::WebSocketMessageType::Fragment:
            break; // transport-level; IXWebSocket answers Pings itself
    }
}

void BotClient::onOpen() {
    log("upgrade", "HTTP 101, WebSocket open");
    {
        std::lock_guard lock(mutex_);
        ++generation_;
        state_ = State::AwaitingRegistered;
        openedAt_ = std::chrono::steady_clock::now();
        ws_.sendBinary(encodeRegister(config_.identity, config_.name, config_.modes));
    }
    log("protocol", "sent register (v2)");
}

void BotClient::onClose(const ix::WebSocketMessagePtr& msg) {
    std::chrono::steady_clock::duration lived{};
    {
        std::lock_guard lock(mutex_);
        ++generation_; // any in-flight search result is now stale
        state_ = State::Disconnected;
        lived = std::chrono::steady_clock::now() - openedAt_;
    }

    // Capped exponential backoff with jitter: 1 s, 2 s, 4 s ... 30 s.
    // A connection that stayed up for a while resets the backoff.
    if (lived > std::chrono::seconds(30)) quickCloses_ = 0;
    const unsigned exponent = quickCloses_ < 5 ? quickCloses_ : 5;
    ++quickCloses_;
    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<double> jitter(0.75, 1.25);
    const auto baseMs = std::min(30000.0, 1000.0 * static_cast<double>(1u << exponent));
    const auto delay = std::chrono::milliseconds(static_cast<long long>(baseMs * jitter(rng)));

    std::ostringstream text;
    text << "closed (code " << msg->closeInfo.code << ", reason \"" << msg->closeInfo.reason
         << "\"), reconnecting in " << delay.count() << " ms";
    log("connect", text.str());

    // Runs on IXWebSocket's thread, which reconnects as soon as we return.
    // Wake up early if we're shutting down.
    std::unique_lock lock(doneMutex_);
    doneCv_.wait_for(lock, delay, [this] { return done_; });
}

void BotClient::onError(const ix::WebSocketMessagePtr& msg) {
    const auto& info = msg->errorInfo;
    std::ostringstream text;
    text << info.reason << " (http " << info.http_status << ", retry #" << info.retries
         << " in " << static_cast<long long>(info.wait_time) << " ms)";

    if (info.http_status == 401) {
        log("auth", "HTTP 401: key missing or wrong. Fix the key; not retrying.");
        ws_.disableAutomaticReconnection();
        finish(3);
        return;
    }
    if (info.http_status != 0) {
        log("upgrade", "upgrade failed: " + text.str());
    } else {
        log("tcp/tls", "connection failed: " + text.str());
    }
}

void BotClient::onBinary(const std::string& data, std::uint64_t receivedUs) {
    std::string error;
    auto packet = decodeServerPacket(data, error);
    if (!packet) {
        closeConnection("malformed packet from server: " + error);
        return;
    }

    std::string closeReason; // set => close after releasing the lock
    std::optional<Job> newJob;

    std::visit([&](auto&& p) {
        using T = std::decay_t<decltype(p)>;
        std::lock_guard lock(mutex_);

        if constexpr (std::is_same_v<T, Registered>) {
            if (state_ != State::AwaitingRegistered) {
                closeReason = "unexpected 'registered'";
            } else if (p.version != kProtocolVersion) {
                closeReason = "server registered us with unsupported version " +
                              std::to_string(p.version);
            } else {
                state_ = State::Idle;
                log("protocol", "registered as '" + config_.identity + "', waiting for predictions");
            }
        } else if constexpr (std::is_same_v<T, PredictMove>) {
            if (state_ != State::Idle) {
                closeReason = "predict_move while not idle";
                return;
            }
            // Steps 2 + 3: received, then application pong, before any chess work.
            ws_.sendBinary(encodeReceived(p.requestId, receivedUs));
            ws_.sendBinary(encodePong(p.requestId));
            state_ = State::Searching;
            currentRequest_ = p.requestId;
            newJob = Job{generation_, p.requestId, p.mode, p.fen};
            log("protocol", "predict #" + std::to_string(p.requestId) + " mode=" + p.mode +
                                " fen=" + p.fen);
        } else if constexpr (std::is_same_v<T, Accepted>) {
            if (state_ == State::AwaitingAccept && p.requestId == currentRequest_) {
                state_ = State::Idle;
                log("protocol", "accepted #" + std::to_string(p.requestId));
            } else {
                log("protocol", "ignoring stray accepted #" + std::to_string(p.requestId));
            }
        } else if constexpr (std::is_same_v<T, ServerError>) {
            log("server-error", p.reason);
            if (state_ == State::AwaitingRegistered &&
                p.reason.find("already connected") == std::string::npos) {
                // Registration was rejected for a reason reconnecting won't fix.
                ws_.disableAutomaticReconnection();
                closeReason = "registration rejected";
            }
        }
    }, *packet);

    if (newJob) {
        {
            std::lock_guard lock(jobMutex_);
            job_ = std::move(newJob);
        }
        jobCv_.notify_one();
    }
    if (!closeReason.empty()) {
        closeConnection(closeReason);
        if (closeReason == "registration rejected") finish(4);
    }
}

// ---------------------------------------------------------------------------
// Worker thread
// ---------------------------------------------------------------------------

void BotClient::workerLoop() {
    while (true) {
        Job job;
        {
            std::unique_lock lock(jobMutex_);
            jobCv_.wait(lock, [this] { return stopWorker_ || job_.has_value(); });
            if (stopWorker_) return;
            job = std::move(*job_);
            job_.reset();
        }

        const auto start = std::chrono::steady_clock::now();
        std::string error;
        const auto reply = answerPrediction(job.fen, job.mode, error);
        const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                   std::chrono::steady_clock::now() - start).count();

        bool mustClose = false;
        {
            std::lock_guard lock(mutex_);
            const bool stillCurrent = job.generation == generation_ &&
                                      state_ == State::Searching &&
                                      currentRequest_ == job.requestId;
            if (!stillCurrent) {
                log("engine", "discarding result for #" + std::to_string(job.requestId) +
                                  " (connection changed)");
                continue;
            }
            if (reply) {
                ws_.sendBinary(encodeMove(job.requestId, *reply));
                state_ = State::AwaitingAccept;
                log("engine", "move #" + std::to_string(job.requestId) + " -> " + *reply + " (" +
                                  std::to_string(elapsedMs) + " ms)");
            } else {
                log("engine", "FAILED on #" + std::to_string(job.requestId) + ": " + error);
                mustClose = true;
            }
        }
        if (mustClose) {
            // Protocol: never fabricate a reply; disconnect instead.
            closeConnection("engine could not answer");
        }
    }
}

} // namespace bot
