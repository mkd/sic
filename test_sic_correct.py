import subprocess
fen = "5rk1/5ppp/2N1p3/2b1N3/2R1n1P1/1p6/q4P1P/1Q4K1 w - - 2 29"
p = subprocess.Popen(['./sic'], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)
p.stdin.write(f"position fen {fen}\ngo depth 15\n")
p.stdin.flush()
for line in iter(p.stdout.readline, ''):
    if "depth" in line: print(line.strip())
    if "bestmove" in line: break
p.terminate()
