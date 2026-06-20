#include "../include/attacks.h"
#include "../include/position.h"
#include "../include/search.h"
#include "../include/uci.h"
#include "../include/tt.h"
#include "../include/thread.h"
#include <iostream>
#include <string>
#include <thread>

extern std::thread search_thread;

int main(int argc, char* argv[]) {
    init_attacks();
    init_zobrist();
    init_tt(4096);
    init_lmr();
    ThreadPool::init();
    uci_init();

    // Startup info
    std::cout << "Sic 1.1 by Claudio M. Camacho <claudiomkd@gmail.com>\n";
    std::cout << "Hash table initialized with " << (4096ull * 1024 * 1024 / sizeof(TTCluster)) * 4 << " entries (4096 MBytes)\n";
    std::cout << "Search thread pool initialized with " << ThreadPool::threads.size() << " threads\n";
    std::cout << std::endl;

    if (argc > 1) {
        std::string cmd = "";
        for (int i = 1; i < argc; ++i) {
            cmd += argv[i];
            if (i < argc - 1) cmd += " ";
        }
        
        // Execute the single CLI command
        uci_execute_line(cmd);
        
        // Wait for search thread if 'go' command was issued
        if (search_thread.joinable()) {
            search_thread.join();
        }
        return 0;
    }

    uci_loop();
    return 0;
}
