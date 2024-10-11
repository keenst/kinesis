#pragma once

#include "math.h"

typedef struct {
	float x;
	float y;
	float z;
} Vec3;

Vec3 new_vec3(const float x, const float y, const float z);
Vec3 vec3_normalize(Vec3 vector);
Vec3 vec3_sub(Vec3 a, const Vec3 b);
float vec3_dot(const Vec3 a, const Vec3 b);
Vec3 vec3_cross(const Vec3 a, const Vec3 b);
