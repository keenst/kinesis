#include "math_ops.h"

Vec3 vec3_add(const Vec3* const a, const Vec3* const b) {
	return (Vec3){
		a->x + b->x,
		a->y + b->y,
		a->z + b->z
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

Vec3 vec3_scale(const Vec3* const a, const float b) {
	return (Vec3){
		a->x * b,
		a->y * b,
		a->z * b
	};
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

Mat4 mat4_translate(Mat4 matrix, const Vec3 vector) {
	matrix.m[0][3] += vector.x;
	matrix.m[1][3] += vector.y;
	matrix.m[2][3] += vector.z;
	return matrix;
}

Mat4 mat4_scale(const Mat4* const matrix, const Vec3* const vector) {
	Mat4 new_matrix = *matrix;
	new_matrix.m[0][0] = matrix->m[0][0] * vector->x;
	new_matrix.m[1][1] = matrix->m[1][1] * vector->y;
	new_matrix.m[2][2] = matrix->m[2][2] * vector->z;
	return new_matrix;
}

Vec4 vec4_mul_mat4(const Vec4* const vec4, const Mat4* const mat4) {
	Vec4 result_vector = {};
	float* const result = (float*)&result_vector;

	for (int i = 0; i < 4; i++) {
		result[i] = mat4->m[i][0] * vec4->x + mat4->m[i][1] * vec4->y + mat4->m[i][2] * vec4->z + mat4->m[i][3] * vec4->w;
	}

	return result_vector;
}
