#include "uci.h"
#include "position.h"
#include "movegen.h"
#include "move.h"
#include <iostream>
#include <string>
#include <sstream>

// ---------------------------------------------------------------------------
//  Global Position
// ---------------------------------------------------------------------------
static Position g_pos;

// ---------------------------------------------------------------------------
//  Parse "position ..." command
// ---------------------------------------------------------------------------
static void parse_position(const std::string& args) {
    std::istringstream iss(args);
    std::string token;
    iss >> token;

    if (token == "startpos") {
        g_pos.set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

        std::string move_token;
        while (iss >> move_token) {
            if (move_token == "moves") {
                std::string m;
                while (iss >> m) {
                    Square from = static_cast<Square>((m[0] - 'a') + (m[1] - '1') * 8);
                    Square to = static_cast<Square>((m[2] - 'a') + (m[3] - '1') * 8);

                    PieceType prom = PieceType::NONE;
                    int flag = MOVE_FLAG_NORMAL;

                    if (m.size() == 5) {
                        char pc = std::tolower(m[4]);
                        switch (pc) {
                            case 'n': prom = PieceType::KNIGHT; break;
                            case 'b': prom = PieceType::BISHOP; break;
                            case 'r': prom = PieceType::ROOK;   break;
                            case 'q': prom = PieceType::QUEEN;  break;
                            default:  break;
                        }
                    }

                    Move mv = make_move(from, to, flag, prom);
                    g_pos.make_move(mv);
                }
            }
        }
    } else if (token == "fen") {
        std::string fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

        for (int i = 0; i < 6; ++i) {
            iss >> token;
            fen += " " + token;
        }

        g_pos.set_fen(fen);

        std::string move_token;
        while (iss >> move_token) {
            if (move_token == "moves") {
                std::string m;
                while (iss >> m) {
                    Square from = static_cast<Square>((m[0] - 'a') + (m[1] - '1') * 8);
                    Square to = static_cast<Square>((m[2] - 'a') + (m[3] - '1') * 8);

                    PieceType prom = PieceType::NONE;
                    int flag = MOVE_FLAG_NORMAL;

                    if (m.size() == 5) {
                        char pc = std::tolower(m[4]);
                        switch (pc) {
                            case 'n': prom = PieceType::KNIGHT; break;
                            case 'b': prom = PieceType::BISHOP; break;
                            case 'r': prom = PieceType::ROOK;   break;
                            case 'q': prom = PieceType::QUEEN;  break;
                            default:  break;
                        }
                    }

                    Move mv = make_move(from, to, flag, prom);
                    g_pos.make_move(mv);
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
//  Parse "go ..." command (stub: pick first legal move)
// ---------------------------------------------------------------------------
static void parse_go(const std::string& args) {
    (void)args;

    MoveList list;
    MoveGen::generate_legal_moves(g_pos, list);

    for (int i = 0; i < list.size(); ++i) {
        Position next_pos = g_pos;
        if (next_pos.make_move(list.moves[i])) {
            std::cout << "bestmove " << move_to_str(list.moves[i]) << std::endl;
            std::cout.flush();
            return;
        }
    }

    std::cout << "bestmove 0000" << std::endl;
    std::cout.flush();
}

// ---------------------------------------------------------------------------
//  UCI Main Loop
// ---------------------------------------------------------------------------
void uci_loop() {
    std::string line;

    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;

        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;

        if (cmd == "uci") {
            std::cout << "id name sic" << std::endl;
            std::cout << "id author sic-team" << std::endl;
            std::cout << "uciok" << std::endl;
        } else if (cmd == "isready") {
            std::cout << "readyok" << std::endl;
        } else if (cmd == "ucinewgame") {
            // TT / history cleared here (future)
        } else if (cmd == "position") {
            std::string rest;
            std::getline(iss, rest);
            parse_position(rest);
        } else if (cmd == "go") {
            std::string rest;
            std::getline(iss, rest);
            parse_go(rest);
        } else if (cmd == "quit") {
            return;
        }

        std::cout.flush();
    }
}
