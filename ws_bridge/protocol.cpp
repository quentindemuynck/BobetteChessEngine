#include "protocol.hpp"

#include <set>
#include <stdexcept>

namespace bot {

namespace {

// ---- Little-endian reader -------------------------------------------------------

class Reader {
public:
    explicit Reader(std::string_view data) : data_(data) {}

    bool u8(std::uint8_t& out) {
        if (remaining() < 1) return false;
        out = static_cast<std::uint8_t>(data_[pos_++]);
        return true;
    }

    bool u16(std::uint16_t& out) {
        if (remaining() < 2) return false;
        out = static_cast<std::uint16_t>(byteAt(0) | (byteAt(1) << 8));
        pos_ += 2;
        return true;
    }

    bool u64(std::uint64_t& out) {
        if (remaining() < 8) return false;
        out = 0;
        for (std::size_t i = 0; i < 8; ++i) {
            out |= static_cast<std::uint64_t>(byteAt(i)) << (8 * i);
        }
        pos_ += 8;
        return true;
    }

    bool bytes(std::size_t count, std::string& out) {
        if (remaining() < count) return false;
        out.assign(data_.substr(pos_, count));
        pos_ += count;
        return true;
    }

    bool str8(std::string& out) {
        std::uint8_t len = 0;
        return u8(len) && bytes(len, out);
    }

    bool str16(std::string& out) {
        std::uint16_t len = 0;
        return u16(len) && bytes(len, out);
    }

    [[nodiscard]] bool atEnd() const { return pos_ == data_.size(); }

private:
    [[nodiscard]] std::size_t remaining() const { return data_.size() - pos_; }
    [[nodiscard]] unsigned byteAt(std::size_t offset) const {
        return static_cast<unsigned char>(data_[pos_ + offset]);
    }

