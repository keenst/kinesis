#pragma once

#include "platform.h"

void startup(int argc, char** argv);
void main_loop(const Inputs old_inputs, const Inputs inputs);
void resize_window(int width, int height);
