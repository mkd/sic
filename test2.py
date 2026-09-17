import subprocess
import time

p = subprocess.Popen(["./sic"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

def send(cmd):
    p.stdin.write(cmd + "\n")
    p.stdin.flush()

send("position fen r5k1/5ppp/4pn2/p7/P1N2B2/5Q2/q4bPP/3R3K w - - 1 28")
send("go depth 4")
time.sleep(2)
send("quit")

out, _ = p.communicate()
print(out)
