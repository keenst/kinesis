#include "glad/gl.h"
#include "shader.h"
#include "matrix.h"
#include "math_ops.h"
#include "math_helper.h"
#include "win32_time.h"
#include <stdbool.h>
#include <stdio.h>
#include <float.h>

typedef struct {
	int index;

	Mat3 scale;
	Mat3 orientation;
	Vec3 position;

	// Gets updated on integration
	Mat4 transform;
	Mat4 inverse_transform;

	Vec3 velocity;
	Vec3 angular_velocity;
	Vec3 torque;
	Mat3 inertia;
	Mat3 inverse_inertia;
} Cube;

typedef struct {
	// TODO: The cube pointer could be replaced by the cube's index
	Cube* cube;
	Vec3 point;
	Vec3 normal;
	float penetration_depth;
} Contact;

enum { MANIFOLD_POINTS = 16 };

typedef struct {
	int num_points;
	Vec3 normal;
	Cube* cube_a;
	Cube* cube_b;
	//Vec3 world_points[MANIFOLD_POINTS]; // Contact points in world space
	Vec3 local_points_a[MANIFOLD_POINTS];
	Vec3 local_points_b[MANIFOLD_POINTS];
	float depths[MANIFOLD_POINTS];
} ContactManifold;

Shader BASIC_SHADER;
Shader POINT_SHADER;

unsigned int CUBE_VAO;
unsigned int PLANE_VAO;
unsigned int COLLISION_POINTS_VAO;
unsigned int LINE_VAO;
unsigned int LINE_VBO;

Mat4 VIEW;
Mat4 PROJECTION;

Mat4 PLANE_TRANSFORM;

double PREV_TIME_MS = 0;
float TOTAL_TIME_MS = 0;
double DELTA_TIME = 1.f / 60;

bool IS_FIRST_FRAME = true;

static const Vec3 LIGHT_COLOR = { 1, 1, 1 };
static const Vec3 LIGHT_DIR = { -0.2f, -0.1f, -0.3f };

static const Vec3 CUBE_SCALE = { 5, 5, 5 };

static const float CUBE_MASS = 5;
static const float COEFFICIENT_OF_RESTITUTION = 0.7f;
static const Vec3 GRAVITY = { 0, -9.81f, 0 };

static const float COLLISION_DIST_TOLERANCE = 0.01f;
static const double COLLISION_TIME_TOLERANCE = 0.00001f;
static const float ANGULAR_DAMPING_FACTOR = 0.999f;
static const float TORSIONAL_FRICTION_COEFFICIENT = 0.01f;
static const float LINEAR_FRICTION_COEFFICIENT = 0.2f;

enum { MAX_CUBES = 256 };
Cube CUBES[MAX_CUBES] = {};
bool ACTIVE_CUBES[MAX_CUBES] = {};
bool RESTING_CUBES[MAX_CUBES] = {};

enum { MAX_CONTACTS = 256 };
Contact CONTACTS[MAX_CONTACTS] = {};
bool ACTIVE_CONTACTS[MAX_CONTACTS] = {};

enum { COLLISION_POINT_BUFFER_SIZE = 5 };
Vec3 COLLISION_POINT_BUFFER[COLLISION_POINT_BUFFER_SIZE] = {};
unsigned int NEXT_COLLISION_POINT_BUFFER_INDEX = 0;

void buffer_collision_point(const Vec3 point) {
	COLLISION_POINT_BUFFER[NEXT_COLLISION_POINT_BUFFER_INDEX] = point;
	NEXT_COLLISION_POINT_BUFFER_INDEX = (NEXT_COLLISION_POINT_BUFFER_INDEX + 1) % COLLISION_POINT_BUFFER_SIZE;
}

void draw_collision_points() {
	glBindVertexArray(COLLISION_POINTS_VAO);
	glUseProgram(POINT_SHADER);

	shader_set_mat4(POINT_SHADER, "view", &VIEW);
	shader_set_mat4(POINT_SHADER, "projection", &PROJECTION);
	const Vec3 color = new_vec3(1, 0, 0);
	shader_set_vec3(POINT_SHADER, "color", &color);

	for (int i = 0; i < COLLISION_POINT_BUFFER_SIZE; i++) {
		const Mat4 model = mat4_translate(&MAT4_IDENTITY, COLLISION_POINT_BUFFER[i]);
		shader_set_mat4(POINT_SHADER, "model", &model);
		glDrawArrays(GL_POINTS, 0, 1);
	}
}

void draw_cubes() {
	for (int i = 0; i < MAX_CUBES; i++) {
		if (!ACTIVE_CUBES[i]) {
			continue;
		}

		shader_set_mat4(BASIC_SHADER, "model", &CUBES[i].transform);
		glDrawArrays(GL_TRIANGLES, 0, 36);
	}
}

