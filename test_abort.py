with open('src/timeman.cpp', 'r') as f:
    content = f.read()
content = content.replace(
    "stop_search = true;",
    "std::cout << \"STOP SEARCH TRIGGERED: \" << get_time_ms() - start_time << \" >= \" << maximum_time << std::endl; stop_search = true;"
)
with open('src/timeman.cpp', 'w') as f:
    f.write(content)
