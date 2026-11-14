#include "config.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string_view>

namespace bot {

namespace {

std::optional<std::string> getEnv(const char* name) {
#ifdef _MSC_VER
    char* buffer = nullptr;
    std::size_t size = 0;
    if (_dupenv_s(&buffer, &size, name) != 0 || buffer == nullptr) return std::nullopt;
    std::string value(buffer);
    std::free(buffer);
#else
    const char* raw = std::getenv(name); // NOLINT(concurrency-mt-unsafe)
    if (raw == nullptr) return std::nullopt;
    std::string value(raw);
#endif
    if (value.empty()) return std::nullopt;
    return value;
}

std::string trim(std::string_view s) {
    const auto first = s.find_first_not_of(" \t\r\n\f\v");
    if (first == std::string_view::npos) return {};
    const auto last = s.find_last_not_of(" \t\r\n\f\v");
    std::string out(s.substr(first, last - first + 1));
    // Strip a UTF-8 BOM that Notepad likes to add to text files.
    if (out.rfind("\xEF\xBB\xBF", 0) == 0) out.erase(0, 3);
    return out;
}

std::vector<std::string> splitModes(std::string_view csv) {
    std::vector<std::string> modes;
    std::size_t start = 0;
    while (start <= csv.size()) {
        const auto comma = csv.find(',', start);
        const auto end = comma == std::string_view::npos ? csv.size() : comma;
        auto mode = trim(csv.substr(start, end - start));
        if (!mode.empty()) modes.push_back(std::move(mode));
        if (comma == std::string_view::npos) break;
        start = comma + 1;
    }
    return modes;
}

bool readKeyFile(const std::string& path, std::string& key, std::string& message) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        message = "cannot open key file '" + path + "'";
        return false;
    }
    std::stringstream buffer;
    buffer << in.rdbuf();
    key = trim(buffer.str());
    if (key.empty()) {
        message = "key file '" + path + "' is empty";
        return false;
    }
    return true;
}

/// Looks for auth_key.txt in `start` and each parent directory.
std::optional<std::filesystem::path> findKeyFileUpwards(std::filesystem::path start) {
    std::error_code ec;
    start = std::filesystem::absolute(start, ec);
    if (ec) return std::nullopt;
    for (auto dir = start; !dir.empty(); dir = dir.parent_path()) {
        const auto candidate = dir / "auth_key.txt";
        if (std::filesystem::is_regular_file(candidate, ec)) return candidate;
        if (dir == dir.parent_path()) break; // reached the drive root
    }
    return std::nullopt;
}

constexpr const char* kUsage =
    "Usage: engine_ws [options]\n"
    "  --public              connect to wss://chess.nqnja.dev/ws/bot\n"
    "  --url <url>           server URL (default ws://127.0.0.1:3000/ws/bot)\n"
    "  --key-file <path>     file containing the auth key\n"
    "  --identity <id>       stable bot identity (default: bobette)\n"
    "  --name <name>         display name (default: Bobette)\n"
    "  --modes <a,b,...>     advertised modes (default: fast,deep)\n"
    "  --ca-file <path>      PEM root certificates (default: system store)\n"
    "Environment: BOBETTE_URL, BOBETTE_AUTH_KEY, BOBETTE_AUTH_KEY_FILE,\n"
    "             BOBETTE_IDENTITY, BOBETTE_NAME, BOBETTE_MODES, BOBETTE_CA_FILE\n";

} // namespace

std::optional<Config> loadConfig(int argc, char** argv, std::string& message) {
    Config cfg;
    std::optional<std::string> keyFile;

    // 1. Environment
    if (auto v = getEnv("BOBETTE_URL")) cfg.url = trim(*v);
    if (auto v = getEnv("BOBETTE_AUTH_KEY")) {
        cfg.authKey = trim(*v);
        cfg.authKeySource = "BOBETTE_AUTH_KEY";
    }
    if (auto v = getEnv("BOBETTE_AUTH_KEY_FILE")) keyFile = trim(*v);
    if (auto v = getEnv("BOBETTE_IDENTITY")) cfg.identity = trim(*v);
    if (auto v = getEnv("BOBETTE_NAME")) cfg.name = trim(*v);
    if (auto v = getEnv("BOBETTE_MODES")) cfg.modes = splitModes(*v);
    if (auto v = getEnv("BOBETTE_CA_FILE")) cfg.caFile = trim(*v);

    // 2. Command line
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        auto next = [&]() -> std::optional<std::string> {
            if (i + 1 >= argc) return std::nullopt;
            return std::string(argv[++i]);
        };

        if (arg == "--help" || arg == "-h") {
            message = kUsage;
            return std::nullopt;
        } else if (arg == "--public") {
            cfg.url = kPublicUrl;
        } else if (arg == "--url" || arg == "--key-file" || arg == "--identity" ||
                   arg == "--name" || arg == "--modes" || arg == "--ca-file") {
            auto value = next();
            if (!value) {
                message = std::string("missing value for ") + std::string(arg) + "\n" + kUsage;
                return std::nullopt;
            }
            if (arg == "--url") cfg.url = *value;
            else if (arg == "--key-file") keyFile = *value;
            else if (arg == "--identity") cfg.identity = *value;
            else if (arg == "--name") cfg.name = *value;
            else if (arg == "--modes") cfg.modes = splitModes(*value);
            else cfg.caFile = *value;
        } else if (arg == "--key") {
            message = "--key is not supported (it would leak the key); use BOBETTE_AUTH_KEY or --key-file";
            return std::nullopt;
        } else {
            message = "unknown argument '" + std::string(arg) + "'\n" + kUsage;
            return std::nullopt;
        }
    }

    // A key file overrides BOBETTE_AUTH_KEY.
    if (keyFile) {
        if (!readKeyFile(*keyFile, cfg.authKey, message)) return std::nullopt;
        cfg.authKeySource = *keyFile;
    }

    // Nothing configured: look for auth_key.txt above the working directory,
    // then above the executable.
    if (cfg.authKey.empty()) {
        auto found = findKeyFileUpwards(std::filesystem::current_path());
        if (!found && argc > 0) {
            std::error_code ec;
            const auto exe = std::filesystem::absolute(argv[0], ec);
            if (!ec) found = findKeyFileUpwards(exe.parent_path());
        }
        if (found) {
            if (!readKeyFile(found->string(), cfg.authKey, message)) return std::nullopt;
            cfg.authKeySource = found->string();
        }
    }

    if (cfg.url.rfind("ws://", 0) != 0 && cfg.url.rfind("wss://", 0) != 0) {
        message = "URL must start with ws:// or wss://";
        return std::nullopt;
    }
    if (cfg.url.rfind("wss://", 0) == 0 && cfg.authKey.empty()) {
        message = "an auth key is required for " + cfg.url +
                  " (put it in auth_key.txt in the project root, set BOBETTE_AUTH_KEY, or pass --key-file)";
        return std::nullopt;
    }
    return cfg;
}

} // namespace bot