void draw_line(const Vec3 start, const Vec3 end, const Vec3 color) {
	const float vertices[] = {
		0, 0, 0,
		end.x, end.y, end.z
	};

	glBindVertexArray(LINE_VAO);
	glUseProgram(POINT_SHADER);

	const Mat4 model = mat4_translate(&MAT4_IDENTITY, start);

	shader_set_vec3(POINT_SHADER, "color", &color);
	shader_set_mat4(POINT_SHADER, "view", &VIEW);
	shader_set_mat4(POINT_SHADER, "projection", &PROJECTION);
	shader_set_mat4(POINT_SHADER, "model", &model);

	glBindBuffer(GL_ARRAY_BUFFER, LINE_VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	glDrawArrays(GL_LINES, 0, 2);
}

void draw_cube_vectors() {
	for (int i = 0; i < MAX_CUBES; i++) {
		if (!ACTIVE_CUBES[i]) {
			continue;
		}

		const Cube cube = CUBES[i];
		draw_line(cube.position, cube.velocity, new_vec3(0, 0.5, 1));
		draw_line(cube.position, cube.angular_velocity, new_vec3(1, 0.6f, 0.6f));
	}
}

void update_transform(Cube* const cube) {
	// Compute transformation matrix
	Mat4 transform = MAT4_IDENTITY;
	transform = mat4_translate(&transform, cube->position);
	transform = mat4_mul(transform, mat3_to_mat4(&cube->orientation));
	transform = mat4_mul(transform, mat3_to_mat4(&cube->scale));
	cube->transform = transform;

	// Compute inverse transformation matrix
	const Mat3 inverse_orientation = mat3_inverse(&cube->orientation);
	const Mat3 inverse_scale = mat3_inverse(&cube->scale);
	const Vec3 inverse_translation = vec3_scale(vec3_mul_mat3(vec3_mul_mat3(cube->position, &inverse_scale), &cube->orientation), -1);

	Mat3 inverse_transform_mat3 = MAT3_IDENTITY;
	inverse_transform_mat3 = mat3_mul(&inverse_transform_mat3, &inverse_orientation);
	inverse_transform_mat3 = mat3_mul(&inverse_transform_mat3, &inverse_scale);
	Mat4 inverse_transform = mat3_to_mat4(&inverse_transform_mat3);
	inverse_transform = mat4_translate(&inverse_transform, inverse_translation);
	cube->inverse_transform = inverse_transform;
}

void startup(int argc, char** argv) {
	// Init cubes
	CUBES[0].position = new_vec3(0, 20, 0);
	CUBES[0].scale = mat3_scale(&MAT3_IDENTITY, CUBE_SCALE);
	//CUBES[0].orientation = mat3_rotate(&MAT3_IDENTITY, 0, new_vec3(0, 1, 0));
	CUBES[0].orientation = mat3_rotate(&MAT3_IDENTITY, 45, new_vec3(0, 1, 0));
	CUBES[0].inertia.m[0][0] = 1.f / 6 * CUBE_MASS * CUBE_SCALE.x * CUBE_SCALE.x;
	CUBES[0].inertia.m[1][1] = 1.f / 6 * CUBE_MASS * CUBE_SCALE.x * CUBE_SCALE.x;
	CUBES[0].inertia.m[2][2] = 1.f / 6 * CUBE_MASS * CUBE_SCALE.x * CUBE_SCALE.x;
	CUBES[0].inverse_inertia = mat3_inverse(&CUBES[0].inertia);
	CUBES[0].index = 0;
	update_transform(&CUBES[0]);
	ACTIVE_CUBES[0] = true;

	CUBES[1].position = new_vec3(0, 30, 0);
	//CUBES[1].position = new_vec3(0, 10, 10);
	CUBES[1].scale = mat3_scale(&MAT3_IDENTITY, CUBE_SCALE);
	CUBES[1].orientation = mat3_rotate(&MAT3_IDENTITY, 45, new_vec3(1, 1, 1));
	//CUBES[1].orientation = mat3_rotate(&MAT3_IDENTITY, 0, new_vec3(0, 3, 4));
	CUBES[1].inertia.m[0][0] = 1.f / 6 * CUBE_MASS * CUBE_SCALE.x * CUBE_SCALE.x;
	CUBES[1].inertia.m[1][1] = 1.f / 6 * CUBE_MASS * CUBE_SCALE.x * CUBE_SCALE.x;
	CUBES[1].inertia.m[2][2] = 1.f / 6 * CUBE_MASS * CUBE_SCALE.x * CUBE_SCALE.x;
	CUBES[1].inverse_inertia = mat3_inverse(&CUBES[0].inertia);
	CUBES[1].index = 1;
	update_transform(&CUBES[1]);
	ACTIVE_CUBES[1] = true;

	// Init plane
	PLANE_TRANSFORM = mat4_scale(&MAT4_IDENTITY, new_vec3(50, 1, 50));

	// Rendering
	//glClearColor(0.1f, 0.2f, 0.3f, 1);
	glClearColor(0.2f, 0.2f, 0.2f, 1);
	glEnable(GL_DEPTH_TEST);
	glPointSize(10);

	// Cube data
	const float cube_vertices[] = {
		// Positions		// Normals		   	// Colors
		-0.5, -0.5, -0.5,  	0.0,  0.0, -1.0,  	0.0, 0.0, 1.0,
		0.5,  -0.5, -0.5,  	0.0,  0.0, -1.0,  	0.0, 0.0, 1.0,
		0.5,   0.5, -0.5,  	0.0,  0.0, -1.0,  	0.0, 0.0, 1.0,
		0.5,   0.5, -0.5,  	0.0,  0.0, -1.0,  	0.0, 0.0, 1.0,
		-0.5,  0.5, -0.5,  	0.0,  0.0, -1.0,  	0.0, 0.0, 1.0,
		-0.5, -0.5, -0.5,  	0.0,  0.0, -1.0,  	0.0, 0.0, 1.0,

		-0.5, -0.5,  0.5,  	0.0,  0.0,  1.0,  	0.0, 0.0, 1.0,
		0.5,  -0.5,  0.5,  	0.0,  0.0,  1.0,  	0.0, 0.0, 1.0,
		0.5,   0.5,  0.5,  	0.0,  0.0,  1.0,  	0.0, 0.0, 1.0,
		0.5,   0.5,  0.5,  	0.0,  0.0,  1.0,  	0.0, 0.0, 1.0,
		-0.5,  0.5,  0.5,  	0.0,  0.0,  1.0,  	0.0, 0.0, 1.0,
		-0.5, -0.5,  0.5,  	0.0,  0.0,  1.0,  	0.0, 0.0, 1.0,

		-0.5,  0.5,  0.5,	-1.0,  0.0,  0.0,  	0.0, 1.0, 0.0,
		-0.5,  0.5, -0.5,	-1.0,  0.0,  0.0,  	0.0, 1.0, 0.0,
		-0.5, -0.5, -0.5,	-1.0,  0.0,  0.0,  	0.0, 1.0, 0.0,
		-0.5, -0.5, -0.5,	-1.0,  0.0,  0.0,  	0.0, 1.0, 0.0,
		-0.5, -0.5,  0.5,	-1.0,  0.0,  0.0,  	0.0, 1.0, 0.0,
		-0.5,  0.5,  0.5,	-1.0,  0.0,  0.0,  	0.0, 1.0, 0.0,

		0.5,   0.5,  0.5, 	1.0,  0.0,  0.0,  	0.0, 1.0, 0.0,
		0.5,   0.5, -0.5,  	1.0,  0.0,  0.0,  	0.0, 1.0, 0.0,
		0.5,  -0.5, -0.5,  	1.0,  0.0,  0.0,  	0.0, 1.0, 0.0,
		0.5,  -0.5, -0.5,  	1.0,  0.0,  0.0,  	0.0, 1.0, 0.0,
		0.5,  -0.5,  0.5,  	1.0,  0.0,  0.0,  	0.0, 1.0, 0.0,
		0.5,   0.5,  0.5,  	1.0,  0.0,  0.0,  	0.0, 1.0, 0.0,

		-0.5, -0.5, -0.5,  	0.0, -1.0,  0.0,  	1.0, 0.0, 0.0,
		0.5,  -0.5, -0.5,  	0.0, -1.0,  0.0,  	1.0, 0.0, 0.0,
		0.5,  -0.5,  0.5,  	0.0, -1.0,  0.0,  	1.0, 0.0, 0.0,
		0.5,  -0.5,  0.5,  	0.0, -1.0,  0.0,  	1.0, 0.0, 0.0,
		-0.5, -0.5,  0.5,  	0.0, -1.0,  0.0,  	1.0, 0.0, 0.0,
		-0.5, -0.5, -0.5,  	0.0, -1.0,  0.0,  	1.0, 0.0, 0.0,

		-0.5,  0.5, -0.5,  	0.0,  1.0,  0.0,  	1.0, 0.0, 0.0,
		0.5,   0.5, -0.5,  	0.0,  1.0,  0.0,  	1.0, 0.0, 0.0,
		0.5,   0.5,  0.5,  	0.0,  1.0,  0.0,  	1.0, 0.0, 0.0,
		0.5,   0.5,  0.5,  	0.0,  1.0,  0.0,  	1.0, 0.0, 0.0,
		-0.5,  0.5,  0.5,  	0.0,  1.0,  0.0,  	1.0, 0.0, 0.0,
		-0.5,  0.5, -0.5,  	0.0,  1.0,  0.0,  	1.0, 0.0, 0.0
	};

	// Cube buffers
	glGenVertexArrays(1, &CUBE_VAO);
	glBindVertexArray(CUBE_VAO);

	unsigned int cube_vbo;
	glGenBuffers(1, &cube_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, cube_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(cube_vertices), cube_vertices, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(6 * sizeof(float)));
	glEnableVertexAttribArray(2);

	// Plane data
	/*
	const float plane_vertices[] = {
		// Position		Normals		Colors
		-0.5, 0, -0.5,	0, 1, 0,	0.39f, 0.58f, 0.93f,
		0.5,  0, -0.5,	0, 1, 0,	0.39f, 0.58f, 0.93f,
		-0.5, 0, 0.5,	0, 1, 0,	0.39f, 0.58f, 0.93f,

		-0.5, 0, 0.5,	0, 1, 0,	0.39f, 0.58f, 0.93f,
		0.5,  0, -0.5,	0, 1, 0,	0.39f, 0.58f, 0.93f,
		0.5,  0, 0.5,	0, 1, 0,	0.39f, 0.58f, 0.93f
	};
	*/

	const float plane_vertices[] = {
		// Position		Normals		Colors
		-0.5, 0, -0.5,	0, 1, 0,	1, 1, 1,
		0.5,  0, -0.5,	0, 1, 0,	1, 1, 1,
		-0.5, 0, 0.5,	0, 1, 0,	1, 1, 1,

		-0.5, 0, 0.5,	0, 1, 0,	1, 1, 1,
		0.5,  0, -0.5,	0, 1, 0,	1, 1, 1,
		0.5,  0, 0.5,	0, 1, 0,	1, 1, 1
	};

	// Plane buffers
	glGenVertexArrays(1, &PLANE_VAO);
	glBindVertexArray(PLANE_VAO);

	unsigned int plane_vbo;
	glGenBuffers(1, &plane_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, plane_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(plane_vertices), plane_vertices, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(6 * sizeof(float)));
	glEnableVertexAttribArray(2);

	// Collision point data
	const float collision_point_vertices[] = {
		0, 0, 0
	};

	// Collision point buffers
	glGenVertexArrays(1, &COLLISION_POINTS_VAO);
	glBindVertexArray(COLLISION_POINTS_VAO);

	unsigned int collision_points_vbo;
	glGenBuffers(1, &collision_points_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, collision_points_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(Vec3), collision_point_vertices, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);

	// Line buffers
	glGenVertexArrays(1, &LINE_VAO);
	glBindVertexArray(LINE_VAO);

	glGenBuffers(1, &LINE_VBO);
	glBindBuffer(GL_ARRAY_BUFFER, LINE_VBO);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);

	// Resources
	BASIC_SHADER = compile_shader("data/shaders/basic.vert", "data/shaders/basic.frag");
	POINT_SHADER = compile_shader("data/shaders/point.vert", "data/shaders/point.frag");

	PROJECTION = mat4_perspective(45, 800.f / 600, 0.1f, 1000);

	VIEW = mat4_look_at(new_vec3(-40, 50, -40), new_vec3(0, 0, 0), new_vec3(0, 1, 0));
	//VIEW = mat4_look_at(new_vec3(-40, 0, -40), new_vec3(0, 0, 0), new_vec3(0, 1, 0));
	//VIEW = mat4_look_at(new_vec3(1, 60, 0), new_vec3(0, 0, 0), new_vec3(0, 1, 0));
}

