import re

with open("include/position.h", "r") as f:
    content = f.read()

content = content.replace("void print() const;", "void print(bool flip = false) const;")

with open("include/position.h", "w") as f:
    f.write(content)


with open("src/position.cpp", "r") as f:
    content = f.read()

new_print = """void Position::print(bool flip) const {
    const char white_chars[] = "PNBRQK";
    const char black_chars[] = "pnbrqk";
    
    std::cout << "\\n    +----+----+----+----+----+----+----+----+\\n";
    
    for (int i = 0; i < 8; ++i) {
        int r = flip ? i : 7 - i;
        std::cout << "  " << (r + 1) << " |";
        for (int j = 0; j < 8; ++j) {
            int f_idx = flip ? 7 - j : j;
            Square sq = static_cast<Square>(r * 8 + f_idx);
            Piece p = piece_on(sq);
            
            if (p == Piece::PIECE_NONE) {
                std::cout << "    |";
            } else {
                int p_int = static_cast<int>(p);
                if (p_int >= 0 && p_int <= 5) {
                    std::cout << " " << white_chars[p_int] << "  |";
                } else if (p_int >= 6 && p_int <= 11) {
                    std::cout << " " << black_chars[p_int - 6] << "* |";
                }
            }
        }
        std::cout << "\\n    +----+----+----+----+----+----+----+----+\\n";
    }
    
    std::cout << "  ";
    for (int j = 0; j < 8; ++j) {
        int f_idx = flip ? 7 - j : j;
        std::cout << "    " << (char)('a' + f_idx);
    }
    std::cout << "\\n\\n";
    
    std::cout << "  Fen:    " << get_fen() << "\\n";
    std::cout << "  Key:    " << std::hex << std::uppercase << zobristKey << std::dec << "\\n";
    std::cout << "  Side:   " << (sideToMove == Color::WHITE ? "White" : "Black") << "\\n";
    
    std::string epsq_str = "-";
    if (epSquare != Square::SQ_NONE) {
        epsq_str = "";
        epsq_str += (char)('a' + (epSquare & 7));
        epsq_str += (char)('1' + (epSquare >> 3));
    }
    std::cout << "  Epsq:   " << epsq_str << "\\n";
    
    std::string castle_str = "";
    if (castlingRights & CASTLING_WK) castle_str += "K";
    if (castlingRights & CASTLING_WQ) castle_str += "Q";
    if (castlingRights & CASTLING_BK) castle_str += "k";
    if (castlingRights & CASTLING_BQ) castle_str += "q";
    if (castle_str.empty()) castle_str = "-";
    std::cout << "  Castle: " << castle_str << "\\n";
    
    std::cout << "  Checkers: ";
    if (checkers.bb) {
        Bitboard c = checkers;
        while(c.bb) {
            Square sq = pop_lsb(c);
            std::cout << (char)('a' + (sq & 7)) << (char)('1' + (sq >> 3)) << " ";
        }
    }
    std::cout << "\\n";
    
    // Eval formatting requires evaluate.h which may not be included here.
    // Instead of including it and risking circular dependencies, the uci.cpp prints eval.
    // Wait, the user asked 'Eval: +0.42' inside the 'd' command.
    // We will do it in uci.cpp instead of Position::print() to keep it decoupled.
    std::cout << std::flush;
}"""

content = re.sub(r'void Position::print\(\) const \{[\s\S]*?std::cout << std::flush;\n\}', new_print, content)

with open("src/position.cpp", "w") as f:
    f.write(content)

