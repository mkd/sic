with open('src/search.cpp', 'r') as f:
    lines = f.readlines()

new_lines = []
for i in range(len(lines)):
    if i > 0 and lines[i].strip() == lines[i-1].strip() and "if (TimeManager::stop_search)" in lines[i]:
        continue
    new_lines.append(lines[i])

with open('src/search.cpp', 'w') as f:
    f.writelines(new_lines)
