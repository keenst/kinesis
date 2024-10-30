#pragma once

#include "math.h"

typedef struct {
	float x;
	float y;
	float z;
} Vec3;

typedef struct {
	float x;
	float y;
	float z;
	float w;
} Vec4;

Vec3 new_vec3(const float x, const float y, const float z);
Vec3 vec3_normalize(Vec3 vector);
const float* const vec3_flatten(const Vec3* const vec3);

Vec4 new_vec4(const float x, const float y, const float z, const float w);
