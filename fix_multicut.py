import os

with open('src/search.cpp', 'r') as f:
    content = f.read()

# Fix Multi-Cut to check legality
old_mc = """            Position next_pos = pos;
            next_pos.make_move(mc_list.moves[i]);
            NnueGuard guard(mc_list.moves[i]);"""

new_mc = """            Position next_pos = pos;
            if (!next_pos.make_move(mc_list.moves[i])) continue;
            NnueGuard guard(mc_list.moves[i]);"""

if old_mc in content:
    content = content.replace(old_mc, new_mc)
    with open('src/search.cpp', 'w') as f:
        f.write(content)
    print("Fixed Multi-Cut")
else:
    print("Could not find Multi-Cut pattern")

