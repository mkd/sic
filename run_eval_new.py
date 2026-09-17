import subprocess
fen = "5rk1/5ppp/2N1p3/1b2N3/3Rn1P1/1p6/q4P1P/1Q4K1 w - - 0 28"
p = subprocess.Popen(['./sic'], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)
p.stdin.write(f"position fen {fen}\nd\nquit\n")
p.stdin.flush()
for line in p.stdout:
    if "Eval:" in line: print(line.strip())
