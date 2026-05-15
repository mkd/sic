#include "../include/uci.h"
#include "../include/position.h"
#include "../include/movegen.h"
#include "../include/move.h"
#include "../include/search.h"
#include "../include/perft.h"
#include "../include/timeman.h"
#include "../include/tt.h"
#include "../include/nnue_bridge.h"
#include "../include/evaluate.h"
#include "../include/thread.h"
#include <iostream>
#include <string>
#include <sstream>

// ---------------------------------------------------------------------------
//  Global Position
// ---------------------------------------------------------------------------
static Position g_pos;

// ---------------------------------------------------------------------------
//  NNUE Network File Path (single HalfKP)
// ---------------------------------------------------------------------------
static std::string evalFile = "nn-62ef826d1a6d.nnue";

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
                        char p = m[4];
                        if (p == 'q') prom = PieceType::QUEEN;
                        else if (p == 'r') prom = PieceType::ROOK;
                        else if (p == 'b') prom = PieceType::BISHOP;
                        else if (p == 'n') prom = PieceType::KNIGHT;
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
                        char p = m[4];
                        if (p == 'q') prom = PieceType::QUEEN;
                        else if (p == 'r') prom = PieceType::ROOK;
                        else if (p == 'b') prom = PieceType::BISHOP;
                        else if (p == 'n') prom = PieceType::KNIGHT;
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

    load_nnue(evalFile);

    std::string line;

    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;

        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;

        if (cmd == "uci") {
            std::cout << "id name Sic" << std::endl;
            std::cout << "id author Claudio M. Camacho <claudiomkd@gmail.com>" << std::endl;
            std::cout << "option name Hash type spin default 1024 min 1 max 131072" << std::endl;
            std::cout << "option name Clear Hash type button" << std::endl;
            std::cout << "option name EvalFile type string default nn-62ef826d1a6d.nnue" << std::endl;
            std::cout << "uciok" << std::endl;
        } else if (cmd == "isready") {
            std::cout << "readyok" << std::endl;
        } else if (cmd == "ucinewgame") {
            TimeManager::stop_search = false;
            clear_tt();
            std::cout << "option name Hash type spin default 1024 min 1 max 131072" << std::endl;
            std::cout << "option name Clear Hash type button" << std::endl;
            std::cout << "option name EvalFile type string default nn-62ef826d1a6d.nnue" << std::endl;
        } else if (cmd == "setoption") {
            std::string rest;
            std::getline(iss, rest);

            std::string name;
            std::string value;

            std::istringstream opt_iss(rest);
            std::string token;
            bool reading_name = false;
            bool reading_value = false;
            while (opt_iss >> token) {
                if (token == "name") {
                    reading_name = true;
                    reading_value = false;
                    name.clear();
                } else if (token == "value") {
                    reading_value = true;
                    reading_name = false;
                    value.clear();
                } else if (reading_name) {
                    if (!name.empty()) name += ' ';
                    name += token;
                } else if (reading_value) {
                    if (!value.empty()) value += ' ';
                    value += token;
                }
            }

            if (name == "EvalFile") {
                evalFile = value;
                load_nnue(evalFile);
            } else if (name == "Threads") {
                ThreadPool::set_thread_count(std::stoi(value));
            } else if (name == "Hash") {
                init_tt(std::stoi(value));
            } else if (name == "Clear Hash") {
                clear_tt();
            }
        } else if (cmd == "position") {
            std::string rest;
            std::getline(iss, rest);
            parse_position(rest);
        } else if (cmd == "go") {
            std::string rest;
            std::getline(iss, rest);
            parse_go(rest);
        } else if (cmd == "d") {
            g_pos.print();
        } else if (cmd == "eval") {
            std::cout << "Static Evaluation: " << evaluate(g_pos) << " cp\n";
        } else if (cmd == "quit") {
            return;
        }

        std::cout.flush();
    }
}
