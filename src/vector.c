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

const float* const vec3_flatten(const Vec3* const vec3) {
	return (float*)vec3;
}
