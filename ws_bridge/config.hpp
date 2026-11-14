#pragma once

#include <optional>
#include <string>
#include <vector>

namespace bot {

inline constexpr const char* kPublicUrl = "wss://chess.nqnja.dev/ws/bot";
inline constexpr const char* kLocalUrl  = "ws://127.0.0.1:3000/ws/bot";

struct Config {
    std::string url = kLocalUrl;
    std::string authKey;                 // never log this
    std::string authKeySource;           // where the key came from (safe to log)
    std::string identity = "bobette";    // stable: reuse it so games resume after restarts
    std::string name = "Bobette";
    std::vector<std::string> modes = {"fast", "deep"};
#ifdef _WIN32
    std::string caFile = "SYSTEM";       // Windows certificate store (trusted roots)
#else
    std::string caFile = "/etc/ssl/certs/ca-certificates.crt"; // IXWebSocket+mbedTLS: SYSTEM is Windows-only
#endif
    int pingIntervalSecs = 25;           // transport-level WebSocket Ping heartbeat
};

/// Builds the config from (in increasing priority) defaults, environment
/// variables and command-line flags. Returns std::nullopt on error or --help;
/// `message` then holds the error or usage text.
///
/// Environment variables:
///   BOBETTE_URL            server URL (default ws://127.0.0.1:3000/ws/bot)
///   BOBETTE_AUTH_KEY       the auth key itself
///   BOBETTE_AUTH_KEY_FILE  path to a file containing the auth key
///   BOBETTE_IDENTITY       stable bot identity (A-Z a-z 0-9 - _ .)
///   BOBETTE_NAME           display name
///   BOBETTE_MODES          comma-separated modes, e.g. "fast,deep"
///   BOBETTE_CA_FILE        PEM root bundle, or SYSTEM (default)
///
/// If none of those provide a key, auth_key.txt is searched for in the current
/// directory and its parents, then next to the executable and its parents
/// (so the one in the project root is found from out/build/<preset>/...).
///
/// Flags: --url, --key-file, --identity, --name, --modes, --ca-file, --public, --help
/// (there is deliberately no --key flag: keys on the command line leak into
/// shell history and process listings).
std::optional<Config> loadConfig(int argc, char** argv, std::string& message);

} // namespace bot
