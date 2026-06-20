import subprocess

sic = subprocess.Popen(["./sic"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)

def send(cmd):
    sic.stdin.write(cmd + "\n")
    sic.stdin.flush()

# FEN after White plays a1c1
send("position fen r2rb1k1/1p1nbppp/p1n1p3/q3P3/P1B1N3/1P3N2/1B2QPPP/2RR2K1 b - - 4 17")
send("setoption name MultiPV value 5")
send("go depth 12")

while True:
    line = sic.stdout.readline()
    if not line:
        break
    if "info depth" in line:
        print(line.strip())
    if "bestmove" in line:
        break

sic.terminate()
