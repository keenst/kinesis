#include "vector.h"

Vec3 new_vec3(const float x, const float y, const float z) {
	Vec3 vec3 = { x, y, z };
	return vec3;
}

float vec3_length(const Vec3 vector) {
	return sqrtf(vector.x * vector.x + vector.y * vector.y + vector.z * vector.z);
}

Vec3 vec3_normalize(Vec3 vector) {
	float length = vec3_length(vector);
	if (length > 0) {
		vector.x /= length;
		vector.y /= length;
		vector.z /= length;
	}

	return vector;
}

// Returns the longest vector
Vec3 vec3_max(const Vec3 a, const Vec3 b) {
	if (vec3_length(a) > vec3_length(b)) {
		return a;
	}
	return b;
}

const float* const vec3_flatten(const Vec3* const vec3) {
	return (float*)vec3;
}

Vec3 vec4_to_vec3(const Vec4 vec4) {
	return new_vec3(vec4.x, vec4.y, vec4.z);
}

Vec4 vec3_to_vec4(const Vec3 vec3) {
	return new_vec4(vec3.x, vec3.y, vec3.z, 1);
}

Vec4 new_vec4(const float x, const float y, const float z, const float w) {
	return (Vec4){ x, y, z, w };
}