bool cube_is_resting(const int index) {
	/*
	for (int i = 0; i < MAX_CONTACTS; i++) {
		if (!ACTIVE_CONTACTS[i]) {
			continue;
		}

		if (CONTACTS[i].cube->index == index) {
			return true;
		}

	}
	*/

	if (RESTING_CUBES[index]) {
		return true;
	}

	return false;
}

void add_contact(Cube* const cube, const Vec3 contact_point, const Vec3 contact_normal, const float penetration_depth) {
	if (cube_is_resting(cube->index)) {
		return;
	}

	for (int i = 0; i < MAX_CONTACTS; i++) {
		if (ACTIVE_CONTACTS[i]) {
			continue;
		}

		Contact new_contact;
		new_contact.cube = cube;
		new_contact.point = contact_point;
		new_contact.normal = contact_normal;
		new_contact.penetration_depth = penetration_depth;

		CONTACTS[i] = new_contact;
		ACTIVE_CONTACTS[i] = true;

		return;
	}

	// TODO: Maybe create a system for handling errors?
	printf("Warning: Contact pool overflow\n");
}

void remove_contact(const int contact_index) {
	ACTIVE_CONTACTS[contact_index] = false;
}

// Integrates a cube by t
void integrate_cube(Cube* const cube, const float t) {
	// Dampen angular velocity
	cube->angular_velocity = vec3_scale(cube->angular_velocity, 1 - ANGULAR_DAMPING_FACTOR * t);

	// Integrate linear
	cube->velocity = vec3_add(cube->velocity, vec3_scale(GRAVITY, t));
	cube->position = vec3_add(cube->position, vec3_scale(cube->velocity, t));

	// Integrate angular
	const Vec3 axis = vec3_normalize(cube->angular_velocity);
	const float angle = vec3_length(cube->angular_velocity);
	cube->orientation = mat3_rotate(&cube->orientation, deg(angle) * t, axis);

	// Update transform matrix
	update_transform(cube);
}

