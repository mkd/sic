import subprocess

sic = subprocess.Popen(["./sic"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)

def send(cmd):
    sic.stdin.write(cmd + "\n")
    sic.stdin.flush()

# FEN directly (same as output of test13)
send("position fen r3b1k1/1p2bppp/p3p3/B7/P1n1N3/1P6/4QPPP/3R2K1 b - - 0 21")
send("eval")
send("quit")

while True:
    line = sic.stdout.readline()
    if not line:
        break
    print(line.strip())

sic.terminate()
