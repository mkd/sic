lines = []
with open('src/search.cpp', 'r') as f:
    lines = f.readlines()

new_lines = []
for i, line in enumerate(lines):
    new_lines.append(line)
    
    # 368:        Value val = -quiescence(next_pos, -beta, -alpha, ply + 1, sw);
    # 493:        Value se_score = -negamax(pos, se_depth, ply, -se_beta - 1, -se_beta, true, sw, MOVE_NONE, tt_move);
    # 523:            Value pc_score = -negamax(next_pos, depth - 4, ply + 1, -prob_beta, -prob_beta + 1, false, sw, m);
    # 553:            Value null_val = -negamax(null_pos, nmp_depth, ply + 1, -beta, -beta + 1, true, sw, MOVE_NONE);
    # 635:            val = -negamax(next_pos, depth - 1 + current_extension, ply + 1, -beta, -alpha, false, sw, list.moves[i]);
    # 648:                val = -negamax(next_pos, reduced_depth, ply + 1, -alpha - 1, -alpha, false, sw, list.moves[i]);
    # 650:                    val = -negamax(next_pos, depth - 1 + current_extension, ply + 1, -alpha - 1, -alpha, false, sw, list.moves[i]);
    # 653:                val = -negamax(next_pos, depth - 1 + current_extension, ply + 1, -alpha - 1, -alpha, false, sw, list.moves[i]);
    # 657:                val = -negamax(next_pos, depth - 1 + current_extension, ply + 1, -beta, -alpha, false, sw, list.moves[i]);
    if i + 1 in [368, 493, 523, 553, 635, 648, 650, 653, 657]:
        indent = line[:len(line) - len(line.lstrip())]
        new_lines.append(indent + "if (TimeManager::stop_search) return 0;\n")

    # 861:                    val = -negamax(next_pos, d - 1, 1, -beta, -alpha, false, sw, list.moves[i]);
    # 863:                    val = -negamax(next_pos, d - 1, 1, -alpha - 1, -alpha, false, sw, list.moves[i]);
    # 865:                        val = -negamax(next_pos, d - 1, 1, -beta, -alpha, false, sw, list.moves[i]);
    if i + 1 in [861, 863, 865]:
        indent = line[:len(line) - len(line.lstrip())]
        new_lines.append(indent + "if (TimeManager::stop_search) break;\n")

with open('src/search.cpp', 'w') as f:
    f.writelines(new_lines)
