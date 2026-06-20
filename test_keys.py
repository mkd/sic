import subprocess
import chess
import chess.pgn

with open('test_game.pgn') as f:
    game = chess.pgn.read_game(f)
moves = ' '.join(m.uci() for m in game.mainline_moves())

p = subprocess.Popen(['./sic'], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
p.stdin.write('position startpos moves ' + moves + '\nkeys\nquit\n')
p.stdin.flush()

for line in p.stdout:
    print(line.strip())
