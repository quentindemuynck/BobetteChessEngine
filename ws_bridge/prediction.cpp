#include "prediction.hpp"

#include <algorithm>
#include <chrono>
#include <iostream>

#include "engine/search.hpp"

namespace bot {

engine::TimeLimit timeLimitForMode(std::string_view mode) {
    using namespace std::chrono_literals;
    engine::TimeLimit limit;
    if (mode == "fast") {
        limit.moveTime = 200ms;
        limit.depth = 2;
    } else if (mode == "deep") {
        limit.moveTime = 900000ms;
        limit.depth = 6;
    } else {
        limit.moveTime = 1000ms; // unknown mode: sensible default
    }
    return limit;
}

bool loadFen(chess::Board& board, std::string_view fen, std::string& error) {
    if (!board.setFen(fen)) {
        error = "malformed FEN";
        return false;
    }
    const auto whiteKings = board.pieces(chess::PieceType::KING, chess::Color::WHITE).count();
    const auto blackKings = board.pieces(chess::PieceType::KING, chess::Color::BLACK).count();
    if (whiteKings != 1 || blackKings != 1) {
        error = "FEN must have exactly one king per side";
        return false;
    }
    return true;
}

std::optional<std::string> resultingFen(const chess::Board& root, chess::Move move) {
    chess::Movelist legal;
    chess::movegen::legalmoves(legal, root);
    if (std::find(legal.begin(), legal.end(), move) == legal.end()) {
        return std::nullopt;
    }

    chess::Board next = root; // never touch the root: apply exactly one move to a copy
    next.makeMove(move);
    return next.getFen(true); // all six fields, including both counters
}

std::optional<std::string> answerPrediction(std::string_view fen,
                                            std::string_view mode,
                                            std::string& error) {
    chess::Board board;
    if (!loadFen(board, fen, error)) {
        return std::nullopt;
    }

    std::vector<engine::MoveEval> debug;
    const chess::Move best = engine::findBestMove(board, timeLimitForMode(mode), &debug);

    // Only the move actually chosen, plus the line it expects to follow.
    const auto chosen = std::find_if(debug.begin(), debug.end(),
        [&](const engine::MoveEval& e) { return e.move == best; });
    if (chosen != debug.end()) {
        std::cerr << "  [eval] chosen " << chess::uci::moveToUci(chosen->move)
                   << " score=" << chosen->score
                   << " (material=" << chosen->materialScore << " mobility=" << chosen->mobilityScore << ")"
                   << (chosen->timedOut ? " [TIMED OUT]" : "") << " pv:";
        for (chess::Move m : chosen->pv) std::cerr << " " << chess::uci::moveToUci(m);
        std::cerr << "\n";
    }

    if (best == chess::Move::NO_MOVE) {
        error = "no legal move in position (checkmate or stalemate)";
        return std::nullopt;
    }

    auto reply = resultingFen(board, best);
    if (!reply) {
        error = "engine returned an illegal move: " + chess::uci::moveToUci(best);
    }
    return reply;
}

} // namespace bot
