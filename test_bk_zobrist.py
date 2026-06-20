import subprocess

p = subprocess.Popen(["./sic"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

# Test 1: Black King on e8
p.stdin.write("position fen 4k3/8/8/8/8/8/8/4K3 b - - 0 1\nkeys\n")
p.stdin.flush()
key_e8 = None
while True:
    line = p.stdout.readline().strip()
    if line.startswith("Current pos key:"):
        key_e8 = line.split()[-1]
        break

# Test 2: Black King on d8
p.stdin.write("position fen 3k4/8/8/8/8/8/8/4K3 b - - 0 1\nkeys\n")
p.stdin.flush()
key_d8 = None
while True:
    line = p.stdout.readline().strip()
    if line.startswith("Current pos key:"):
        key_d8 = line.split()[-1]
        break

p.stdin.write("quit\n")
p.stdin.flush()

if key_e8 != key_d8:
    print(f"SUCCESS: Keys are different! (e8: {key_e8}, d8: {key_d8})")
else:
    print(f"FAIL: Keys are the same! ({key_e8})")
