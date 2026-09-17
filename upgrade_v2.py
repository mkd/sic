import os

# 1. Update thread.h to add pawn_history and 4-ply continuation history
with open('include/thread.h', 'r') as f:
    content = f.read()

# Add pawn_history and expand continuation_history to 4 (1, 2, 4, 6 ply)
content = content.replace('int continuation_history[2][14][64][14][64];', 
                          'int continuation_history[4][14][64][14][64];\n    int pawn_history[512][64];')

# Update memsets
memset_old = 'std::memset(continuation_history, 0, sizeof(continuation_history));'
memset_new = 'std::memset(continuation_history, 0, sizeof(continuation_history));\n        std::memset(pawn_history, 0, sizeof(pawn_history));'
content = content.replace(memset_old, memset_new)

with open('include/thread.h', 'w') as f:
    f.write(content)


# 2. Update search.cpp for Pawn History, Multi-Cut, and LMR improvements
with open('src/search.cpp', 'r') as f:
    search_cpp = f.read()

# Replace LMR init
lmr_init_old = 'double reduction = 1.0 + std::log(d) * std::log(m) / 2.00;'
lmr_init_new = 'double reduction = 0.75 + std::log(d) * std::log(m) / 2.25;'
search_cpp = search_cpp.replace(lmr_init_old, lmr_init_new)

with open('src/search.cpp', 'w') as f:
    f.write(search_cpp)
