import re

with open('src/search.cpp', 'r') as f:
    lines = f.readlines()

new_lines = []
for line in lines:
    new_lines.append(line)
    
    # Check for quiescence recursive call
    if 'Value val = -quiescence(next_pos, -beta, -alpha, ply + 1, sw);' in line:
        indent = line[:len(line) - len(line.lstrip())]
        new_lines.append(indent + "if (TimeManager::stop_search) return 0;\n")
        
    # Check for negamax recursive calls in negamax()
    elif 'Value se_score = -negamax(' in line or \
         'Value pc_score = -negamax(' in line or \
         'Value null_val = -negamax(' in line or \
         ('val = -negamax(' in line and 'list.moves[i]' in line and 'next_pos' in line and 'd - 1' not in line):
        indent = line[:len(line) - len(line.lstrip())]
        new_lines.append(indent + "if (TimeManager::stop_search) return 0;\n")
        
    # Check for negamax recursive calls in search_position()
    elif 'val = -negamax(' in line and 'd - 1' in line:
        indent = line[:len(line) - len(line.lstrip())]
        new_lines.append(indent + "if (TimeManager::stop_search) break;\n")

with open('src/search.cpp', 'w') as f:
    f.writelines(new_lines)
