
#include "engine/openingbook.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <random>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace engine {

    namespace {

        using MoveList = std::vector<std::string_view>;
        using Book = std::unordered_map<std::uint64_t, MoveList>;

        constexpr std::string_view START_FEN =
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

        // Each sequence represents a complete opening variation.
        // Every intermediate position is automatically added to the book.
        //
        // Add more variations here without manually calculating FENs.
        const std::vector<MoveList>& openingVariations() {
            static const std::vector<MoveList> lines = {

                // ============================================
                // OPEN GAMES — 1.e4 e5
                // ============================================

                // Ruy Lopez — Morphy Defense
                {"e2e4", "e7e5", "g1f3", "b8c6",
                 "f1b5", "a7a6", "b5a4", "g8f6",
                 "e1g1", "f8e7", "f1e1", "b7b5",
                 "a4b3", "e8g8"},

                 // Ruy Lopez — Berlin Defense
                 {"e2e4", "e7e5", "g1f3", "b8c6",
                  "f1b5", "g8f6", "e1g1", "f6e4",
                  "d2d4", "e4d6"},

                  // Italian Game — Giuoco Piano
                  {"e2e4", "e7e5", "g1f3", "b8c6",
                   "f1c4", "f8c5", "c2c3", "g8f6",
                   "d2d4", "e5d4", "c3d4", "c5b4"},

                   // Italian Game — Giuoco Pianissimo
                   {"e2e4", "e7e5", "g1f3", "b8c6",
                    "f1c4", "f8c5", "d2d3", "g8f6",
                    "c2c3", "d7d6", "e1g1", "e8g8"},

                    // Two Knights Defense
                    {"e2e4", "e7e5", "g1f3", "b8c6",
                     "f1c4", "g8f6", "d2d3", "f8c5",
                     "c2c3", "d7d6"},

                     // Scotch Game
                     {"e2e4", "e7e5", "g1f3", "b8c6",
                      "d2d4", "e5d4", "f3d4", "g8f6",
                      "d4c6", "b7c6", "e4e5", "d8e7"},

                      // Four Knights Game
                      {"e2e4", "e7e5", "g1f3", "b8c6",
                       "b1c3", "g8f6", "f1b5", "f8b4",
                       "e1g1", "e8g8"},

                       // Vienna Game
                       {"e2e4", "e7e5", "b1c3", "g8f6",
                        "f2f4", "d7d5", "f4e5", "f6e4"},

                        // King's Gambit
                        {"e2e4", "e7e5", "f2f4", "e5f4",
                         "g1f3", "g7g5", "h2h4", "g5g4",
                         "f3e5"},

                         // Petroff Defense
                         {"e2e4", "e7e5", "g1f3", "g8f6",
                          "f3e5", "d7d6", "e5f3", "f6e4",
                          "d2d4", "d6d5"},

                          // Philidor Defense
                          {"e2e4", "e7e5", "g1f3", "d7d6",
                           "d2d4", "g8f6", "b1c3", "b8d7"},

                           // ============================================
                           // SICILIAN DEFENSE — 1.e4 c5
                           // ============================================

                           // Najdorf
                           {"e2e4", "c7c5", "g1f3", "d7d6",
                            "d2d4", "c5d4", "f3d4", "g8f6",
                            "b1c3", "a7a6", "f1e2", "e7e5",
                            "d4b3", "f8e7"},

                            // Dragon
                            {"e2e4", "c7c5", "g1f3", "d7d6",
                             "d2d4", "c5d4", "f3d4", "g8f6",
                             "b1c3", "g7g6", "c1e3", "f8g7",
                             "f2f3", "e8g8", "d1d2"},

                             // Classical Sicilian
                             {"e2e4", "c7c5", "g1f3", "d7d6",
                              "d2d4", "c5d4", "f3d4", "g8f6",
                              "b1c3", "b8c6", "c1g5", "e7e6"},

                              // Scheveningen
                              {"e2e4", "c7c5", "g1f3", "d7d6",
                               "d2d4", "c5d4", "f3d4", "g8f6",
                               "b1c3", "e7e6", "f1e2", "f8e7"},

                               // Sveshnikov
                               {"e2e4", "c7c5", "g1f3", "b8c6",
                                "d2d4", "c5d4", "f3d4", "g8f6",
                                "b1c3", "e7e5", "d4b5", "d7d6"},

                                // Accelerated Dragon
                                {"e2e4", "c7c5", "g1f3", "b8c6",
                                 "d2d4", "c5d4", "f3d4", "g7g6",
                                 "b1c3", "f8g7", "c1e3", "g8f6"},

                                 // Alapin Sicilian
                                 {"e2e4", "c7c5", "c2c3", "d7d5",
                                  "e4d5", "d8d5", "d2d4", "g8f6",
                                  "g1f3", "c8g4"},

                                  // Closed Sicilian
                                  {"e2e4", "c7c5", "b1c3", "b8c6",
                                   "g2g3", "g7g6", "f1g2", "f8g7",
                                   "d2d3", "d7d6"},

                                   // ============================================
                                   // FRENCH DEFENSE
                                   // ============================================

                                   // French — Advance Variation
                                   {"e2e4", "e7e6", "d2d4", "d7d5",
                                    "e4e5", "c7c5", "c2c3", "b8c6",
                                    "g1f3", "d8b6"},

                                    // French — Tarrasch
                                    {"e2e4", "e7e6", "d2d4", "d7d5",
                                     "b1d2", "g8f6", "e4e5", "f6d7",
                                     "f1d3", "c7c5"},

                                     // French — Winawer
                                     {"e2e4", "e7e6", "d2d4", "d7d5",
                                      "b1c3", "f8b4", "e4e5", "c7c5",
                                      "a2a3", "b4c3", "b2c3"},

                                      // French — Classical
                                      {"e2e4", "e7e6", "d2d4", "d7d5",
                                       "b1c3", "g8f6", "c1g5", "f8e7",
                                       "e4e5", "f6d7"},

                                       // ============================================
                                       // CARO-KANN DEFENSE
                                       // ============================================

                                       // Classical Caro-Kann
                                       {"e2e4", "c7c6", "d2d4", "d7d5",
                                        "b1c3", "d5e4", "c3e4", "c8f5",
                                        "e4g3", "f5g6", "h2h4", "h7h6"},

                                        // Advance Caro-Kann
                                        {"e2e4", "c7c6", "d2d4", "d7d5",
                                         "e4e5", "c8f5", "g1f3", "e7e6",
                                         "f1e2", "c6c5"},

                                         // Exchange Caro-Kann
                                         {"e2e4", "c7c6", "d2d4", "d7d5",
                                          "e4d5", "c6d5", "f1d3", "b8c6",
                                          "c2c3", "g8f6"},

                                          // Panov Attack
                                          {"e2e4", "c7c6", "d2d4", "d7d5",
                                           "e4d5", "c6d5", "c2c4", "g8f6",
                                           "b1c3", "e7e6"},

                                           // ============================================
                                           // OTHER 1.e4 DEFENSES
                                           // ============================================

                                           // Scandinavian Defense
                                           {"e2e4", "d7d5", "e4d5", "d8d5",
                                            "b1c3", "d5a5", "d2d4", "g8f6",
                                            "g1f3", "c8f5"},

                                            // Pirc Defense
                                            {"e2e4", "d7d6", "d2d4", "g8f6",
                                             "b1c3", "g7g6", "g1f3", "f8g7",
                                             "f1e2", "e8g8"},

                                             // Modern Defense
                                             {"e2e4", "g7g6", "d2d4", "f8g7",
                                              "b1c3", "d7d6", "g1f3", "g8f6"},

                                              // Alekhine Defense
                                              {"e2e4", "g8f6", "e4e5", "f6d5",
                                               "d2d4", "d7d6", "g1f3", "c8g4"},

                                               // ============================================
                                               // QUEEN'S GAMBIT — 1.d4 d5
                                               // ============================================

                                               // Queen's Gambit Declined
                                               {"d2d4", "d7d5", "c2c4", "e7e6",
                                                "b1c3", "g8f6", "c1g5", "f8e7",
                                                "e2e3", "e8g8", "g1f3", "b8d7"},

                                                // Queen's Gambit Accepted
                                                {"d2d4", "d7d5", "c2c4", "d5c4",
                                                 "g1f3", "g8f6", "e2e3", "e7e6",
                                                 "f1c4", "c7c5", "e1g1"},

                                                 // Slav Defense
                                                 {"d2d4", "d7d5", "c2c4", "c7c6",
                                                  "g1f3", "g8f6", "b1c3", "d5c4",
                                                  "a2a4", "c8f5"},

                                                  // Semi-Slav Defense
                                                  {"d2d4", "d7d5", "c2c4", "c7c6",
                                                   "g1f3", "g8f6", "b1c3", "e7e6",
                                                   "e2e3", "b8d7"},

                                                   // Tarrasch Defense
                                                   {"d2d4", "d7d5", "c2c4", "e7e6",
                                                    "b1c3", "c7c5", "c4d5", "e6d5",
                                                    "g1f3", "b8c6"},

                                                    // ============================================
                                                    // INDIAN DEFENSES — 1.d4 Nf6
                                                    // ============================================

                                                    // King's Indian Defense
                                                    {"d2d4", "g8f6", "c2c4", "g7g6",
                                                     "b1c3", "f8g7", "e2e4", "d7d6",
                                                     "g1f3", "e8g8", "f1e2", "e7e5",
                                                     "e1g1"},

                                                     // Nimzo-Indian Defense
                                                     {"d2d4", "g8f6", "c2c4", "e7e6",
                                                      "b1c3", "f8b4", "e2e3", "e8g8",
                                                      "f1d3", "d7d5", "g1f3"},

                                                      // Queen's Indian Defense
                                                      {"d2d4", "g8f6", "c2c4", "e7e6",
                                                       "g1f3", "b7b6", "g2g3", "c8b7",
                                                       "f1g2", "f8e7", "e1g1"},

                                                       // Bogo-Indian Defense
                                                       {"d2d4", "g8f6", "c2c4", "e7e6",
                                                        "g1f3", "f8b4", "c1d2", "d8e7",
                                                        "g2g3", "b8c6"},

                                                        // Grunfeld Defense
                                                        {"d2d4", "g8f6", "c2c4", "g7g6",
                                                         "b1c3", "d7d5", "c4d5", "f6d5",
                                                         "e2e4", "d5c3", "b2c3", "f8g7"},

                                                         // Benoni Defense
                                                         {"d2d4", "g8f6", "c2c4", "c7c5",
                                                          "d4d5", "e7e6", "b1c3", "e6d5",
                                                          "c4d5", "d7d6"},

                                                          // Dutch Defense
                                                          {"d2d4", "f7f5", "g2g3", "g8f6",
                                                           "f1g2", "e7e6", "g1f3", "f8e7",
                                                           "e1g1", "e8g8"},

                                                           // ============================================
                                                           // LONDON SYSTEM & OTHER 1.d4 OPENINGS
                                                           // ============================================

                                                           // London System
                                                           {"d2d4", "d7d5", "c1f4", "g8f6",
                                                            "e2e3", "e7e6", "g1f3", "f8d6",
                                                            "f4g3", "e8g8", "f1d3"},

                                                            // London vs King's Indian setup
                                                            {"d2d4", "g8f6", "c1f4", "g7g6",
                                                             "e2e3", "f8g7", "g1f3", "e8g8",
                                                             "f1e2", "d7d6"},

                                                             // Colle System
                                                             {"d2d4", "d7d5", "g1f3", "g8f6",
                                                              "e2e3", "e7e6", "f1d3", "c7c5",
                                                              "c2c3", "b8c6"},

                                                              // Catalan Opening
                                                              {"d2d4", "g8f6", "c2c4", "e7e6",
                                                               "g2g3", "d7d5", "f1g2", "f8e7",
                                                               "g1f3", "e8g8", "e1g1"},

                                                               // ============================================
                                                               // ENGLISH OPENING — 1.c4
                                                               // ============================================

                                                               // English — Reversed Sicilian
                                                               {"c2c4", "e7e5", "b1c3", "g8f6",
                                                                "g2g3", "d7d5", "c4d5", "f6d5",
                                                                "f1g2", "d5b6"},

                                                                // English — Symmetrical
                                                                {"c2c4", "c7c5", "b1c3", "b8c6",
                                                                 "g2g3", "g7g6", "f1g2", "f8g7",
                                                                 "g1f3", "e7e6"},

                                                                 // English — Four Knights
                                                                 {"c2c4", "e7e5", "b1c3", "g8f6",
                                                                  "g1f3", "b8c6", "g2g3", "f8b4",
                                                                  "f1g2", "e8g8"},

                                                                  // ============================================
                                                                  // RETI & FLANK OPENINGS
                                                                  // ============================================

                                                                  // Reti Opening
                                                                  {"g1f3", "d7d5", "c2c4", "e7e6",
                                                                   "g2g3", "g8f6", "f1g2", "f8e7",
                                                                   "e1g1", "e8g8"},

                                                                   // King's Indian Attack
                                                                   {"g1f3", "d7d5", "g2g3", "g8f6",
                                                                    "f1g2", "e7e6", "e1g1", "f8e7",
                                                                    "d2d3", "e8g8", "b1d2"},

                                                                    // Bird Opening
                                                                    {"f2f4", "d7d5", "g1f3", "g8f6",
                                                                     "e2e3", "g7g6", "b2b3", "f8g7"},

                                                                     // ============================================
                                                                     // ADDITIONAL TRANSPOSITIONS
                                                                     // ============================================

                                                                     // Queen's Gambit via 1.Nf3
                                                                     {"g1f3", "d7d5", "d2d4", "g8f6",
                                                                      "c2c4", "e7e6", "b1c3", "f8e7"},

                                                                      // King's Indian via 1.c4
                                                                      {"c2c4", "g8f6", "b1c3", "g7g6",
                                                                       "d2d4", "f8g7", "e2e4", "d7d6"},

                                                                       // Nimzo-Indian via 1.c4
                                                                       {"c2c4", "g8f6", "b1c3", "e7e6",
                                                                        "d2d4", "f8b4", "e2e3", "e8g8"},

            };

            return lines;
        }

        // Adds a move without duplicating it.
        // Different variations can reach the same position.
        void addMove(Book& book,
            std::uint64_t hash,
            std::string_view move) {
            auto& moves = book[hash];

            for (const auto existing : moves) {
                if (existing == move)
                    return;
            }

            moves.push_back(move);
        }

        Book buildBook() {
            Book book;
            book.reserve(1024);

            for (const auto& variation : openingVariations()) {
                chess::Board board;
                board.setFen(START_FEN);

                for (const auto moveUci : variation) {
                    const chess::Move move =
                        chess::uci::uciToMove(board, moveUci);

                    // uciToMove only checks the string's *format* — it happily
                    // builds a move for a square with no piece on it, or one
                    // that can't legally move there, instead of rejecting it.
                    // Checking against the real legal move list is what
                    // actually catches a typo'd entry (e.g. the wrong square)
                    // before it gets stored and silently corrupts play later.
                    chess::Movelist legal;
                    chess::movegen::legalmoves(legal, board);
                    const bool isLegal =
                        std::find(legal.begin(), legal.end(), move) != legal.end();

                    if (move == chess::Move::NO_MOVE || !isLegal) {
                        // Invalid book entry: stop this variation.
                        break;
                    }

                    // Store the move for the CURRENT position.
                    addMove(book, board.hash(), moveUci);

                    // Advance to the next position.
                    board.makeMove(move);
                }
            }

            return book;
        }

    } // namespace

    std::optional<chess::Move>
        lookupBookMove(const chess::Board& board) {
        static const Book book = buildBook();

        const auto it = book.find(board.hash());

        if (it == book.end())
            return std::nullopt;

        const auto& candidates = it->second;

        if (candidates.empty())
            return std::nullopt;

        static thread_local std::mt19937 rng{
            std::random_device{}()
        };

        // Randomly select a book move.
        std::uniform_int_distribution<std::size_t> dist(
            0, candidates.size() - 1
        );

        // Try other candidates if one is invalid.
        const std::size_t start = dist(rng);

        chess::Movelist legal;
        chess::movegen::legalmoves(legal, board);

        for (std::size_t i = 0; i < candidates.size(); ++i) {
            const auto uci =
                candidates[(start + i) % candidates.size()];

            const chess::Move move =
                chess::uci::uciToMove(board, uci);

            // Same reasoning as buildBook(): uciToMove alone only checks the
            // string's format, not whether it's actually legal here.
            if (move != chess::Move::NO_MOVE &&
                std::find(legal.begin(), legal.end(), move) != legal.end())
                return move;
        }

        return std::nullopt;
    }

} // namespace engine
