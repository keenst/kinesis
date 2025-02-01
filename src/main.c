#include "glad/gl.h"
#include "shader.h"
#include "matrix.h"
#include "math_ops.h"
#include "math_helper.h"
#include "win32_time.h"
#include <stdbool.h>
#include <stdio.h>

typedef struct {
	int index;

	Mat3 scale;
	Mat3 orientation;
	Vec3 position;

	// Gets updated on integration
	Mat4 transform;

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

enum { MANIFOLD_POINTS = 8 };

typedef struct {
	int num_points;
	// Contact points in local space
	Vec3 points[MANIFOLD_POINTS];
	Vec3 normals[MANIFOLD_POINTS];
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

void startup(int argc, char** argv) {
	// Init cubes
	CUBES[0].position = new_vec3(0, 20, 0);
	CUBES[0].scale = mat3_scale(&MAT3_IDENTITY, CUBE_SCALE);
	CUBES[0].orientation = mat3_rotate(&MAT3_IDENTITY, 40, new_vec3(0, 3, 4));
	CUBES[0].inertia.m[0][0] = 1.f / 6 * CUBE_MASS * CUBE_SCALE.x * CUBE_SCALE.x;
	CUBES[0].inertia.m[1][1] = 1.f / 6 * CUBE_MASS * CUBE_SCALE.x * CUBE_SCALE.x;
	CUBES[0].inertia.m[2][2] = 1.f / 6 * CUBE_MASS * CUBE_SCALE.x * CUBE_SCALE.x;
	CUBES[0].inverse_inertia = mat3_inverse(&CUBES[0].inertia);
	CUBES[0].index = 0;
	ACTIVE_CUBES[0] = true;

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
	Mat3 model_mat3 = mat3_mul(&cube->scale, &cube->orientation);
	Mat4 transform = mat3_to_mat4(&model_mat3);
	transform = mat4_translate(&transform, cube->position);
	cube->transform = transform;
}

// Checks for collisions and contacts.
// t = 0 is start of frame,
// t = DELTA_TIME is end of frame
bool collision_check(ContactManifold* const contact_manifold, Cube* const cube, const float t) {
	if (contact_manifold) {
		*contact_manifold = (ContactManifold){};
	}

	Mat4 cube_transform = cube->transform;
	if (t != 0) {
		Cube cube_copy = *cube;
		integrate_cube(&cube_copy, t);
		cube_transform = cube_copy.transform;
	}

	bool no_collisions = true;

	// Check all corners against the floor
	for (int j = 0; j < 8; j++) {
		const float x = (j & 1) ? 0.5f : -0.5f;
		const float y = (j & 2) ? 0.5f : -0.5f;
		const float z = (j & 4) ? 0.5f : -0.5f;
		const Vec4 world_point = vec4_mul_mat4(new_vec4(x, y, z, 1), &cube_transform);

		if (world_point.y < COLLISION_DIST_TOLERANCE) {
			buffer_collision_point(vec4_to_vec3(world_point));
			no_collisions = false;

			if (contact_manifold) {
				contact_manifold->points[contact_manifold->num_points] = new_vec3(x, y, z);
				contact_manifold->normals[contact_manifold->num_points] = new_vec3(0, 1, 0);
				contact_manifold->depths[contact_manifold->num_points] = world_point.y;
				contact_manifold->num_points++;
			}
		}

	}

	return !no_collisions;
}

void main_loop() {
	// Start frame timer
	const double pre_draw_time_ms = get_time_ms();

	// Collision detection
	for (int i = 0; i < MAX_CUBES; i++) {
		if (!ACTIVE_CUBES[i]) {
			continue;
		}

		if (cube_is_resting(CUBES[i].index)) {
			continue;
		}

		Cube* const cube = &CUBES[i];

		double t0 = 0;
		double t1 = DELTA_TIME;
		double t_mid = 0;
		ContactManifold contact_manifold = {};
		while (t1 - t0 > COLLISION_TIME_TOLERANCE) {
			t_mid = (t0 + t1) / 2;

			if (collision_check(&contact_manifold, cube, (float)t_mid)) {
				t1 = t_mid;
			} else {
				t0 = t_mid;
			}

		}

		//printf("iterations: %i\n", num_iterations);

		if (contact_manifold.num_points > 2) {
			if (vec3_length(cube->velocity) < 1 && vec3_length(cube->angular_velocity) < .3f) {
				RESTING_CUBES[i] = true;
			}
		}

		// Caculate impulses
		Vec3 total_linear_impulse = {};
		Vec3 total_angular_impulse = {};

		const float time_of_impact = t_mid;

		integrate_cube(cube, time_of_impact);
		for (int j = 0; j < contact_manifold.num_points; j++) {
			const Vec3 local_collision_point = contact_manifold.points[j];
			const Vec3 collision_normal = contact_manifold.normals[j];


			// Linear friction
			/*
			const Vec3 linear_friction_force = vec3_scale(vec3_normalize(cube.velocity), normal_force * -LINEAR_FRICTION_COEFFICIENT / vec3_length(CUBES[i].velocity));
			const Vec3 linear_friction_acceleration = vec3_div(linear_friction_force, CUBE_MASS);
			cube.velocity = vec3_add(CUBES[i].velocity, vec3_scale(linear_friction_acceleration, time_of_impact));
			*/

			// Normal impulse
			const Vec3 local_point_velocity = vec3_cross(cube->angular_velocity, local_collision_point);
			const Vec3 point_velocity = vec3_add(cube->velocity, local_point_velocity);
			const float numerator = vec3_dot(vec3_scale(point_velocity, -(1 + COEFFICIENT_OF_RESTITUTION)), collision_normal);

			const Vec3 mass_part = vec3_scale(collision_normal, 1 / CUBE_MASS);
			const Vec3 inertia_part = vec3_cross(vec3_mul_mat3(vec3_cross(local_collision_point, collision_normal), &cube->inverse_inertia), local_collision_point);
			const float denominator = vec3_dot(collision_normal, mass_part) + vec3_dot(collision_normal, inertia_part);

			const float normal_impulse_magnitude = numerator / denominator;
			const Vec3 normal_impulse = vec3_scale(collision_normal, normal_impulse_magnitude);

			// Tangential impulse
			const Vec3 contact_velocity = vec3_add(cube->velocity, vec3_cross(cube->angular_velocity, local_collision_point));
			// TODO: Change this to actually calculate the tangential plane
			const Vec3 tangential_velocity = new_vec3(contact_velocity.x, 0, contact_velocity.z);

			const float tangential_impulse_magnitude_max = LINEAR_FRICTION_COEFFICIENT * normal_impulse_magnitude;
			const float tangential_impulse_magnitude = fmin(vec3_length(tangential_velocity), tangential_impulse_magnitude_max);
			const Vec3 tangential_impulse = vec3_scale(vec3_normalize(tangential_velocity), -tangential_impulse_magnitude);

			// Sum impulses
			total_linear_impulse = vec3_add(total_linear_impulse, vec3_div(vec3_add(normal_impulse, tangential_impulse), CUBE_MASS));
			total_angular_impulse = vec3_add(total_angular_impulse, vec3_mul_mat3(vec3_cross(local_collision_point, normal_impulse), &cube->inverse_inertia));
		}

		// Collision response
		if (contact_manifold.num_points > 0) {
			// Apply impulses
			cube->velocity = vec3_add(cube->velocity, vec3_div(total_linear_impulse, contact_manifold.num_points));
			cube->angular_velocity = vec3_add(cube->angular_velocity, vec3_div(total_angular_impulse, contact_manifold.num_points));

			for (int j = 0; j < contact_manifold.num_points; j++) {
				// Correct penetration
				cube->position = vec3_add(cube->position, vec3_scale(contact_manifold.normals[j], -contact_manifold.depths[j]));

				// Torsional friction
				// FIX: I think this is missing something (normal force?)
				Vec3 torsional_friction_torque = vec3_scale(contact_manifold.normals[j], -TORSIONAL_FRICTION_COEFFICIENT);
				const Vec3 torsional_friction_torque_limit = vec3_div(vec3_mul_mat3(cube->angular_velocity, &cube->inertia), time_of_impact);
				if (vec3_length(torsional_friction_torque) > vec3_length(torsional_friction_torque_limit)) {
					torsional_friction_torque = vec3_scale(vec3_normalize(torsional_friction_torque), vec3_length(torsional_friction_torque_limit));
				}

				const Vec3 torsional_friction_acceleration = vec3_mul_mat3(vec3_mul_mat3(torsional_friction_torque, &cube->orientation), &cube->inverse_inertia);
				cube->angular_velocity = vec3_add(cube->angular_velocity, vec3_scale(torsional_friction_acceleration, time_of_impact));
			}
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

	//draw_collision_points();

	// Update delta time
	const double post_draw_time_ms = get_time_ms();
	const double elapsed_ms = post_draw_time_ms - pre_draw_time_ms;

	const double sleep_amount = 1000.0 / 60 - 1 / elapsed_ms;
	sleep_ms(fmax(sleep_amount, 0));
}
