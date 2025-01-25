#pragma once

#include "matrix.h"

// Mat3
Mat3 mat3_mul(const Mat3* const a, const Mat3* const b);
Mat3 mat3_mul_float(const Mat3* const matrix, const float scalar);

// Mat4
Mat4 mat4_mul(const Mat4 a, const Mat4 b);

// Vec3
Vec3 vec3_add(const Vec3 a, const Vec3 b);
Vec3 vec3_sub(Vec3 a, const Vec3 b);
Vec3 vec3_cross(const Vec3 a, const Vec3 b);
Vec3 vec3_scale(const Vec3 a, const float b);
Vec3 vec3_div(const Vec3 a, const float b);
float vec3_dot(const Vec3 a, const Vec3 b);
Vec3 vec3_mul_mat3(const Vec3 a, const Mat3* const b);

// Vec4
Vec4 vec4_mul_mat4(const Vec4 vec4, const Mat4* const mat4);
