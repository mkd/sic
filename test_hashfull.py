import subprocess
import time

p = subprocess.Popen(["./sic"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

print("Search 1:")
p.stdin.write("setoption name Hash value 64\nposition startpos\ngo depth 12\n")
p.stdin.flush()

for line in p.stdout:
    if "info depth" in line:
        print(line.strip())
    if "bestmove" in line:
        break

print("Search 2:")
p.stdin.write("position startpos moves e2e4\ngo depth 12\n")
p.stdin.flush()

for line in p.stdout:
    if "info depth" in line:
        print(line.strip())
    if "bestmove" in line:
        break

p.stdin.write("quit\n")
p.stdin.flush()
