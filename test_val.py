with open('src/search.cpp', 'r') as f:
    content = f.read()

content = content.replace(
    "                if (val > best_value) {",
    "                if (thread_id == 0) std::cout << \"val=\" << val << \" move=\" << move_to_str(list.moves[i]) << std::endl;\n                if (val > best_value) {"
)
with open('src/search.cpp', 'w') as f:
    f.write(content)
