import re

with open("src/uci.cpp", "r") as f:
    content = f.read()

def repl_d(match):
    return """    } else if (cmd == "flip") {
        g_flip_board = !g_flip_board;
        g_pos.print(g_flip_board);
        int eval_val = evaluate(g_pos);
        if (g_pos.sideToMove == Color::BLACK) eval_val = -eval_val;
        std::cout << "  Eval: " << (eval_val > 0 ? "+" : "") << (eval_val / 100.0) << "\\n\\n";
    } else if (cmd == "d" || cmd == "eval") {
        g_pos.print(g_flip_board);
        int eval_val = evaluate(g_pos);
        if (g_pos.sideToMove == Color::BLACK) eval_val = -eval_val;
        std::cout << "  Eval: " << (eval_val > 0 ? "+" : "") << (eval_val / 100.0) << "\\n\\n";
    }"""

content = re.sub(r'    \} else if \(cmd == "flip"\) \{[\s\S]*?    \} else if \(cmd == "moves"\) \{', repl_d + '\n    } else if (cmd == "moves") {', content)

with open("src/uci.cpp", "w") as f:
    f.write(content)
