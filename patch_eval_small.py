import os

with open('src/uci.cpp', 'r') as f:
    content = f.read()

content = content.replace('int eval_val = evaluate(g_pos);', 'int eval_val = evaluate(g_pos, true);')

with open('src/uci.cpp', 'w') as f:
    f.write(content)
