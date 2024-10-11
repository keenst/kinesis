#include "matrix.h"
#include "math.h"
#include "math_helper.h"

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

Mat4 mat4_rotate(const Mat4 matrix, const float angle_deg, Vec3 axis) {
	axis = vec3_normalize(axis);

	const float angle_rad = rad(angle_deg);
	const float cos = cosf(angle_rad);
	const float sin = sinf(angle_rad);

	const Mat4 rotation_matrix = {
		{
			{ cos + axis.x * axis.x * (1 - cos),			axis.y * axis.x * (1 - cos) + axis.z * sin,		axis.z * axis.x * (1 - cos) - axis.z * sin, 0 },
			{ axis.x * axis.y * (1 - cos) - axis.z * sin,	cos + axis.y * axis.y * (1 - cos),				axis.z * axis.y * (1 - cos) - axis.x * sin, 0 },
			{ axis.x * axis.z * (1 - cos) + axis.y * sin, 	axis.y * axis.z * (1 - cos) + axis.x * sin,		cos + axis.z * axis.z * (1 - cos),			0 },
			{ 0, 0, 0, 1 }
		}
	};

	return mat4_mul(matrix, rotation_matrix);
}

Mat4 mat4_perspective(const float fov_deg, const float aspect_ratio, const float near_clip_plane, const float far_clip_plane) {
	Mat4 matrix = {};

	const float fov_rad = rad(fov_deg);
	const float tan_half_fov = tanf(fov_rad / 2);

	matrix.m[0][0] = 1 / (aspect_ratio * tan_half_fov);
	matrix.m[1][1] = 1 / tan_half_fov;
	matrix.m[2][2] = -(far_clip_plane + near_clip_plane) / (far_clip_plane - near_clip_plane);
	matrix.m[2][3] = -(2 * far_clip_plane * near_clip_plane) / (far_clip_plane - near_clip_plane);
	matrix.m[3][2] = -1;

	return matrix;
}

Mat4 mat4_look_at(const Vec3 eye, const Vec3 center, Vec3 up) {
	const Vec3 forward = vec3_normalize(vec3_sub(center, eye));
	const Vec3 right = vec3_normalize(vec3_cross(forward, up));
	up = vec3_cross(right, forward);

	const Mat4 view = {
		{
			/*
			{ right.x, up.x, -forward.x, 0 },
			{ right.y, up.y, -forward.y, 0 },
			{ right.z, up.z, -forward.z, 0 },
			{ vec3_dot(right, eye), vec3_dot(up, eye), -vec3_dot(forward, eye), 1 }
			*/
			{ right.x, right.y, right.z, -vec3_dot(right, eye) },
			{ up.x, up.y, up.z, -vec3_dot(up, eye) },
			{ -forward.x, -forward.y, -forward.z, vec3_dot(forward, eye) },
			{ 0, 0, 0, 1 }
		}
	};

	return view;
}

const float* const mat4_flatten(const Mat4* const matrix) {
	return &matrix->m[0][0];
}
