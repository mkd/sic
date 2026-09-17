import subprocess
with open("/Users/claudio/Library/CloudStorage/GoogleDrive-claudiomkd@gmail.com/My Drive/projects/Chess/sic/src/search.cpp", "r") as f:
    sic = f.read()

with open("/Users/claudio/Library/CloudStorage/GoogleDrive-claudiomkd@gmail.com/My Drive/projects/Chess/gargantua/src/search.cpp", "r") as f:
    gar = f.read()

print("Sic len:", len(sic))
print("Gargantua len:", len(gar))
