import subprocess
p = subprocess.Popen(['./sic'], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
p.stdin.write(b"position fen 8/8/7p/7P/k3Q1P1/1q6/3p2K1/8 b - - 0 63\n")
p.stdin.write(b"go depth 12\n")
p.stdin.flush()

for line in p.stdout:
    print(line.decode().strip())
    if b"bestmove" in line:
        break
