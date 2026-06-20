import chess

board = chess.Board()
moves = ["e4", "e5", "Nf3", "Nc6", "Bb5", "a6", "Ba4", "Nf6", "O-O", "Be7", "Re1", "b5", "Bb3", "O-O", "c3", "d6", "h3", "Nb8", "d4", "Nbd7", "a4", "Bb7", "Nbd2", "c5", "d5", "c4", "Bc2", "Nc5", "Qe2", "Nfd7", "Nf1", "f5", "exf5", "Bxd5", "Be3", "Nxa4", "Bxa4", "bxa4", "Red1", "Bf7", "f6", "Bxf6", "Rxd6", "e4", "Ng5", "Bxg5", "Bxg5", "Qxg5", "Rxd7", "Rab8", "Rd2", "Qb5", "Qd1", "Qb3", "Qxb3", "axb3", "Rxa6", "Rfd8", "Kh2", "Kf8", "Re2", "h6", "Ne3", "Rb5", "Rc6", "Rb7", "Nxc4", "Rbd7", "Kg3", "Rd3+", "Kf4", "Bd5", "Rc5", "Be6", "Kxe4", "Bxc4", "Rxc4", "Rd2"]

for i, move in enumerate(moves):
    board.push_san(move)
    if i == 39 * 2 - 1: # Black's 39th move
        print("Move 39 Black FEN:", board.fen())

