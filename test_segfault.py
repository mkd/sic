import subprocess
import time

p = subprocess.Popen(['./sic'], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
p.stdin.write("position startpos\n")
p.stdin.write("go depth 12\n")
p.stdin.flush()

while True:
    line = p.stdout.readline()
    if not line:
        break
    print(line.strip())
    
p.wait()
print(f"Exit code: {p.returncode}")
if p.returncode != 0:
    print(f"Stderr: {p.stderr.read()}")
