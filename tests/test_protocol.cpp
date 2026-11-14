#include <doctest/doctest.h>

#include <string>
#include <variant>

#include <chess.hpp>

#include "prediction.hpp"
#include "protocol.hpp"

namespace {

/// "01 02 0a" -> raw bytes. Whitespace is ignored.
std::string hex(std::string_view text) {
    std::string out;
    int nibble = -1;
    for (char c : text) {
        int v = -1;
        if (c >= '0' && c <= '9') v = c - '0';
        else if (c >= 'a' && c <= 'f') v = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') v = c - 'A' + 10;
        if (v < 0) continue;
        if (nibble < 0) {
            nibble = v;
        } else {
            out.push_back(static_cast<char>((nibble << 4) | v));
            nibble = -1;
        }
    }
    return out;
}

/// Plays `uci` from `fen` and returns the resulting FEN.
std::string after(std::string_view fen, std::string_view uci) {
    chess::Board board;
    std::string error;
    REQUIRE(bot::loadFen(board, fen, error));
    const auto move = chess::uci::uciToMove(board, std::string(uci));
    const auto result = bot::resultingFen(board, move);
    REQUIRE(result.has_value());
    return *result;
}

} // namespace

// ---- Fixtures copied from the "Example payloads" section of PROTOCOL.md --------

TEST_CASE("encode register matches the doc example") {
    CHECK(bot::encodeRegister("demo", "Demo", {"fast"}) ==
          hex("01 02 04 64 65 6d 6f 04 44 65 6d 6f 01 04 66 61 73 74"));
}

TEST_CASE("decode registered") {
    std::string error;
    const auto packet = bot::decodeServerPacket(hex("81 02"), error);
    REQUIRE(packet.has_value());
    CHECK(std::get<bot::Registered>(*packet).version == 2);
}