// Checks for collisions and contacts.
// t = 0 is start of frame,
// t = DELTA_TIME is end of frame
bool collision_check_floor(ContactManifold* const contact_manifold, Cube* const cube, const float t) {
	Mat4 cube_transform = cube->transform;
	if (t != 0) {
		Cube cube_copy = *cube;
		integrate_cube(&cube_copy, t);
		cube_transform = cube_copy.transform;
	}

	bool no_collisions = true;

	// Check all corners against the floor
	for (int i = 0; i < 8; i++) {
		const float x = (i & 1) ? 0.5f : -0.5f;
		const float y = (i & 2) ? 0.5f : -0.5f;
		const float z = (i & 4) ? 0.5f : -0.5f;
		const Vec4 world_point_h = vec4_mul_mat4(new_vec4(x, y, z, 1), &cube_transform);

		if (world_point_h.y < COLLISION_DIST_TOLERANCE) {
			no_collisions = false;

			if (contact_manifold) {
				contact_manifold->local_points_a[contact_manifold->num_points] = new_vec3(x, y, z);
				contact_manifold->depths[contact_manifold->num_points] = world_point_h.y;
				contact_manifold->normal = new_vec3(0, 1, 0);
				contact_manifold->num_points++;
				contact_manifold->cube_a = cube;

				if (contact_manifold->num_points >= MANIFOLD_POINTS) {
					printf("Overflow of manifold contact points\n");
				}
			}
		}

	}

	return !no_collisions;
}

