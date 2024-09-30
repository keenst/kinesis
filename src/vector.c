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
