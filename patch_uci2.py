import re

with open("src/uci.cpp", "r") as f:
    content = f.read()

# Fix get_mvv_lva
def repl_mvv(match):
    return """// Quick MVV LVA for smoves
static int get_mvv_lva(const Position& pos, Move m) {
    Piece p_attacker = pos.piece_on(move_from(m));
    Piece p_victim = pos.piece_on(move_to(m));
    
    PieceType attacker = p_attacker == Piece::PIECE_NONE ? PieceType::NONE : static_cast<PieceType>((static_cast<int>(p_attacker) % 6) + 1);
    PieceType victim = p_victim == Piece::PIECE_NONE ? PieceType::NONE : static_cast<PieceType>((static_cast<int>(p_victim) % 6) + 1);
    
    if (move_flag(m) == MOVE_FLAG_ENPASSANT) {
        victim = PieceType::PAWN;
    }
    int score = 0;
    if (victim != PieceType::NONE) {
        score = 100 * static_cast<int>(victim) - static_cast<int>(attacker);
    }
    if (move_flag(m) == MOVE_FLAG_PROMOTION) {
        score += 800; // rough queen value
    }
    return score;
}"""

content = re.sub(r'// Quick MVV LVA for smoves[\s\S]*?return score;\n\}', repl_mvv, content)

# Fix help string
def repl_help(match):
    return """    } else if (cmd == "help") {
        std::cout << "Help:\\n"
                  << "- d: display the current position on the board\\n"
                  << "- eval: print the static evaluation for the current position\\n"
                  << "- flip: flip the board when being printed\\n"
                  << "- moves: print the list of pseudo-legal moves, without being sorted\\n"
                  << "- smoves: print the list of pseudo-legal moves, sorted by score\\n";"""

content = re.sub(r'    \} else if \(cmd == "help"\) \{[\s\S]*?- smoves.*\\n";', repl_help, content)

# Fix print(g_flip_board) to print()
content = content.replace("g_pos.print(g_flip_board);", "g_pos.print();")

# Fix newline string literals
content = content.replace('std::cout << move_to_str(list.moves[i]) << "\n";', 'std::cout << move_to_str(list.moves[i]) << "\\n";')
content = content.replace('std::cout << move_to_str(list.moves[i]) << " (" << scores[i] << ")\n";', 'std::cout << move_to_str(list.moves[i]) << " (" << scores[i] << ")\\n";')

with open("src/uci.cpp", "w") as f:
    f.write(content)

