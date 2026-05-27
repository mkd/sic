#pragma once

#include <string>

void uci_init();
void uci_loop();
bool uci_execute_line(const std::string& line);
