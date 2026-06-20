import re

with open("src/main.cpp", "r") as f:
    content = f.read()

new_main = """#include <iostream>
#include "../include/tt.h"

int main(int argc, char* argv[]) {
    init_attacks();
    init_zobrist();
    init_tt(4096);
    init_lmr();
    ThreadPool::init();

    // Startup info
    std::cout << "Sic 1.0 by Claudio M. Camacho <claudiomkd@gmail.com>\\n";
    std::cout << "Hash table initialized with " << (4096ull * 1024 * 1024 / sizeof(TTCluster)) * 4 << " entries (4096 MBytes)\\n";

    if (argc > 1) {
        std::string cmd = "";
        for (int i = 1; i < argc; ++i) {
            cmd += argv[i];
            if (i < argc - 1) cmd += " ";
        }
        
        // Execute the single CLI command
        uci_execute_line(cmd);
        
        // Wait for search thread if 'go' command was issued
        extern std::thread search_thread;
        if (search_thread.joinable()) {
            search_thread.join();
        }
        return 0;
    }

    uci_loop();
    return 0;
}
"""

content = re.sub(r'int main\(\) \{[\s\S]*?return 0;\n\}', new_main, content)

with open("src/main.cpp", "w") as f:
    f.write(content)

