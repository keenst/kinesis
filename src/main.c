#include "glad/gl.h"
#include "shader.h"
#include "matrix.h"
#include "math_ops.h"
#include "math_helper.h"
#include "win32_time.h"
#include "main.h"
#include <stdbool.h>
#include <stdio.h>
#include <float.h>
#include <string.h>

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

Vec3 CAMERA_POSITION = { -40, 50, -40 };

Mat4 VIEW;
Mat4 PROJECTION;

Mat4 PLANE_TRANSFORM;

double PREV_TIME_MS = 0;
float TOTAL_TIME_MS = 0;
double DELTA_TIME = 1.f / 60;

bool IS_PAUSED;
bool IS_SLEEPING;
double SLEEP_END_TIME;

bool IS_WIREFRAME;

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

enum { COLLISION_NORMAL_BUFFER_SIZE = 1 };
Vec3 COLLISION_NORMAL_BUFFER[COLLISION_NORMAL_BUFFER_SIZE][2] = {};
Vec3 COLLISION_EDGES_BUFFER[COLLISION_NORMAL_BUFFER_SIZE][4] = {};
unsigned int NEXT_COLLISION_NORMAL_BUFFER_INDEX = 0;
unsigned int NEXT_COLLISION_EDGES_BUFFER_INDNEX = 0;

void buffer_collision_point(const Vec3 point) {
	COLLISION_POINT_BUFFER[NEXT_COLLISION_POINT_BUFFER_INDEX] = point;
	NEXT_COLLISION_POINT_BUFFER_INDEX = (NEXT_COLLISION_POINT_BUFFER_INDEX + 1) % COLLISION_POINT_BUFFER_SIZE;
}

void buffer_collision_normal(const Vec3 position, const Vec3 direction) {
	COLLISION_NORMAL_BUFFER[NEXT_COLLISION_NORMAL_BUFFER_INDEX][0] = position;
	COLLISION_NORMAL_BUFFER[NEXT_COLLISION_NORMAL_BUFFER_INDEX][1] = direction;
	NEXT_COLLISION_NORMAL_BUFFER_INDEX = (NEXT_COLLISION_NORMAL_BUFFER_INDEX + 1) % COLLISION_NORMAL_BUFFER_SIZE;
}

