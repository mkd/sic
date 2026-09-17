import subprocess
p = subprocess.Popen(['./sic'], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
p.stdin.write(b"position startpos\n")
p.stdin.write(b"go depth 10\n")
p.stdin.flush()

for line in p.stdout:
    print(line.decode().strip())
    if b"bestmove" in line:
        break