bool collision_check_cubes(ContactManifold* const contact_manifold, Cube* const cube_a, Cube* const cube_b, const float t) {
	Mat4 cube_a_transform = cube_a->transform;
	Mat4 cube_b_transform = cube_b->transform;
	Mat3 cube_a_orientation = cube_a->orientation;
	Mat3 cube_b_orientation = cube_b->orientation;
	if (t != 0) {
		Cube cube_a_copy = *cube_a;
		integrate_cube(&cube_a_copy, t);
		cube_a_transform = cube_a_copy.transform;
		cube_a_orientation = cube_a_copy.orientation;

		Cube cube_b_copy = *cube_b;
		integrate_cube(&cube_b_copy, t);
		cube_b_transform = cube_b_copy.transform;
		cube_b_orientation = cube_b_copy.orientation;
	}

	Vec3 normals[15];
	// Face normals
	normals[0] = vec3_normalize(vec3_mul_mat3(new_vec3(1, 0, 0), &cube_a_orientation));
	normals[1] = vec3_normalize(vec3_mul_mat3(new_vec3(0, 1, 0), &cube_a_orientation));
	normals[2] = vec3_normalize(vec3_mul_mat3(new_vec3(0, 0, 1), &cube_a_orientation));
	normals[3] = vec3_normalize(vec3_mul_mat3(new_vec3(1, 0, 0), &cube_b_orientation));
	normals[4] = vec3_normalize(vec3_mul_mat3(new_vec3(0, 1, 0), &cube_b_orientation));
	normals[5] = vec3_normalize(vec3_mul_mat3(new_vec3(0, 0, 1), &cube_b_orientation));
	// Edge normals (cross products between edges on both cubes)
	/*
	normals[6] = vec3_normalize(vec3_cross(normals[0], normals[3]));
	normals[7] = vec3_normalize(vec3_cross(normals[1], normals[3]));
	normals[8] = vec3_normalize(vec3_cross(normals[2], normals[3]));
	normals[9] = vec3_normalize(vec3_cross(normals[0], normals[4]));
	normals[10] = vec3_normalize(vec3_cross(normals[1], normals[4]));
	normals[11] = vec3_normalize(vec3_cross(normals[2], normals[4]));
	normals[12] = vec3_normalize(vec3_cross(normals[0], normals[5]));
	normals[13] = vec3_normalize(vec3_cross(normals[1], normals[5]));
	normals[14] = vec3_normalize(vec3_cross(normals[2], normals[5]));
	*/

	Vec3 min_penetration_axis; // This is the same as collision normal
	float min_penetration_depth = FLT_MAX;
	const Cube* penetrated_cube; // The cube whose face is min_penetration_axis

	// Project the corners of both cubes onto all normals
	for (int i = 0; i < 6; i++) {
		float a_min = FLT_MAX;
		float b_min = FLT_MAX;
		float a_max = -FLT_MAX;
		float b_max = -FLT_MAX;

		for (int j = 0; j < 8; j++) {
			const float x = (j & 1) ? 0.5f : -0.5f;
			const float y = (j & 2) ? 0.5f : -0.5f;
			const float z = (j & 4) ? 0.5f : -0.5f;
			const Vec4 world_point_a = vec4_mul_mat4(new_vec4(x, y, z, 1), &cube_a_transform);
			const Vec4 world_point_b = vec4_mul_mat4(new_vec4(x, y, z, 1), &cube_b_transform);

			const float projection_a = vec3_dot(vec4_to_vec3(world_point_a), normals[i]);
			const float projection_b = vec3_dot(vec4_to_vec3(world_point_b), normals[i]);

			if (projection_a < a_min) {
				a_min = projection_a;
			} else if (projection_a > a_max) {
				a_max = projection_a;
			}

			if (projection_b < b_min) {
				b_min = projection_b;
			} else if (projection_b > b_max) {
				b_max = projection_b;
			}
		}

		// Look for separation along axis
		if (a_max <= b_min || b_max <= a_min) {
			return false;
		}

		// Find axis of minimum penetration
		const float penetration_depth = fminf(fabsf(a_max - b_min), fabsf(b_max - a_min));
		if (penetration_depth < min_penetration_depth) {
			min_penetration_depth = penetration_depth;
			min_penetration_axis = normals[i];
			if (i < 3) {
				penetrated_cube = cube_a;
			} else {
				penetrated_cube = cube_b;
			}
		}
	}

	// Use the minimum penetration axis to calculate the point of contact
	// Find the corner of the penetrating cube that is furthest along the collision normal
	float max_penetration_depth = -FLT_MAX;
	Vec3 contact_point;
	for (int i = 0; i < 8; i++) {
		const float x = (i & 1) ? 0.5f : -0.5f;
		const float y = (i & 2) ? 0.5f : -0.5f;
		const float z = (i & 4) ? 0.5f : -0.5f;
		const Vec3 local_point = new_vec3(x, y, z);

		const float penetration_depth = vec3_dot(local_point, min_penetration_axis);
		if (penetration_depth > max_penetration_depth) {
			max_penetration_depth = penetration_depth;
			const Vec4 local_point_h = new_vec4(x, y, z, 1);
			const Vec4 world_point_h = vec4_mul_mat4(local_point_h, &penetrated_cube->transform);
			contact_point = vec4_to_vec3(world_point_h);
		}
	}

	if (contact_manifold->num_points >= MANIFOLD_POINTS) {
		printf("Overflow of manifold contact points\n");
	}

	const Vec3 contact_point_a = vec4_to_vec3(vec4_mul_mat4(vec3_to_vec4(contact_point), &cube_a->inverse_transform));
	const Vec3 contact_point_b = vec4_to_vec3(vec4_mul_mat4(vec3_to_vec4(contact_point), &cube_b->inverse_transform));

	contact_manifold->local_points_a[contact_manifold->num_points] = contact_point_a;
	contact_manifold->local_points_b[contact_manifold->num_points] = contact_point_b;
	contact_manifold->depths[contact_manifold->num_points] = max_penetration_depth;
	contact_manifold->cube_a = cube_a;
	contact_manifold->cube_b = cube_b;
	contact_manifold->normal = min_penetration_axis;
	contact_manifold->num_points++;

	return true;
}

