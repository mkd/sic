import os

with open('src/uci.cpp', 'r') as f:
    content = f.read()

start_idx = content.find('} else if (cmd == "see") {')
if start_idx != -1:
    end_idx = content.find('} else if (cmd == "keys") {', start_idx)
    content = content[:start_idx] + content[end_idx:]

content = content.replace('int eval_val = evaluate(g_pos, true);', 'int eval_val = evaluate(g_pos);')

with open('src/uci.cpp', 'w') as f:
    f.write(content)
