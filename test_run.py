import subprocess

p = subprocess.Popen(["./sic"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

p.stdin.write("position startpos\ngo depth 24\n")
p.stdin.flush()

for line in p.stdout:
    print(line.strip())
    if "bestmove" in line:
        break

p.stdin.write("quit\n")
p.stdin.flush()
