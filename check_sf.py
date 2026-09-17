import urllib.request
url = "https://raw.githubusercontent.com/official-stockfish/Stockfish/master/src/search.cpp"
content = urllib.request.urlopen(url).read().decode('utf-8')

# Extract qsearch function
lines = content.split('\n')
qsearch_start = -1
for i, line in enumerate(lines):
    if "Value qsearch(" in line:
        qsearch_start = i
        break

if qsearch_start != -1:
    print("Found qsearch")
    in_check_code = []
    for i in range(qsearch_start, qsearch_start + 200):
        if "if (inCheck)" in lines[i]:
            for j in range(i, i+20):
                in_check_code.append(lines[j])
            break
    print('\n'.join(in_check_code))