void main_loop() {
	// Start frame timer
	const double pre_draw_time_ms = get_time_ms();

	// Collision detection
	ContactManifold contact_manifolds[255];
	int num_manifolds = 0;

	float times_of_impact[MAX_CUBES] = {}; // Store the earliest time of impact for each cube

	for (int i = 0; i < MAX_CUBES; i++) {
		if (!ACTIVE_CUBES[i] || cube_is_resting(i)) {
			continue;
		}

		float earliest_time_of_impact = FLT_MAX;

		Cube* const cube = &CUBES[i];

		double t0 = 0;
		double t1 = DELTA_TIME;
		double t_mid = 0;
		ContactManifold contact_manifold;
		while (t1 - t0 > COLLISION_TIME_TOLERANCE) {
			contact_manifold = (ContactManifold){};

			t_mid = (t0 + t1) / 2;

			bool collision = false;

			if (collision_check_floor(&contact_manifold, cube, (float)t_mid)) {
				collision = true;
			}

			for (int j = i + 1; j < MAX_CUBES; j++) {
				if (!ACTIVE_CUBES[j] || cube_is_resting(j)) {
					continue;
				}

				if (collision_check_cubes(&contact_manifold, cube, &CUBES[j], (float)t_mid)) {
					collision = true;
				}
			}

			if (collision) {
				t1 = t_mid;
			} else {
				t0 = t_mid;
			}
		}

		if (contact_manifold.num_points > 2) {
			if (vec3_length(cube->velocity) < 1 && vec3_length(cube->angular_velocity) < .3f) {
				RESTING_CUBES[i] = true;
				continue;
			}
		}

		// If a collision occurred
		if (contact_manifold.num_points > 0) {
			if (num_manifolds >= 255) {
				printf("Overflow of manifolds\n");
			}

			contact_manifolds[num_manifolds++] = contact_manifold;
			if (earliest_time_of_impact > t_mid) {
				earliest_time_of_impact = t_mid;
				times_of_impact[i] = t_mid;
			}
		}
	}


	// Apply post-bisection integration
	for (int i = 0; i < MAX_CUBES; i++) {
		if (ACTIVE_CUBES[i]) {
			integrate_cube(&CUBES[i], times_of_impact[i]);
		}
	}

	// Caculate impulses
	Vec3 total_linear_impulses[MAX_CUBES] = {};
	Vec3 total_angular_impulses[MAX_CUBES] = {};

	for (int i = 0; i < num_manifolds; i++) {
		const ContactManifold contact_manifold = contact_manifolds[i];
		const Cube* const cube_a = contact_manifold.cube_a;
		const Cube* const cube_b = contact_manifold.cube_b;

		for (int j = 0; j < contact_manifold.num_points; j++) {
			/*
			const Vec3 world_collision_point = contact_manifold.local_points[j];
			buffer_collision_point(world_collision_point);
			const Vec4 world_collision_point_h = vec3_to_vec4(world_collision_point);
			const Vec3 local_collision_point_a = vec4_to_vec3(vec4_mul_mat4(world_collision_point_h, &cube_a->inverse_transform));
			Vec3 local_collision_point_b;

			if (cube_b) {
				local_collision_point_b = vec4_to_vec3(vec4_mul_mat4(world_collision_point_h, &cube_b->inverse_transform));
			}
			*/

			const Vec3 local_collision_point_a = contact_manifold.local_points_a[j];
			const Vec3 local_collision_point_b = contact_manifold.local_points_b[j];

			const Vec3 collision_normal = contact_manifold.normal;

			// Normal impulse
			float denominator;
			Vec3 relative_velocity;
			// If the collision was between two cubes
			if (contact_manifold.cube_b) {
				relative_velocity = vec3_sub(cube_a->velocity, cube_b->velocity);

				const Vec3 mass_part = vec3_scale(collision_normal, 1 / CUBE_MASS + 1 / CUBE_MASS);
				const Vec3 inertia_part_a = vec3_cross(vec3_mul_mat3(vec3_cross(local_collision_point_a, collision_normal), &cube_a->inverse_inertia), local_collision_point_a);
				const Vec3 inertia_part_b = vec3_cross(vec3_mul_mat3(vec3_cross(local_collision_point_b, collision_normal), &cube_b->inverse_inertia), local_collision_point_b);
				denominator = vec3_dot(collision_normal, mass_part) + vec3_dot(collision_normal, vec3_add(inertia_part_a, inertia_part_b));
			// If the collision was between a cube and the floor
			} else {
				relative_velocity = cube_a->velocity;

				const Vec3 mass_part = vec3_scale(collision_normal, 1 / CUBE_MASS);
				const Vec3 inertia_part = vec3_cross(vec3_mul_mat3(vec3_cross(local_collision_point_a, collision_normal), &cube_a->inverse_inertia), local_collision_point_a);
				denominator = vec3_dot(collision_normal, mass_part) + vec3_dot(collision_normal, inertia_part);
			}

			const float numerator = vec3_dot(vec3_scale(relative_velocity, -(1 + COEFFICIENT_OF_RESTITUTION)), collision_normal);

			const float normal_impulse_magnitude = numerator / denominator;
			const Vec3 normal_impulse_a = vec3_scale(collision_normal, normal_impulse_magnitude);
			const Vec3 normal_impulse_b = vec3_scale(collision_normal, -normal_impulse_magnitude);

			// Tangential impulse
			/*
			const Vec3 contact_velocity = vec3_add(cube->velocity, vec3_cross(cube->angular_velocity, local_collision_point));
			// TODO: Change this to actually calculate the tangential plane
			const Vec3 tangential_velocity = new_vec3(contact_velocity.x, 0, contact_velocity.z);
			*/
			//const Vec3 relative_angular_velocity = vec3_sub(cube_a->angular_velocity, cube_b->angular_velocity);
			Vec3 relative_point_velocity;
			if (contact_manifold.cube_b) {
				const Vec3 local_point_velocity_a = vec3_cross(cube_a->angular_velocity, local_collision_point_a);
				const Vec3 local_point_velocity_b = vec3_cross(cube_b->angular_velocity, local_collision_point_b);
				const Vec3 point_velocity_a = vec3_add(relative_velocity, local_point_velocity_a);
				const Vec3 point_velocity_b = vec3_add(relative_velocity, local_point_velocity_b);
				relative_point_velocity = vec3_sub(point_velocity_a, point_velocity_b);
			} else {
				const Vec3 local_point_velocity = vec3_cross(cube_a->angular_velocity, local_collision_point_a);
				relative_point_velocity = vec3_add(relative_velocity, local_point_velocity);
			}

			const Vec3 tangential_velocity = vec3_sub(relative_point_velocity, vec3_scale(collision_normal, vec3_dot(relative_point_velocity, collision_normal)));

			const float tangential_impulse_magnitude_max = LINEAR_FRICTION_COEFFICIENT * normal_impulse_magnitude;
			const float tangential_impulse_magnitude = fmin(vec3_length(tangential_velocity), tangential_impulse_magnitude_max);
			const Vec3 tangential_impulse_a = vec3_scale(vec3_normalize(tangential_velocity), -tangential_impulse_magnitude);
			const Vec3 tangential_impulse_b = vec3_scale(vec3_normalize(tangential_velocity), tangential_impulse_magnitude);

			// Sum impulses
			Vec3* const linear_impulse_a = &total_linear_impulses[cube_a->index];
			*linear_impulse_a = vec3_add(*linear_impulse_a, vec3_div(vec3_add(normal_impulse_a, tangential_impulse_a), CUBE_MASS));
			Vec3* const angular_impulse_a = &total_angular_impulses[cube_a->index];
			*angular_impulse_a = vec3_add(*angular_impulse_a, vec3_mul_mat3(vec3_cross(local_collision_point_a, normal_impulse_a), &cube_a->inverse_inertia));

			if (contact_manifold.cube_b) {
				Vec3* const linear_impulse_b = &total_linear_impulses[cube_b->index];
				*linear_impulse_b = vec3_add(*linear_impulse_b, vec3_div(vec3_add(normal_impulse_b, tangential_impulse_b), CUBE_MASS));
				Vec3* const angular_impulse_b = &total_angular_impulses[cube_b->index];
				*angular_impulse_b = vec3_add(*angular_impulse_b, vec3_mul_mat3(vec3_cross(local_collision_point_b, normal_impulse_b), &cube_b->inverse_inertia));
			}
		}
	}

	// Collision response
	for (int i = 0; i < num_manifolds; i++) {
		const ContactManifold contact_manifold = contact_manifolds[i];
		Cube* const cube_a = contact_manifold.cube_a;
		Cube* const cube_b = contact_manifold.cube_b;

		// Apply impulses
		cube_a->velocity = vec3_add(cube_a->velocity, vec3_div(total_linear_impulses[cube_a->index], contact_manifold.num_points));
		cube_a->angular_velocity = vec3_add(cube_a->angular_velocity, vec3_div(total_angular_impulses[cube_a->index], contact_manifold.num_points));
		if (contact_manifold.cube_b) {
			cube_b->velocity = vec3_add(cube_b->velocity, vec3_div(total_linear_impulses[cube_b->index], contact_manifold.num_points));
			cube_b->angular_velocity = vec3_add(cube_b->angular_velocity, vec3_div(total_angular_impulses[cube_b->index], contact_manifold.num_points));
		}

		for (int j = 0; j < contact_manifold.num_points; j++) {
			// Correct penetration
			cube_a->position = vec3_add(cube_a->position, vec3_scale(contact_manifold.normal, -contact_manifold.depths[j] / 2));
			if (contact_manifold.cube_b) {
				cube_b->position = vec3_add(cube_b->position, vec3_scale(contact_manifold.normal, contact_manifold.depths[j] / 2));
			}

			// Torsional friction
			// FIX: I think this is missing something (normal force?)
			// FIX: Does this do anything?
			/*
			Vec3 torsional_friction_torque = vec3_scale(contact_manifold.normal, -TORSIONAL_FRICTION_COEFFICIENT);
			const Vec3 torsional_friction_torque_limit = vec3_div(vec3_mul_mat3(cube->angular_velocity, &cube->inertia), time_of_impact);
			if (vec3_length(torsional_friction_torque) > vec3_length(torsional_friction_torque_limit)) {
				torsional_friction_torque = vec3_scale(vec3_normalize(torsional_friction_torque), vec3_length(torsional_friction_torque_limit));
			}

			const Vec3 torsional_friction_acceleration = vec3_mul_mat3(vec3_mul_mat3(torsional_friction_torque, &cube->orientation), &cube->inverse_inertia);
			cube->angular_velocity = vec3_add(cube->angular_velocity, vec3_scale(torsional_friction_acceleration, time_of_impact));
			*/
		}
	}

	// Integrate cubes
	for (int i = 0; i < MAX_CUBES; i++) {
		if (!ACTIVE_CUBES[i]) {
			continue;
		}

		if (cube_is_resting(CUBES[i].index)) {
			continue;
		}

		integrate_cube(&CUBES[i], (float)DELTA_TIME);

	}

	// Rendering
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glBindVertexArray(CUBE_VAO);
	glUseProgram(BASIC_SHADER);

	shader_set_mat4(BASIC_SHADER, "view", &VIEW);
	shader_set_mat4(BASIC_SHADER, "projection", &PROJECTION);

	shader_set_vec3(BASIC_SHADER, "light_dir", &LIGHT_DIR);
	shader_set_vec3(BASIC_SHADER, "light_color", &LIGHT_COLOR);

	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	draw_cubes();
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glBindVertexArray(PLANE_VAO);
	shader_set_mat4(BASIC_SHADER, "model", &PLANE_TRANSFORM);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	draw_collision_points();
	draw_cube_vectors();

	// Update delta time
	const double post_draw_time_ms = get_time_ms();
	const double elapsed_ms = post_draw_time_ms - pre_draw_time_ms;

	const double sleep_amount = 1000.0 / 60 - 1 / elapsed_ms;
	sleep_ms(fmax(sleep_amount, 0));
}
