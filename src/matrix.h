#pragma once

#include "vector.h"

typedef struct {
	float m[3][3];
} Mat3;

typedef struct {
	float m[4][4];
} Mat4;

static const Mat3 MAT3_IDENTITY = {
	.m = {
		{ 1, 0, 0 },
		{ 0, 1, 0 },
		{ 0, 0, 1 },
	}
};

static const Mat4 MAT4_IDENTITY = {
	.m = {
		{ 1, 0, 0, 0 },
		{ 0, 1, 0, 0 },
		{ 0, 0, 1, 0 },
		{ 0, 0, 0, 1 }
	}
};

Mat3 mat3_inverse(const Mat3* const matrix);
Mat3 mat3_scale(const Mat3* const matrix, const Vec3 vector);
Mat3 mat3_rotate(const Mat3* const matrix, const float angle_deg, Vec3 axis);
Mat4 mat4_rotate(const Mat4 matrix, const float angle_deg, Vec3 axis);
Mat4 mat4_scale(const Mat4* const matrix, const Vec3 vector);
Mat4 mat4_translate(const Mat4* const matrix, const Vec3 vector);
Mat4 mat4_perspective(const float fov_deg, const float aspect_ratio, const float near_clip_plane, const float far_clip_plane);
Mat4 mat4_look_at(const Vec3 eye, const Vec3 center, const Vec3 up);
const float* const mat4_flatten(const Mat4* const matrix);
Mat4 mat3_to_mat4(const Mat3* const mat3);
