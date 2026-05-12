#include "../include/uci.h"
#include "../include/position.h"
#include "../include/movegen.h"
#include "../include/move.h"
#include "../include/search.h"
#include "../include/perft.h"
#include "../include/timeman.h"
#include "../include/tt.h"
#include "../include/nnue.h"
#include "../include/thread.h"
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
//  Parse "go ..." command
// ---------------------------------------------------------------------------
static void parse_go(const std::string& args) {
    std::istringstream iss(args);
    std::string token;
    int max_depth = 64;
    bool has_time = false;
    int wtime = 300000, btime = 300000, winc = 0, binc = 0;
    int movetime_ms = 0;
    bool infinite = false;

    while (iss >> token) {
        if (token == "perft") {
            int d;
            iss >> d;
            perft_divide(g_pos, d);
            return;
        } else if (token == "depth") {
            iss >> max_depth;
        } else if (token == "wtime") {
            iss >> wtime;
            has_time = true;
        } else if (token == "btime") {
            iss >> btime;
            has_time = true;
        } else if (token == "winc") {
            iss >> winc;
        } else if (token == "binc") {
            iss >> binc;
        } else if (token == "movetime") {
            iss >> movetime_ms;
            has_time = true;
        } else if (token == "infinite") {
            infinite = true;
        }
    }

    (void)infinite;

    if (has_time) {
        int time_left = (g_pos.sideToMove == Color::WHITE) ? wtime : btime;
        int increment = (g_pos.sideToMove == Color::WHITE) ? winc : binc;

        if (movetime_ms > 0) {
            max_depth = 64;
            TimeManager::start_time = TimeManager::get_time_ms();
            TimeManager::allocated_time = movetime_ms;
            TimeManager::stop_search = false;
        } else {
            TimeManager::init_timer(time_left, increment);
        }
    } else {
        TimeManager::start_time = TimeManager::get_time_ms();
        TimeManager::allocated_time = 999999999;
        TimeManager::stop_search = false;
    }

    Move best = ThreadPool::start_search(g_pos, max_depth);
    std::cout << "bestmove " << move_to_str(best) << std::endl;
    std::cout.flush();
}

// ---------------------------------------------------------------------------
//  UCI Main Loop
// ---------------------------------------------------------------------------
void uci_loop() {
    g_pos.set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    std::string line;

    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;

        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;

        if (cmd == "uci") {
            std::cout << "id name Sic" << std::endl;
            std::cout << "id author Claudio M. Camacho <claudiomkd@gmail.com>" << std::endl;
            std::cout << "uciok" << std::endl;
        } else if (cmd == "isready") {
            std::cout << "readyok" << std::endl;
        } else if (cmd == "ucinewgame") {
            TimeManager::stop_search = false;
            clear_tt();
        } else if (cmd == "setoption") {
            std::string rest;
            std::getline(iss, rest);

            std::string name;
            std::string value;

            std::istringstream opt_iss(rest);
            std::string token;
            while (opt_iss >> token) {
                if (token == "name") {
                    opt_iss >> name;
                } else if (token == "value") {
                    opt_iss >> value;
                }
            }

            if (name == "EvalFile") {
                load_nnue(value);
            } else if (name == "Threads") {
                ThreadPool::set_thread_count(std::stoi(value));
            }
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