TEST_CASE("decode predict_move matches the doc example") {
    const auto bytes = hex(
        "10 01 00 00 00 00 00 00 00 04 66 61 73 74 38 00"
        "72 6e 62 71 6b 62 6e 72 2f 70 70 70 70 70 70 70 70 2f 38 2f 38 2f 38 2f"
        "38 2f 50 50 50 50 50 50 50 50 2f 52 4e 42 51 4b 42 4e 52 20 77 20 4b 51"
        "6b 71 20 2d 20 30 20 31");
    std::string error;
    const auto packet = bot::decodeServerPacket(bytes, error);
    REQUIRE_MESSAGE(packet.has_value(), error);
    const auto& p = std::get<bot::PredictMove>(*packet);
    CHECK(p.requestId == 1);
    CHECK(p.mode == "fast");
    CHECK(p.fen == "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

TEST_CASE("encode received / pong / move match the doc examples") {
    CHECK(bot::encodeReceived(1, 1700000000000000ULL) ==
          hex("11 01 00 00 00 00 00 00 00 00 40 1e 18 24 0a 06 00"));
    CHECK(bot::encodePong(1) == hex("12 01 00 00 00 00 00 00 00"));

    const std::string fen = "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1";
    const auto move = bot::encodeMove(1, fen);
    CHECK(move.size() == 11 + fen.size());
    CHECK(move == hex("13 01 00 00 00 00 00 00 00 3b 00"
                      "72 6e 62 71 6b 62 6e 72 2f 70 70 70 70 70 70 70 70 2f 38 2f 38 2f 34 50"
                      "33 2f 38 2f 50 50 50 50 31 50 50 50 2f 52 4e 42 51 4b 42 4e 52 20 62 20"
                      "4b 51 6b 71 20 65 33 20 30 20 31"));
}

TEST_CASE("decode accepted and error") {
    std::string error;
    auto accepted = bot::decodeServerPacket(hex("14 01 00 00 00 00 00 00 00"), error);
    REQUIRE(accepted.has_value());
    CHECK(std::get<bot::Accepted>(*accepted).requestId == 1);

    auto err = bot::decodeServerPacket(hex("ff 03 00 62 61 64"), error);
    REQUIRE(err.has_value());
    CHECK(std::get<bot::ServerError>(*err).reason == "bad");
}

TEST_CASE("full 64-bit request ids survive") {
    std::string error;
    auto packet = bot::decodeServerPacket(hex("14 ff ee dd cc bb aa 99 88"), error);
    REQUIRE(packet.has_value());
    CHECK(std::get<bot::Accepted>(*packet).requestId == 0x8899aabbccddeeffULL);
    CHECK(bot::encodePong(0x8899aabbccddeeffULL) == hex("12 ff ee dd cc bb aa 99 88"));
}

TEST_CASE("decoder rejects bad input") {
    std::string error;
    CHECK_FALSE(bot::decodeServerPacket("", error));
    CHECK_FALSE(bot::decodeServerPacket(hex("81"), error));                      // truncated
    CHECK_FALSE(bot::decodeServerPacket(hex("81 02 00"), error));                // trailing
    CHECK_FALSE(bot::decodeServerPacket(hex("14 01 00 00"), error));             // truncated
    CHECK_FALSE(bot::decodeServerPacket(hex("42"), error));                      // unknown opcode
    CHECK_FALSE(bot::decodeServerPacket(hex("10 01 00 00 00 00 00 00 00 01 ff 01 00 38"),
                                        error));                                 // bad UTF-8 mode
    CHECK_FALSE(bot::decodeServerPacket(hex("10 01 00 00 00 00 00 00 00 00 00 00"),
                                        error));                                 // empty FEN
}

TEST_CASE("registration validation") {
    CHECK_FALSE(bot::validateRegistration("bobette", "Bobette", {"fast", "deep"}));
    CHECK(bot::validateRegistration("", "x", {"fast"}));
    CHECK(bot::validateRegistration("has space", "x", {"fast"}));
    CHECK(bot::validateRegistration(std::string(65, 'a'), "x", {"fast"}));
    CHECK(bot::validateRegistration("ok", "   ", {"fast"}));
    CHECK(bot::validateRegistration("ok", "x", {}));
    CHECK(bot::validateRegistration("ok", "x", {"fast", "fast"}));
    CHECK(bot::validateRegistration("ok", "x", {"tab\there"}));
}

// ---- Resulting FEN: everything the backend checks -----------------------------

TEST_CASE("resulting FEN: double push sets en passant, counters") {
    CHECK(after("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", "e2e4") ==
          "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1");
}

TEST_CASE("resulting FEN: quiet move increments halfmove, black move increments fullmove") {
    CHECK(after("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1", "g8f6") ==
          "rnbqkb1r/pppppppp/5n2/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 1 2");
}

TEST_CASE("resulting FEN: castling moves both pieces and drops rights") {
    CHECK(after("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 3 10", "e1g1") ==
          "r3k2r/8/8/8/8/8/8/R4RK1 b kq - 4 10");
    CHECK(after("r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 3 10", "e8c8") ==
          "2kr3r/8/8/8/8/8/8/R3K2R w KQ - 4 11");
}

TEST_CASE("resulting FEN: capturing a rook on its home square removes that right") {
    CHECK(after("r3k2r/8/8/8/8/8/6B1/R3K2R w KQkq - 0 1", "g2a8") ==
          "B3k2r/8/8/8/8/8/8/R3K2R b KQk - 0 1");
}

TEST_CASE("resulting FEN: en passant removes the captured pawn") {
    CHECK(after("4k3/8/8/3Pp3/8/8/8/4K3 w - e6 0 2", "d5e6") ==
          "4k3/8/4P3/8/8/8/8/4K3 b - - 0 2");
}

TEST_CASE("resulting FEN: promotion and underpromotion") {
    CHECK(after("4k3/1P6/8/8/8/8/8/4K3 w - - 5 40", "b7b8q") == "1Q2k3/8/8/8/8/8/8/4K3 b - - 0 40");
    CHECK(after("4k3/1P6/8/8/8/8/8/4K3 w - - 5 40", "b7b8n") == "1N2k3/8/8/8/8/8/8/4K3 b - - 0 40");
}

TEST_CASE("resulting FEN: illegal move is refused") {
    chess::Board board;
    std::string error;
    // Black rook on d8 controls the d-file, so Ke1-d1 would walk into check.
    REQUIRE(bot::loadFen(board, "3rk3/8/8/8/8/8/8/4K3 w - - 0 1", error));
    const chess::Move intoCheck = chess::Move::make(chess::Square::SQ_E1, chess::Square::SQ_D1);
    CHECK_FALSE(bot::resultingFen(board, intoCheck).has_value());
}

TEST_CASE("answerPrediction gives a legal successor and refuses mate") {
    std::string error;
    auto reply = bot::answerPrediction(
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", "fast", error);
    REQUIRE_MESSAGE(reply.has_value(), error);
    CHECK(reply->find(" b ") != std::string::npos);

    // Fool's mate: white to move, no legal moves.
    CHECK_FALSE(bot::answerPrediction(
        "rnb1kbnr/pppp1ppp/8/4p3/6Pq/5P2/PPPPP2P/RNBQKBNR w KQkq - 1 3", "fast", error));
    CHECK_FALSE(bot::answerPrediction("not a fen", "fast", error));
}
