import subprocess
import chess
import chess.pgn

with open('test_game.pgn') as f:
    game = chess.pgn.read_game(f)

board = game.board()
moves = list(game.mainline_moves())

p = subprocess.Popen(["./sic"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

for i, move in enumerate(moves):
    if i >= 80: # move 41
        print(f"Ply {i} before move {move}: FEN {board.fen()}")
        p.stdin.write(f"position fen {board.fen()}\nkeys\n")
        p.stdin.flush()
        # Read two lines from sic
        while True:
            line = p.stdout.readline().strip()
            if line.startswith("Current pos key:"):
                print(f"  -> {line}")
                break
    board.push(move)

print(f"Final Ply {len(moves)}: FEN {board.fen()}")
p.stdin.write(f"position fen {board.fen()}\nkeys\n")
p.stdin.flush()
while True:
    line = p.stdout.readline().strip()
    if line.startswith("Current pos key:"):
        print(f"  -> {line}")
        break

p.stdin.write("quit\n")
p.stdin.flush()
