import subprocess

sic = subprocess.Popen(["./sic"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)

def send(cmd):
    sic.stdin.write(cmd + "\n")
    sic.stdin.flush()

# FEN after c3a5
send("position fen r2rb1k1/1p2bppp/p3p3/B3P3/P1n1N3/1P3N2/4QPPP/3R2K1 b - - 0 21")
send("eval")
send("quit")

while True:
    line = sic.stdout.readline()
    if not line:
        break
    print(line.strip())

sic.terminate()
