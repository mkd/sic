import subprocess
p = subprocess.Popen(['./sic'], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
p.stdin.write(b"position fen 6rk/3Qb1pp/5P2/4p3/1r2N2q/3BB1P1/5P1P/3R2K1 b - - 0 1\n")
p.stdin.write(b"go depth 14\n")
p.stdin.flush()

for line in p.stdout:
    print(line.decode().strip())
    if b"bestmove" in line:
        break
