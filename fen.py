import chess

board = chess.Board()
moves = ["e4", "e5", "Nf3", "Nc6", "Bb5", "a6", "Ba4", "Nf6", "O-O", "Be7", "Re1", "b5", "Bb3", "O-O", "c3", "d6", "h3", "Nb8", "d4", "Nbd7", "a4", "Bb7", "Nbd2", "c5", "d5", "c4", "Bc2", "Nc5", "Qe2", "Nfd7", "Nf1", "f5", "exf5", "Bxd5", "Be3", "Nxa4", "Bxa4", "bxa4", "Red1", "Bf7", "f6", "Bxf6", "Rxd6", "e4", "Ng5", "Bxg5", "Bxg5", "Qxg5", "Rxd7", "Rab8", "Rd2", "Qb5", "Qd1", "Qb3", "Qxb3", "axb3", "Rxa6", "Rfd8", "Kh2", "Kf8", "Re2", "h6", "Ne3", "Rb5", "Rc6", "Rb7", "Nxc4", "Rbd7", "Kg3", "Rd3+", "Kf4", "Bd5", "Rc5", "Be6", "Kxe4", "Bxc4", "Rxc4", "Rd2", "Rxd2", "Rxd2", "Rb4", "Rxb2", "Rb8+", "Kf7", "Rb7+", "Ke6", "Rb6+", "Kf7", "g3", "Rxf2", "Rxb3", "Re2+", "Kf3", "Rd2", "h4", "Rd3+", "Kf4", "g5+", "hxg5", "hxg5+", "Kg4", "Kf6", "Ra3", "Ke6", "Ra6+", "Ke7", "c4", "Kf7", "Ra7+", "Kg6", "Rc7", "Rd4+", "Kh3", "Kh5", "Rc5", "Kg6", "g4", "Rd3+", "Kh2", "Rd4", "Kg3", "Rd3+", "Kf2", "Rd4", "Rc6+", "Kf7", "Rc7+", "Ke6", "Kf3", "Rd3+", "Ke4", "Rg3", "Rc8", "Rxg4+", "Kf3", "Rh4", "c5", "Rf4+", "Ke3", "Kd7", "Rh8", "Rg4", "c6+", "Kc7", "Rh7+", "Kxc6", "Kf2", "Rh4", "Rg7", "Rf4+", "Ke3", "Rf6", "Rxg5", "Re6+", "Kd4", "Re7", "Kc3", "Re6", "Kb4"]

for i, move in enumerate(moves):
    board.push_san(move)
    if i == 64 * 2 - 2: # White's 64th move
        print("Move 64 White FEN:", board.fen())

print("Final FEN:", board.fen())
