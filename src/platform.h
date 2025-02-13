#pragma once

#include <stdbool.h>
#include "vector.h"

typedef struct {
	bool pause;
	bool toggle_wireframe;
	bool reset_simulation;
	bool mouse_left;
	Vec2 mouse_pos;
} Inputs;
