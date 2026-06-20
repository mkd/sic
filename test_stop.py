with open('src/search.cpp', 'r') as f:
    content = f.read()

content = content.replace(
    "    Move best_root_move = MOVE_NONE;",
    "    Move best_root_move = MOVE_NONE;\n    if (thread_id == 0) std::cout << \"stop_search at start: \" << TimeManager::stop_search << std::endl;"
)
with open('src/search.cpp', 'w') as f:
    f.write(content)
