import subprocess
import time

sic = subprocess.Popen(["./sic"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)

def send(cmd):
    sic.stdin.write(cmd + "\n")
    sic.stdin.flush()

send("position fen r2rb1k1/1p1nbppp/p1n1p3/q3P3/P1B1N3/1P3N2/1B2QPPP/R2R2K1 w - - 3 17")
send("go depth 15")

while True:
    line = sic.stdout.readline()
    if not line:
        break
    print(line.strip())
    if "bestmove" in line:
        break

sic.terminate()
