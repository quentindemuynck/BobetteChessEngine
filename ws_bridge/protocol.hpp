#pragma once
// Binary application protocol v2 for the chess bot backend (see PROTOCOL.md).
// Pure encoding/decoding: no sockets, no threads, so it is unit-testable.

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace bot {

inline constexpr std::uint8_t kProtocolVersion = 2;
inline constexpr std::size_t kMaxFenBytes = 128;
inline constexpr std::size_t kMaxMessageBytes = 2048;

namespace opcode {
inline constexpr std::uint8_t Register    = 0x01;
inline constexpr std::uint8_t Registered  = 0x81;
inline constexpr std::uint8_t PredictMove = 0x10;
inline constexpr std::uint8_t Received    = 0x11;
inline constexpr std::uint8_t Pong        = 0x12;
inline constexpr std::uint8_t Move        = 0x13;
inline constexpr std::uint8_t Accepted    = 0x14;
inline constexpr std::uint8_t Error       = 0xff;
} // namespace opcode

// ---- Backend -> bot packets ---------------------------------------------------

struct Registered {
    std::uint8_t version = 0;
};

struct PredictMove {
    std::uint64_t requestId = 0;
    std::string mode;
    std::string fen;
};

struct Accepted {
    std::uint64_t requestId = 0;
};

struct ServerError {
    std::string reason;
};

using ServerPacket = std::variant<Registered, PredictMove, Accepted, ServerError>;

/// Decodes one binary WebSocket message from the backend.
/// Returns std::nullopt and fills `error` on truncated data, trailing bytes,
/// unknown opcodes, invalid UTF-8 or a non-ASCII / oversized FEN.
std::optional<ServerPacket> decodeServerPacket(std::string_view bytes, std::string& error);

// ---- Bot -> backend packets ---------------------------------------------------

/// Checks identity / display name / modes against the registration rules.
/// Returns an error description, or std::nullopt if everything is valid.
std::optional<std::string> validateRegistration(std::string_view identity,
                                                std::string_view name,
                                                const std::vector<std::string>& modes);

/// Encoders. Each returns exactly one application packet (one binary message).
/// encodeRegister throws std::invalid_argument if validateRegistration fails.
std::string encodeRegister(std::string_view identity,
                           std::string_view name,
                           const std::vector<std::string>& modes);
std::string encodeReceived(std::uint64_t requestId, std::uint64_t receivedUnixUs);
std::string encodePong(std::uint64_t requestId);
std::string encodeMove(std::uint64_t requestId, std::string_view resultingFen);

// ---- Helpers ------------------------------------------------------------------

bool isValidUtf8(std::string_view text);

} // namespace bot