    std::string_view data_;
    std::size_t pos_ = 0;
};

// ---- Little-endian writer -------------------------------------------------------

void putU8(std::string& out, std::uint8_t v) { out.push_back(static_cast<char>(v)); }

void putU16(std::string& out, std::uint16_t v) {
    out.push_back(static_cast<char>(v & 0xFF));
    out.push_back(static_cast<char>((v >> 8) & 0xFF));
}

void putU64(std::string& out, std::uint64_t v) {
    for (int i = 0; i < 8; ++i) {
        out.push_back(static_cast<char>((v >> (8 * i)) & 0xFF));
    }
}

void putStr8(std::string& out, std::string_view s) {
    if (s.size() > 0xFF) throw std::invalid_argument("str8 longer than 255 bytes");
    putU8(out, static_cast<std::uint8_t>(s.size()));
    out.append(s);
}

void putStr16(std::string& out, std::string_view s) {
    if (s.size() > 0xFFFF) throw std::invalid_argument("str16 longer than 65535 bytes");
    putU16(out, static_cast<std::uint16_t>(s.size()));
    out.append(s);
}

// ---- Field validation -----------------------------------------------------------

bool isAscii(std::string_view s) {
    for (char c : s) {
        if (static_cast<unsigned char>(c) > 0x7F) return false;
    }
    return true;
}

bool hasControlChars(std::string_view s) {
    for (char c : s) {
        const auto u = static_cast<unsigned char>(c);
        if (u < 0x20 || u == 0x7F) return true;
    }
    return false;
}

bool isAllWhitespace(std::string_view s) {
    for (char c : s) {
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r' && c != '\f' && c != '\v') return false;
    }
    return true;
}

bool isIdentityChar(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
           c == '-' || c == '_' || c == '.';
}

std::optional<std::string> checkLabel(std::string_view what, std::string_view value,
                                      std::size_t maxBytes) {
    const std::string w(what);
    if (value.empty() || value.size() > maxBytes)
        return w + " must be 1-" + std::to_string(maxBytes) + " bytes";
    if (!isValidUtf8(value)) return w + " is not valid UTF-8";
    if (hasControlChars(value)) return w + " contains control characters";
    if (isAllWhitespace(value)) return w + " is only whitespace";
    return std::nullopt;
}

} // namespace

// ---- UTF-8 -----------------------------------------------------------------------

bool isValidUtf8(std::string_view text) {
    std::size_t i = 0;
    while (i < text.size()) {
        const auto c = static_cast<unsigned char>(text[i]);
        std::size_t extra = 0;
        std::uint32_t cp = 0;
        if (c < 0x80) { ++i; continue; }
        if ((c & 0xE0) == 0xC0) { extra = 1; cp = c & 0x1Fu; }
        else if ((c & 0xF0) == 0xE0) { extra = 2; cp = c & 0x0Fu; }
        else if ((c & 0xF8) == 0xF0) { extra = 3; cp = c & 0x07u; }
        else return false;

        if (i + extra >= text.size()) return false; // sequence runs past the end
        for (std::size_t k = 1; k <= extra; ++k) {
            const auto cc = static_cast<unsigned char>(text[i + k]);
            if ((cc & 0xC0) != 0x80) return false;
            cp = (cp << 6) | (cc & 0x3Fu);
        }
        // Reject overlong encodings, surrogates and out-of-range code points.
        if ((extra == 1 && cp < 0x80) || (extra == 2 && cp < 0x800) ||
            (extra == 3 && cp < 0x10000) || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
            return false;
        i += extra + 1;
    }
    return true;
}

// ---- Decoding --------------------------------------------------------------------

std::optional<ServerPacket> decodeServerPacket(std::string_view bytes, std::string& error) {
    if (bytes.size() > kMaxMessageBytes) {
        error = "message larger than " + std::to_string(kMaxMessageBytes) + " bytes";
        return std::nullopt;
    }

    Reader r(bytes);
    std::uint8_t op = 0;
    if (!r.u8(op)) {
        error = "empty message";
        return std::nullopt;
    }

    auto truncated = [&](const char* packet) {
        error = std::string("truncated ") + packet + " packet";
        return std::nullopt;
    };
    auto trailing = [&](const char* packet) {
        error = std::string("trailing bytes after ") + packet + " packet";
        return std::nullopt;
    };

    switch (op) {
        case opcode::Registered: {
            Registered p;
            if (!r.u8(p.version)) return truncated("registered");
            if (!r.atEnd()) return trailing("registered");
            return p;
        }
        case opcode::PredictMove: {
            PredictMove p;
            if (!r.u64(p.requestId) || !r.str8(p.mode) || !r.str16(p.fen))
                return truncated("predict_move");
            if (!r.atEnd()) return trailing("predict_move");
            if (!isValidUtf8(p.mode)) {
                error = "predict_move mode is not valid UTF-8";
                return std::nullopt;
            }
            if (p.fen.empty() || p.fen.size() > kMaxFenBytes || !isAscii(p.fen)) {
                error = "predict_move FEN must be 1-128 ASCII bytes";
                return std::nullopt;
            }
            return p;
        }
        case opcode::Accepted: {
            Accepted p;
            if (!r.u64(p.requestId)) return truncated("accepted");
            if (!r.atEnd()) return trailing("accepted");
            return p;
        }
        case opcode::Error: {
            ServerError p;
            if (!r.str16(p.reason)) return truncated("error");
            if (!r.atEnd()) return trailing("error");
            if (!isValidUtf8(p.reason)) {
                error = "error reason is not valid UTF-8";
                return std::nullopt;
            }
            return p;
        }
        default: {
            static constexpr char hex[] = "0123456789abcdef";
            error = std::string("unknown opcode 0x") + hex[op >> 4] + hex[op & 0xF];
            return std::nullopt;
        }
    }
}

// ---- Encoding --------------------------------------------------------------------

std::optional<std::string> validateRegistration(std::string_view identity,
                                                std::string_view name,
                                                const std::vector<std::string>& modes) {
    if (identity.empty() || identity.size() > 64)
        return std::string("identity must be 1-64 bytes");
    for (char c : identity) {
        if (!isIdentityChar(c)) return std::string("identity may only contain A-Z a-z 0-9 - _ .");
    }

    if (auto err = checkLabel("display name", name, 64)) return err;

    if (modes.empty() || modes.size() > 16) return std::string("need 1-16 modes");
    std::set<std::string> seen;
    for (const auto& mode : modes) {
        if (auto err = checkLabel("mode '" + mode + "'", mode, 32)) return err;
        if (!seen.insert(mode).second) return "duplicate mode '" + mode + "'";
    }
    return std::nullopt;
}

std::string encodeRegister(std::string_view identity,
                           std::string_view name,
                           const std::vector<std::string>& modes) {
    if (auto err = validateRegistration(identity, name, modes)) throw std::invalid_argument(*err);

    std::string out;
    putU8(out, opcode::Register);
    putU8(out, kProtocolVersion);
    putStr8(out, identity);
    putStr8(out, name);
    putU8(out, static_cast<std::uint8_t>(modes.size()));
    for (const auto& mode : modes) putStr8(out, mode);
    return out;
}

std::string encodeReceived(std::uint64_t requestId, std::uint64_t receivedUnixUs) {
    std::string out;
    out.reserve(17);
    putU8(out, opcode::Received);
    putU64(out, requestId);
    putU64(out, receivedUnixUs);
    return out;
}

std::string encodePong(std::uint64_t requestId) {
    std::string out;
    out.reserve(9);
    putU8(out, opcode::Pong);
    putU64(out, requestId);
    return out;
}

std::string encodeMove(std::uint64_t requestId, std::string_view resultingFen) {
    std::string out;
    out.reserve(11 + resultingFen.size());
    putU8(out, opcode::Move);
    putU64(out, requestId);
    putStr16(out, resultingFen);
    return out;
}

} // namespace bot
