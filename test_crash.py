import chess
import subprocess

board = chess.Board()
moves = ["Nf3", "Nf6", "c4", "e6", "g3", "d5", "Bg2", "Be7", "O-O", "O-O", "d4", "Nbd7", "Qc2", "c6", "b3", "b6", "Rd1", "Ba6", "Nbd2", "Rc8", "e4", "c5", "e5", "Ne8", "Bb2", "cxd4", "Qd3", "Nc5", "Qxd4", "f5", "Qe3", "Nc7", "Rac1", "Qe8", "Nd4", "Ne4", "Nxe4", "dxe4"]
uci_moves = []
for m in moves:
    move = board.push_san(m)
    uci_moves.append(move.uci())

p = subprocess.Popen(["./sic"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
p.stdin.write("position startpos moves " + " ".join(uci_moves) + "\ngo depth 20\n")
p.stdin.flush()
for line in p.stdout:
    print(line.strip())
    if "bestmove" in line:
        break
