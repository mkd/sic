import subprocess
with open("fen2.txt") as f:
    fen = f.read().strip()
p = subprocess.Popen(['./sic'], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)
p.stdin.write(f"position fen {fen}\nd\nquit\n")
p.stdin.flush()
for line in p.stdout:
    if "Eval:" in line: print(line.strip())
