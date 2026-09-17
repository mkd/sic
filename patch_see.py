import os

with open('src/uci.cpp', 'r') as f:
    content = f.read()

patch = """
    } else if (cmd == "see") {
        MoveList list;
        MoveGen::generate_legal_moves(g_pos, list);
        for (int i = 0; i < list.size(); ++i) {
            std::cout << move_to_str(list.moves[i]) << " SEE: " << get_see_score(g_pos, list.moves[i]) << "\n";
        }
"""

content = content.replace('} else if (cmd == "keys") {', patch + '} else if (cmd == "keys") {')

with open('src/uci.cpp', 'w') as f:
    f.write(content)