void buffer_collision_edges(const Vec3 edge_a_start, const Vec3 edge_a_dir, const Vec3 edge_b_start, const Vec3 edge_b_dir)  {
	COLLISION_EDGES_BUFFER[NEXT_COLLISION_NORMAL_BUFFER_INDEX][0] = edge_a_start;
	COLLISION_EDGES_BUFFER[NEXT_COLLISION_NORMAL_BUFFER_INDEX][1] = edge_a_dir;
	COLLISION_EDGES_BUFFER[NEXT_COLLISION_NORMAL_BUFFER_INDEX][2] = edge_b_start;
	COLLISION_EDGES_BUFFER[NEXT_COLLISION_NORMAL_BUFFER_INDEX][3] = edge_b_dir;
	NEXT_COLLISION_EDGES_BUFFER_INDNEX = (NEXT_COLLISION_EDGES_BUFFER_INDNEX + 1) % COLLISION_NORMAL_BUFFER_SIZE;
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

void draw_line(const Vec3 start, const Vec3 dir, const Vec3 color) {
	const float vertices[] = {
		0, 0, 0,
		dir.x, dir.y, dir.z
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

void draw_collision_normals() {
	for (int i = 0; i < COLLISION_NORMAL_BUFFER_SIZE; i++) {
		const Vec3 start = COLLISION_NORMAL_BUFFER[i][0];
		const Vec3 end = vec3_scale(COLLISION_NORMAL_BUFFER[i][1], 5);

		draw_line(start, end, new_vec3(1, 0, 1));
	}
}

void draw_collision_edges() {
	for (int i = 0; i < COLLISION_NORMAL_BUFFER_SIZE; i++) {
		const Vec3 edge_a_start = COLLISION_EDGES_BUFFER[i][0];
		const Vec3 edge_a_end = COLLISION_EDGES_BUFFER[i][1];
		const Vec3 edge_b_start = COLLISION_EDGES_BUFFER[i][2];
		const Vec3 edge_b_end = COLLISION_EDGES_BUFFER[i][3];

		draw_line(edge_a_start, edge_a_end, new_vec3(1, 1, 1));
		draw_line(edge_b_start, edge_b_end, new_vec3(1, 1, 1));
	}
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
	const Vec3 inverse_translation = vec3_scale(vec3_mul_mat3(vec3_mul_mat3(cube->position, &inverse_scale), &inverse_orientation), -1);

	Mat3 inverse_transform_mat3 = MAT3_IDENTITY;
	inverse_transform_mat3 = mat3_mul(&inverse_transform_mat3, &inverse_orientation);
	inverse_transform_mat3 = mat3_mul(&inverse_transform_mat3, &inverse_scale);
	Mat4 inverse_transform = mat3_to_mat4(&inverse_transform_mat3);
	inverse_transform = mat4_translate(&inverse_transform, inverse_translation);
	cube->inverse_transform = inverse_transform;
}

void start_simulation() {
	// Reset resting
	memset(RESTING_CUBES, 0, sizeof(RESTING_CUBES));

	// Init cubes
	CUBES[0] = (Cube){};
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

	CUBES[1] = (Cube){};
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
}

void startup(int argc, char** argv) {
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

	start_simulation();
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

void sim_sleep_ms(float time_ms) {
	IS_SLEEPING = true;
	SLEEP_END_TIME = get_time_ms() + time_ms;
}

// Pauses the simulation until the user unpauses
void sim_pause() {
	IS_PAUSED = true;
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

Vec3 lerp_line_segment(const Vec3 start, const Vec3 end, const float t) {
	return vec3_add(start, vec3_scale(end, t));
}

// Returns minimum distance between lines
float closest_points_line_segments(const Vec3 a_start, const Vec3 a_end, const Vec3 b_start, const Vec3 b_end, Vec3* const point_a, Vec3* const point_b) {
	const Vec3 a_dir = vec3_sub(a_end, a_start);
	const Vec3 b_dir = vec3_sub(b_end, b_start);

	// Variables for system of equations
	// ax + by = e
	// cx + dy = f
	const float a = vec3_dot(b_dir, a_dir);
	const float b = -vec3_dot(a_dir, a_dir);
	const float c = vec3_dot(b_dir, b_dir);
	const float d = -vec3_dot(a_dir, b_dir);
	const float e = -vec3_dot(b_start, a_dir) + vec3_dot(a_start, a_dir);
	const float f = -vec3_dot(b_start, b_dir) + vec3_dot(a_start, b_dir);

	// Solve system of equations
	float s = (e * d - b * f) / (a * d - b * c);
	float t = (a * f - e * c) / (a * d - b * c);

	// Clamp to make sure the points are on the line
	s = fminf(fmaxf(s, 0), 1);
	t = fminf(fmaxf(t, 0), 1);

	// Compute points
	*point_a = vec3_add(a_start, vec3_scale(a_dir, t));
	*point_b = vec3_add(b_start, vec3_scale(b_dir, s));

	// Return distance between points
	const Vec3 dist_vec = vec3_sub(*point_b, *point_a);
	return vec3_length(dist_vec);
}

float lowest_cross = FLT_MAX;

bool collision_check_cubes(ContactManifold* const contact_manifold, Cube* const cube_a, Cube* const cube_b, const float t) {
	Mat4 cube_a_transform = cube_a->transform;
	Mat4 cube_b_transform = cube_b->transform;
	Mat4 cube_a_inverse_transform = cube_a->inverse_transform;
	Mat4 cube_b_inverse_transform = cube_b->inverse_transform;
	Mat3 cube_a_orientation = cube_a->orientation;
	Mat3 cube_b_orientation = cube_b->orientation;
	Vec3 cube_a_position = cube_a->position;
	Vec3 cube_b_position = cube_b->position;
	Cube cube_a_copy = *cube_a;
	Cube cube_b_copy = *cube_b;
	if (t != 0) {
		integrate_cube(&cube_a_copy, t);
		cube_a_transform = cube_a_copy.transform;
		cube_a_inverse_transform = cube_a_copy.inverse_transform;
		cube_a_orientation = cube_a_copy.orientation;
		cube_a_position = cube_a_copy.position;

		integrate_cube(&cube_b_copy, t);
		cube_b_transform = cube_b_copy.transform;
		cube_b_inverse_transform = cube_b_copy.inverse_transform;
		cube_b_orientation = cube_b_copy.orientation;
		cube_b_position = cube_b_copy.position;
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
	normals[6] = vec3_normalize(vec3_cross(normals[0], normals[3]));
	normals[7] = vec3_normalize(vec3_cross(normals[1], normals[3]));
	normals[8] = vec3_normalize(vec3_cross(normals[2], normals[3]));
	normals[9] = vec3_normalize(vec3_cross(normals[0], normals[4]));
	normals[10] = vec3_normalize(vec3_cross(normals[1], normals[4]));
	normals[11] = vec3_normalize(vec3_cross(normals[2], normals[4]));
	normals[12] = vec3_normalize(vec3_cross(normals[0], normals[5]));
	normals[13] = vec3_normalize(vec3_cross(normals[1], normals[5]));
	normals[14] = vec3_normalize(vec3_cross(normals[2], normals[5]));

	const Vec3 vertices[8] = {
		{ 0.5f, -0.5f, -0.5f }, { -0.5f, -0.5f, -0.5f }, { -0.5f, -0.5f, 0.5f }, { 0.5f, -0.5f, 0.5f },
		{ 0.5f, 0.5f, -0.5f }, { -0.5f, 0.5f, -0.5f }, { -0.5f, 0.5f, 0.5f }, { 0.5f, 0.5f, 0.5f }
	};

	// Vertex indices of all edge pairs on a cube
	const int edge_indices[12][2] = {
		{ 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 }, // Bottom face
		{ 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 }, // Top face
		{ 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 } // Connecting the faces
	};

	typedef enum {
		CORNER_TO_FACE,
		EDGE_TO_EDGE
	} CollisionType;

	CollisionType collision_type;
	Vec3 min_penetration_axis; // This is the same as collision normal
	float min_penetration_depth = FLT_MAX;
	const Cube* penetrated_cube; // The cube whose face is min_penetration_axis

	// Values for edge-to-edge collisiosn
	Vec3 edge_a_start;
	Vec3 edge_a_end;
	Vec3 edge_b_start;
	Vec3 edge_b_end;

	// Project the corners of both cubes onto all normals
	for (int normal_index = 0; normal_index < 15; normal_index++) {
		float a_min = FLT_MAX;
		float b_min = FLT_MAX;
		float a_max = -FLT_MAX;
		float b_max = -FLT_MAX;

		for (int vertex_index = 0; vertex_index < 8; vertex_index++) {
			const Vec3 vertex = vertices[vertex_index];

			const Vec4 world_point_a = vec4_mul_mat4(vec3_to_vec4(vertex), &cube_a_transform);
			const Vec4 world_point_b = vec4_mul_mat4(vec3_to_vec4(vertex), &cube_b_transform);

			const float projection_a = vec3_dot(vec4_to_vec3(world_point_a), normals[normal_index]);
			const float projection_b = vec3_dot(vec4_to_vec3(world_point_b), normals[normal_index]);

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

		// If corner-to-face collision
		if (normal_index < 6) {
			// Find axis of minimum penetration
			const float penetration_depth = fminf(fabsf(a_max - b_min), fabsf(b_max - a_min));
			if (penetration_depth < min_penetration_depth) {
				min_penetration_depth = penetration_depth;
				min_penetration_axis = normals[normal_index];
				collision_type = CORNER_TO_FACE;
				if (normal_index < 3) {
					penetrated_cube = &cube_a_copy;
				} else {
					penetrated_cube = &cube_b_copy;
				}
			}
		// If edge-to-edge collision
		} else {
			// Find vectors parallel to collision edges
			Vec3 normal_a; // Vector parallel to the collision edges on cube a
			Vec3 normal_b;

			normal_a = normals[(normal_index - 6) % 3];
			if (normal_index - 6 < 3) {
				normal_b = normals[3];
			} else if (normal_index - 6 < 6) {
				normal_b = normals[4];
			} else {
				normal_b = normals[5];
			}

			// Find the edges that are parallel to the normals
			Vec3 edges_a[4][2];
			Vec3 edges_b[4][2];
			int num_edges_a = 0;
			int num_edges_b = 0;
			for (int edge_index = 0; edge_index < 12; edge_index++) {
				const Vec3 start_vertex = vertices[edge_indices[edge_index][0]];
				const Vec3 end_vertex = vertices[edge_indices[edge_index][1]];

				const Vec3 start_vertex_a = vec3_mul_mat4(start_vertex, &cube_a_transform);
				const Vec3 end_vertex_a = vec3_mul_mat4(end_vertex, &cube_a_transform);
				const Vec3 start_vertex_b = vec3_mul_mat4(start_vertex, &cube_b_transform);
				const Vec3 end_vertex_b = vec3_mul_mat4(end_vertex, &cube_b_transform);

				const Vec3 edge_a = vec3_sub(end_vertex_a, start_vertex_a);
				const Vec3 edge_b = vec3_sub(end_vertex_b, start_vertex_b);

				const Vec3 cross_product_a = vec3_cross(normal_a, edge_a);
				if (fabsf(vec3_length(cross_product_a)) < 0.001) {
					const Vec3 edge_start = vec4_to_vec3(vec4_mul_mat4(vec3_to_vec4(vertices[edge_indices[edge_index][0]]), &cube_a_transform));
					const Vec3 edge_end = vec4_to_vec3(vec4_mul_mat4(vec3_to_vec4(vertices[edge_indices[edge_index][1]]), &cube_a_transform));
					edges_a[num_edges_a][0] = edge_start;
					edges_a[num_edges_a++][1] = edge_end;
				}

				const Vec3 cross_product_b = vec3_cross(normal_b, edge_b);
				if (fabsf(vec3_length(cross_product_b)) < 0.001) {
					const Vec3 edge_start = vec4_to_vec3(vec4_mul_mat4(vec3_to_vec4(vertices[edge_indices[edge_index][0]]), &cube_b_transform));
					const Vec3 edge_end = vec4_to_vec3(vec4_mul_mat4(vec3_to_vec4(vertices[edge_indices[edge_index][1]]), &cube_b_transform));
					edges_b[num_edges_b][0] = edge_start;
					edges_b[num_edges_b++][1] = edge_end;
				}
				int x = 5;
			}

			// Find edges of collision
			Vec3 potential_edge_a_start;
			Vec3 potential_edge_a_end;
			Vec3 potential_edge_b_start;
			Vec3 potential_edge_b_end;

			float min_distance = FLT_MAX;
			for (int edge_index_a = 0; edge_index_a < num_edges_a; edge_index_a++) {
				const Vec3 start_vertex_a = edges_a[edge_index_a][0];
				const Vec3 end_vertex_a = edges_a[edge_index_a][1];

				for (int edge_index_b = 0; edge_index_b < num_edges_b; edge_index_b++) {
					const Vec3 start_vertex_b = edges_b[edge_index_b][0];
					const Vec3 end_vertex_b = edges_b[edge_index_b][1];

					Vec3 point_a, point_b;
					const float distance = closest_points_line_segments(start_vertex_a, end_vertex_a, start_vertex_b, end_vertex_b, &point_a, &point_b);

					if (distance < min_distance) {
						min_distance = distance;

						potential_edge_a_start = start_vertex_a;
						potential_edge_a_end = end_vertex_a;
						potential_edge_b_start = start_vertex_b;
						potential_edge_b_end = end_vertex_b;
					}
				}
			}

			if (min_distance < min_penetration_depth) {
				collision_type = EDGE_TO_EDGE;

				min_penetration_depth = min_distance;

				// Check if normal should be flipped
				const Vec3 displacement = vec3_sub(cube_b_position, cube_a_position);
				if (vec3_dot(displacement, normals[normal_index]) > 0) {
					min_penetration_axis = vec3_scale(normals[normal_index], -1);
				} else {
					min_penetration_axis = normals[normal_index];
				}

				edge_a_start = potential_edge_a_start;
				edge_a_end = potential_edge_a_end;
				edge_b_start = potential_edge_b_start;
				edge_b_end = potential_edge_b_end;
			}
		}
	}

	Vec3 contact_point_a;
	Vec3 contact_point_b;
	float max_penetration_depth = -FLT_MAX;

	// Find the contact points
	if (collision_type == CORNER_TO_FACE) {
		printf("face\n");
		// Use the minimum penetration axis to calculate the point of contact
		// Find the corner of the penetrating cube that is furthest along the collision normal
		Vec3 contact_point;
		for (int vertex_index = 0; vertex_index < 8; vertex_index++) {
			const Vec3 local_point = vertices[vertex_index];

			const float penetration_depth = vec3_dot(local_point, min_penetration_axis);
			if (penetration_depth > max_penetration_depth) {
				max_penetration_depth = penetration_depth;
				const Vec4 local_point_h = vec3_to_vec4(local_point);
				const Vec4 world_point_h = vec4_mul_mat4(local_point_h, &penetrated_cube->transform);
				contact_point = vec4_to_vec3(world_point_h);
			}
		}

		contact_point_a = vec4_to_vec3(vec4_mul_mat4(vec3_to_vec4(contact_point), &cube_a_inverse_transform));
		contact_point_b = vec4_to_vec3(vec4_mul_mat4(vec3_to_vec4(contact_point), &cube_b_inverse_transform));

		max_penetration_depth = min_penetration_depth;
	} else {
		printf("edge:\n");
		Vec3 point_a, point_b;
		max_penetration_depth = -closest_points_line_segments(edge_a_start, edge_a_end, edge_b_start, edge_b_end, &point_a, &point_b);
		const Vec3 a_dir = vec3_sub(edge_a_end, edge_a_start);
		const Vec3 b_dir = vec3_sub(edge_b_end, edge_b_start);
		/*
		printf("edge a: %.2f %.2f %.2f -> %.2f %.2f %.2f (%.2f %.2f %.2f) %.2f\n", edge_a_start.x, edge_a_start.y, edge_a_start.z, edge_a_end.x, edge_a_end.y, edge_a_end.z, a_dir.x, a_dir.y, a_dir.z, vec3_length(a_dir));
		printf("edge b: %.2f %.2f %.2f -> %.2f %.2f %.2f (%.2f %.2f %.2f) %.2f\n", edge_b_start.x, edge_b_start.y, edge_b_start.z, edge_b_end.x, edge_b_end.y, edge_b_end.z, b_dir.x, b_dir.y, b_dir.z, vec3_length(b_dir));
		*/

		// Convert points to local space
		const Vec3 local_point_a = vec4_to_vec3(vec4_mul_mat4(vec3_to_vec4(point_a), &cube_a_inverse_transform));
		const Vec3 local_point_b = vec4_to_vec3(vec4_mul_mat4(vec3_to_vec4(point_b), &cube_b_inverse_transform));

		contact_point_a = local_point_a;
		contact_point_b = local_point_b;
	}

	if (contact_manifold->num_points >= MANIFOLD_POINTS) {
		printf("Overflow of manifold contact points\n");
	}

	contact_manifold->local_points_a[contact_manifold->num_points] = contact_point_a;
	contact_manifold->local_points_b[contact_manifold->num_points] = contact_point_b;
	contact_manifold->depths[contact_manifold->num_points] = max_penetration_depth;
	contact_manifold->cube_a = cube_a;
	contact_manifold->cube_b = cube_b;
	contact_manifold->normal = min_penetration_axis;
	contact_manifold->num_points++;

	if (collision_type == EDGE_TO_EDGE) {
		buffer_collision_edges(edge_a_start, vec3_sub(edge_a_end, edge_a_start), edge_b_start, vec3_sub(edge_b_end, edge_b_start));
		//sim_sleep_ms(1000);
	}

	buffer_collision_normal(penetrated_cube->position, min_penetration_axis);

	printf("depth: %f\n", max_penetration_depth);
	//sim_pause();

	/*
	printf("point a: %f %f %f\n", contact_point_a.x, contact_point_a.y, contact_point_a.z);
	printf("point b: %f %f %f\n", contact_point_b.x, contact_point_b.y, contact_point_b.z);
	*/
	//printf("normal: %f %f %f\n", min_penetration_axis.x, min_penetration_axis.y, min_penetration_axis.z);
	//printf("num_points: %d\n", contact_manifold->num_points);

	return true;
}

void physics_step() {
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
		printf("new collision check\n");
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
}

void main_loop(const Inputs old_inputs, const Inputs inputs) {
	// Start frame timer
	const double pre_draw_time_ms = get_time_ms();

	// If orbiting
	if (inputs.mouse_left) {
		const Vec2 mouse_delta = vec2_sub(inputs.mouse_pos, old_inputs.mouse_pos);
		Mat3 rotation = MAT3_IDENTITY;
		rotation = mat3_rotate(&rotation, -mouse_delta.x, new_vec3(0, 1, 0));
		const Vec3 y_axis = vec3_normalize(vec3_cross(new_vec3(0, 1, 0), CAMERA_POSITION));
		rotation = mat3_rotate(&rotation, -mouse_delta.y, y_axis);
		CAMERA_POSITION = vec3_mul_mat3(CAMERA_POSITION, &rotation);
	}

	if (inputs.pause && !old_inputs.pause) {
		IS_PAUSED = !IS_PAUSED;
	}

	if (inputs.toggle_wireframe && !old_inputs.toggle_wireframe) {
		IS_WIREFRAME = !IS_WIREFRAME;
	}

	if (inputs.reset_simulation && !old_inputs.reset_simulation) {
		start_simulation();
	}

	VIEW = mat4_look_at(CAMERA_POSITION, new_vec3(0, 0, 0), new_vec3(0, 1, 0));

	// Physics
	if (IS_SLEEPING && get_time_ms() > SLEEP_END_TIME) {
		IS_SLEEPING = false;
		physics_step();
	} else if (!IS_SLEEPING && !IS_PAUSED) {
		physics_step();
	}

	// Rendering
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glBindVertexArray(CUBE_VAO);
	glUseProgram(BASIC_SHADER);

	shader_set_mat4(BASIC_SHADER, "view", &VIEW);
	shader_set_mat4(BASIC_SHADER, "projection", &PROJECTION);

	shader_set_vec3(BASIC_SHADER, "light_dir", &LIGHT_DIR);
	shader_set_vec3(BASIC_SHADER, "light_color", &LIGHT_COLOR);

	if (IS_WIREFRAME) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	}
	draw_cubes();
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glBindVertexArray(PLANE_VAO);
	shader_set_mat4(BASIC_SHADER, "model", &PLANE_TRANSFORM);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	//draw_collision_points();
	//draw_cube_vectors();
	/*
	draw_collision_normals();
	draw_collision_edges();
	*/

	// Update delta time
	const double post_draw_time_ms = get_time_ms();
	const double elapsed_ms = post_draw_time_ms - pre_draw_time_ms;

	const double sleep_amount = 1000.0 / 60 - 1 / elapsed_ms;
	sleep_ms(fmax(sleep_amount, 0));
}
