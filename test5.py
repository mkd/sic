import subprocess

sic = subprocess.Popen(["./sic"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)

def send(cmd):
    sic.stdin.write(cmd + "\n")
    sic.stdin.flush()

# FEN after 17... Ndxe5 18. Nxe5 Rxd1+ 19. Rxd1 Nxe5 20. Bc3 Nxc4 21. Bxa5 Nxa5
send("position fen r3b1k1/1p2bppp/p3p3/n3P3/P3N3/1P6/4QPPP/3R2K1 w - - 0 22")
send("eval")
send("quit")

while True:
    line = sic.stdout.readline()
    if not line:
        break
    print(line.strip())

sic.terminate()
