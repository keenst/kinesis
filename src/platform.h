#pragma once

#include <stdbool.h>
#include "vector.h"

typedef struct {
	bool pause;
	bool toggle_wireframe;
	bool reset_simulation;
	bool realtime;
	bool slowmo_2x;
	bool slowmo_3x;
	bool slowmo_4x;
	bool slowmo_5x;

	bool mouse_left;
	Vec2 mouse_pos;
} Inputs;
