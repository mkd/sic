import subprocess
moves = "g1f3 g8f6 d2d4 d7d5 c2c4 e7e6 e2e3 f8e7 f1e2 e8g8 b1d2 b7b6 b2b3 c8b7 c4d5 f6d5 c1b2 b8d7 e1g1 c7c5 a1c1 a8c8 a2a3 c5d4 c1c8 d8c8 f3d4 c8a8 b1b1 d5f6 e2f3 b7f3 d4f3 b6b5 b2b4 a7a5 f3f6 d7f6 b4a5 a8a5 d2b3 a5a3 b3d4 b5b4 d4c6 e7d6 e3e4 d6c5 f3e5 b4b3 g2g4 a3a4 c1c1 f6e4 c1c4 a4a2"
p = subprocess.Popen(['./sic'], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)
p.stdin.write("setoption name Threads value 10\n")
p.stdin.write("setoption name Hash value 8192\n")
p.stdin.write(f"position startpos moves {moves}\n")
p.stdin.write("go depth 15\n")
p.stdin.flush()
for line in iter(p.stdout.readline, ''):
    if "depth" in line: print(line.strip())
    if "bestmove" in line: break
p.terminate()
