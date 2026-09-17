with open('src/stockfish_probe/nnue_incremental.cpp', 'r') as f:
    content = f.read()

# Replace resize with something that only resizes if smaller
content = content.replace("search_stack.resize(2048);", "if (search_stack.size() < 2048) search_stack.resize(2048);")

with open('src/stockfish_probe/nnue_incremental.cpp', 'w') as f:
    f.write(content)
