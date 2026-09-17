import subprocess
import chess

# FEN BEFORE Qa2 was played (White to move after Qa2 is played, so Black's Qa2 is on the board)
fen = "5rk1/5ppp/2N1p3/2b1N3/2Rn2P1/1p6/q4P1P/1Q4K1 w - - 0 29"
board = chess.Board(fen)

# Let's see what happens after Qxa2 bxa2
board.push_san("Qxa2")
board.push_san("bxa2")

with open("fen2.txt", "w") as f:
    f.write(board.fen())
