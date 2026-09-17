import subprocess
import time

p = subprocess.Popen(['/Users/claudio/projects/Chess/gargantua/dist/gargantua-2.1-macos-arm64'], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)
p.stdin.write("position fen 5rk1/5ppp/2N1p3/1b2N3/3Rn1P1/1p6/q4P1P/1Q4K1 w - - 0 29\n")
p.stdin.write("go depth 12\n")
p.stdin.flush()

for line in iter(p.stdout.readline, ''):
    if "depth" in line: print(line.strip())
    if "bestmove" in line: break
p.terminate()
