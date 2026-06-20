import subprocess

sic = subprocess.Popen(["./sic"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)

def send(cmd):
    sic.stdin.write(cmd + "\n")
    sic.stdin.flush()

send("position fen r2rb1k1/1p1nbppp/p1n1p3/q3P3/P1B1N3/1P3N2/1B2QPPP/2RR2K1 b - - 4 17 moves d7e5 f3e5 d8d1 c1d1 c6e5 b2c3 e5c4")
send("eval")
send("go depth 0") # Wait, sic doesn't support depth 0 in uci.cpp? Let's use depth 1.

while True:
    line = sic.stdout.readline()
    if not line:
        break
    print(line.strip())
    if "bestmove" in line:
        break

sic.terminate()
