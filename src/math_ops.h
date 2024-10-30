#pragma once

#include "matrix.h"

// Mat4
Mat4 mat4_mul(const Mat4 a, const Mat4 b);
Mat4 mat4_scale(const Mat4* const matrix, const Vec3* const vector);
Mat4 mat4_translate(const Mat4 matrix, const Vec3 vector);

// Vec3
Vec3 vec3_add(const Vec3* const a, const Vec3* const b);
Vec3 vec3_sub(Vec3 a, const Vec3 b);
Vec3 vec3_cross(const Vec3 a, const Vec3 b);
Vec3 vec3_scale(const Vec3* const a, const float b);
float vec3_dot(const Vec3 a, const Vec3 b);

// Vec4
Vec4 vec4_mul_mat4(const Vec4* const vec4, const Mat4* const mat4);
