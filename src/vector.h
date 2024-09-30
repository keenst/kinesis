#pragma once

#include "math.h"

typedef struct {
	float x;
	float y;
	float z;
} Vec3;

Vec3 new_vec3(const float x, const float y, const float z);
Vec3 vec3_normalize(Vec3 vector);
