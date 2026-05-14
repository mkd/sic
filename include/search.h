#pragma once

#include "position.h"

Move search_position(Position& pos, int depth, int thread_id);
void init_lmr();
