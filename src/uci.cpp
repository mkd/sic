#include "../include/uci.h"
#include "../include/position.h"
#include "../include/movegen.h"
#include "../include/move.h"
#include "../include/search.h"
#include "../include/perft.h"
#include "../include/timeman.h"
#include "../include/tt.h"
#include "../include/evaluate.h"
#include "../include/thread.h"
#include "../include/tbprobe.h"
#include "stockfish_probe/probe.h"
#include "stockfish_probe/nnue_incremental.h"
#include <iostream>
#include <string>
#include <sstream>
#include <thread>

// ---------------------------------------------------------------------------
//  Global Search Thread
// ---------------------------------------------------------------------------
static std::thread search_thread;

// ---------------------------------------------------------------------------
//  Global Position
// ---------------------------------------------------------------------------
static Position g_pos;

// ---------------------------------------------------------------------------
//  Game History (Zobrist keys for repetition detection)
// ---------------------------------------------------------------------------
std::vector<uint64_t> g_gameHistory;

// ---------------------------------------------------------------------------
//  NNUE Network File Path (single HalfKP)
// ---------------------------------------------------------------------------
static std::string evalFile = "nn-62ef826d1a6d.nnue";

// ---------------------------------------------------------------------------
//  Parse "position ..." command
// ---------------------------------------------------------------------------
static Move parse_move(Position& pos, const std::string& str) {
    MoveList list;
    MoveGen::generate_legal_moves(pos, list);
    for (int i = 0; i < list.size(); ++i) {
        if (move_to_str(list.moves[i]) == str) {
            return list.moves[i];
        }
    }
    return MOVE_NONE;
}

static void parse_position(const std::string& args) {
    std::istringstream iss(args);
    std::string token;
    iss >> token;

    if (token == "startpos") {
        g_pos.set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
        Stockfish::Incremental::setup_reset("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

        g_gameHistory.clear();
        g_gameHistory.push_back(g_pos.zobristKey);

        std::string move_token;
        while (iss >> move_token) {
            if (move_token == "moves") {
                std::string m;
                while (iss >> m) {
                    Move mv = parse_move(g_pos, m);
                    if (mv != MOVE_NONE && g_pos.make_move(mv)) {
                        Stockfish::Incremental::setup_move(mv);
                        g_gameHistory.push_back(g_pos.zobristKey);
                    }
                }
            }
        }
    } else if (token == "fen") {
        // --- Read exactly 6 FEN tokens, building the string from scratch ---
        std::string fen;
        for (int i = 0; i < 6; ++i) {
            iss >> token;
            fen += token;
            if (i < 5) fen += ' ';
        }

        g_pos.set_fen(fen);
        Stockfish::Incremental::setup_reset(fen);

        g_gameHistory.clear();
        g_gameHistory.push_back(g_pos.zobristKey);

        // --- Handle "moves ..." appended after FEN ---
        std::string move_token;
        while (iss >> move_token) {
            if (move_token == "moves") {
                std::string m;
                while (iss >> m) {
                    Move mv = parse_move(g_pos, m);
                    if (mv != MOVE_NONE && g_pos.make_move(mv)) {
                        Stockfish::Incremental::setup_move(mv);
                        g_gameHistory.push_back(g_pos.zobristKey);
                    }
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
            TimeManager::allocated_time = movetime_ms;
            TimeManager::start_time = TimeManager::get_time_ms();
            TimeManager::stop_search = false;
        } else {
            TimeManager::init_timer(time_left, increment);
        }
    } else {
        TimeManager::allocated_time = 999999999;
        TimeManager::start_time = TimeManager::get_time_ms();
        TimeManager::stop_search = false;
    }

    if (search_thread.joinable()) {
        search_thread.join();
    }

    auto pos_ptr = std::make_shared<Stockfish::Position>();
    std::memcpy(pos_ptr.get(), &Stockfish::Incremental::get_global_pos(), sizeof(Stockfish::Position));
    auto setup_ptr = std::make_shared<std::deque<Stockfish::StateInfo>>(Stockfish::Incremental::get_setup_states());

    search_thread = std::thread([max_depth, pos_ptr, setup_ptr]() {
        Stockfish::Incremental::sync_from_main_thread(*pos_ptr, *setup_ptr);
        inc_tt_age();
        Move best = ThreadPool::start_search(g_pos, max_depth);
        std::cout << "bestmove " << move_to_str(best) << std::endl;
        std::cout.flush();
    });
}

// ---------------------------------------------------------------------------
//  UCI Main Loop
// ---------------------------------------------------------------------------
void uci_loop() {
    g_pos.set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    Stockfish::Incremental::setup_reset("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    std::string line;

    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;

        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;

        if (cmd == "uci") {
            std::cout << "id name Sic" << std::endl;
            std::cout << "id author Claudio M. Camacho <claudiomkd@gmail.com>" << std::endl;
            std::cout << "option name Hash type spin default 4096 min 1 max 131072" << std::endl;
            std::cout << "option name Clear Hash type button" << std::endl;
            std::cout << "option name EvalFile type string default nn-62ef826d1a6d.nnue" << std::endl;
            std::cout << "option name SyzygyPath type string default <empty>" << std::endl;
            std::cout << "uciok" << std::endl;

            Stockfish::Probe::init("nn-sfnnv10.nnue", "nn-baff1ede1f90.nnue");
            Stockfish::Incremental::init();
        } else if (cmd == "isready") {
            std::cout << "readyok" << std::endl;
        } else if (cmd == "ucinewgame") {
            TimeManager::stop_search = false;
            clear_tt();
            g_gameHistory.clear();
            std::cout << "option name Hash type spin default 4096 min 1 max 131072" << std::endl;
            std::cout << "option name Clear Hash type button" << std::endl;
            std::cout << "option name EvalFile type string default nn-62ef826d1a6d.nnue" << std::endl;
            std::cout << "option name SyzygyPath type string default <empty>" << std::endl;
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
                Stockfish::Probe::init(evalFile.c_str(), "nn-baff1ede1f90.nnue");
            } else if (name == "Threads") {
                ThreadPool::set_thread_count(std::stoi(value));
            } else if (name == "Hash") {
                init_tt(std::stoi(value));
            } else if (name == "Clear Hash") {
                clear_tt();
            } else if (name == "SyzygyPath") {
                if (tb_init(value.c_str())) {
                    std::cout << "info string Syzygy tablebases initialized (Up to " << TB_LARGEST << " pieces)" << std::endl;
                } else {
                    std::cout << "info string Failed to initialize Syzygy tablebases at " << value << std::endl;
                }
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
            std::cout << "FEN: " << g_pos.get_fen() << "\n";
            MoveList list;
            MoveGen::generate_legal_moves(g_pos, list);
            std::cout << "Legal moves: ";
            for (int i = 0; i < list.size(); ++i) {
                std::cout << move_to_str(list.moves[i]) << "(" << list.moves[i] << ") ";
            }
            std::cout << "\n";
        } else if (cmd == "eval") {
            std::cout << "Static Evaluation: " << evaluate(g_pos) << " cp\n";
            std::cout << "FEN: " << g_pos.get_fen() << "\n";
        } else if (cmd == "stop") {
            TimeManager::stop_search = true;
        } else if (cmd == "quit") {
            TimeManager::stop_search = true;
            if (search_thread.joinable()) {
                search_thread.join();
            }
            return;
        }

        std::cout.flush();
    }

    // Clean up if we break out of the loop (e.g., EOF)
    TimeManager::stop_search = true;
    if (search_thread.joinable()) {
        search_thread.join();
    }
}
