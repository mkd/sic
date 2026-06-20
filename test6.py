import subprocess

sic = subprocess.Popen(["./sic"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)

def send(cmd):
    sic.stdin.write(cmd + "\n")
    sic.stdin.flush()

# FEN before Rac1
# 1. d7e5 2. f3e5 3. d8d1 4. c1d1 5. c6e5 6. b2c3 7. e5c4
# Let's get the exact FEN after e5c4
send("position fen r2rb1k1/1p1nbppp/p1n1p3/q3P3/P1B1N3/1P3N2/1B2QPPP/2RR2K1 b - - 4 17 moves d7e5 f3e5 d8d1 c1d1 c6e5 b2c3 e5c4")
send("go depth 1")

while True:
    line = sic.stdout.readline()
    if not line:
        break
    if "info depth" in line or "bestmove" in line:
        print(line.strip())
        if "bestmove" in line:
            break

sic.terminate()
