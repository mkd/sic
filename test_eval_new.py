import subprocess
import chess

fen = "5rk1/5ppp/2N1p3/1b2N3/3Rn1P1/1p6/qQ3P1P/1Q4K1 w - - 3 28"
board = chess.Board(fen)

with open("fen.txt", "w") as f:
    f.write(board.fen())

