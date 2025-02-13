#include "math_ops.h"

Vec2 vec2_sub(const Vec2 a, const Vec2 b) {
	return (Vec2){
		a.x - b.x,
		a.y - b.y
	};
}

Vec3 vec3_add(const Vec3 a, const Vec3 b) {
	return (Vec3){
		a.x + b.x,
		a.y + b.y,
		a.z + b.z
	};
}

Vec3 vec3_sub(Vec3 a, const Vec3 b) {
	a.x -= b.x;
	a.y -= b.y;
	a.z -= b.z;
	return a;
}

float vec3_dot(const Vec3 a, const Vec3 b) {
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 vec3_cross(const Vec3 a, const Vec3 b) {
	Vec3 c;
	c.x = a.y * b.z - a.z * b.y;
	c.y = a.z * b.x - a.x * b.z;
	c.z = a.x * b.y - a.y * b.x;
	return c;
}

Vec3 vec3_scale(const Vec3 a, const float b) {
	return (Vec3){
		a.x * b,
		a.y * b,
		a.z * b
	};
}

Vec3 vec3_div(const Vec3 a, const float b) {
	return (Vec3){
		a.x / b,
		a.y / b,
		a.z / b
	};
}

Vec3 vec3_mul_mat3(const Vec3 a, const Mat3* const b) {
	return (Vec3){
		a.x * b->m[0][0] + a.y * b->m[0][1] + a.z * b->m[0][2],
		a.x * b->m[1][0] + a.y * b->m[1][1] + a.z * b->m[1][2],
		a.x * b->m[2][0] + a.y * b->m[2][1] + a.z * b->m[2][2]
	};
}

Vec3 vec3_mul_mat4(const Vec3 vec3, const Mat4* const mat4) {
	Vec3 result_vector = {};
	float* const result = (float*)&result_vector;

	for (int i = 0; i < 3; i++) {
		result[i] = mat4->m[i][0] * vec3.x + mat4->m[i][1] * vec3.y + mat4->m[i][2] * vec3.z + mat4->m[i][3];
	}

	return result_vector;
}

Mat3 mat3_mul(const Mat3* const a, const Mat3* const b) {
	Mat3 result = {};

	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 3; j++) {
			for (int k = 0; k < 3; k++) {
				result.m[i][j] += a->m[i][k] * b->m[k][j];
			}
		}
	}

	return result;
}

Mat3 mat3_mul_float(const Mat3* const matrix, const float scalar) {
	Mat3 result;

	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 3; j++) {
			result.m[i][j] = matrix->m[i][j] * scalar;
		}
	}

	return result;
}

Mat4 mat4_mul(const Mat4 a, const Mat4 b) {
	Mat4 result = {};

	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			for (int k = 0; k < 4; k++) {
				result.m[i][j] += a.m[i][k] * b.m[k][j];
			}
		}
	}

	return result;
}

Vec4 vec4_mul_mat4(const Vec4 vec4, const Mat4* const mat4) {
	Vec4 result_vector = {};
	float* const result = (float*)&result_vector;

	for (int i = 0; i < 4; i++) {
		result[i] = mat4->m[i][0] * vec4.x + mat4->m[i][1] * vec4.y + mat4->m[i][2] * vec4.z + mat4->m[i][3] * vec4.w;
	}

	return result_vector;
}
