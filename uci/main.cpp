// engine_uci: speaks the UCI protocol over stdin/stdout.
// Protocol reference: https://www.shredderchess.com/download/div/uci.zip

#include <iostream>
#include <sstream>
#include <string>

#include <chess.hpp>

#include "engine/search.hpp"
#include "engine/time_manager.hpp"

int main() {
    // UCI GUIs expect unbuffered, line-oriented output.
    std::ios::sync_with_stdio(false);
    std::cout.setf(std::ios::unitbuf);

    chess::Board board; // starts at the standard initial position

    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream tokens(line);
        std::string command;
        tokens >> command;

        if (command == "uci") {
            // TODO: print "id name ...", "id author ...", any "option ..." lines, then "uciok".
        } else if (command == "isready") {
            // TODO: print "readyok" once the engine is ready.
        } else if (command == "ucinewgame") {
            // TODO: reset any per-game state (hash tables, history, ...).
        } else if (command == "position") {
            // TODO: parse "position startpos [moves ...]" or
            //       "position fen <fen> [moves ...]" and update `board`.
        } else if (command == "go") {
            // TODO: parse wtime/btime/winc/binc/movestogo/depth/nodes/movetime/infinite
            //       into an engine::TimeLimit, call engine::findBestMove(board, limit),
            //       and print "bestmove <move>" using chess::uci::moveToUci().
        } else if (command == "stop") {
            // TODO: stop an ongoing search and print its best move.
        } else if (command == "quit") {
            break;
        }
        // Unknown commands are ignored, as the UCI spec requires.
    }

    return 0;
}
