import subprocess
p = subprocess.Popen(["./sic"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
p.stdin.write("position fen 1k6/1p2bR2/p3p2p/1q6/4B1P1/1PQ4P/K1P5/r7 w - - 8 45\nkeys\nquit\n")
p.stdin.flush()
for line in p.stdout:
    print(line.strip())
