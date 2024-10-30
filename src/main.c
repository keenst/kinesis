#include "glad/gl.h"
#include "shader.h"
#include "matrix.h"
#include "math_ops.h"
#include <stdbool.h>
#include <stdio.h>

typedef struct {
	Mat4 transform;
	Vec3 velocity;
} Cube;

Shader SHADER;
unsigned int VAO;

Mat4 VIEW;
Mat4 PROJECTION;

static const Vec3 LIGHT_COLOR = { 1, 1, 1 };
static const Vec3 LIGHT_DIR = { -0.2f, -0.1f, -0.3f };
static const Vec3 OBJECT_COLOR = { 0.5f, 0.5f, 0.9f };

static const Vec3 CUBE_SCALE = { 10, 10, 10 };

static const float CUBE_MASS = 10;
static const float DELTA_TIME = 1.f / 60;
static const float COEFFICIENT_OF_RESTITUTION = 0.7f;
static const Vec3 GRAVITY = { 0, -9.81f, 0 };

enum { NUM_CUBES = 256 };
Cube CUBES[NUM_CUBES] = {};
bool ACTIVE_CUBES[NUM_CUBES] = {};

void draw_cubes() {
	for (int i = 0; i < NUM_CUBES; i++) {

		if (!ACTIVE_CUBES[i]) {
			continue;
		}

		shader_set_mat4(SHADER, "model", &CUBES[i].transform);
		glDrawArrays(GL_TRIANGLES, 0, 36);
	}
}

void startup() {
	CUBES[0].transform = mat4_translate(MAT4_IDENTITY, new_vec3(0, 20, 0));
	CUBES[0].transform = mat4_scale(&CUBES[0].transform, &CUBE_SCALE);
	ACTIVE_CUBES[0] = true;

	// Rendering
	glClearColor(0.2f, 0.3f, 0.3f, 1);
	glEnable(GL_DEPTH_TEST);

	const float vertices[] = {
		// Positions		  // Normals
		-0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
		0.5f,  -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
		0.5f,   0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
		0.5f,   0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
		-0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
		-0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,

		-0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
		0.5f,  -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
		0.5f,   0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
		0.5f,   0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
		-0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
		-0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,

		-0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
		-0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
		-0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
		-0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
		-0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
		-0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,

		0.5f,   0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
		0.5f,   0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
		0.5f,  -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
		0.5f,  -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
		0.5f,  -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
		0.5f,   0.5f,  0.5f,  1.0f,  0.0f,  0.0f,

		-0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
		0.5f,  -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
		0.5f,  -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
		0.5f,  -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
		-0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
		-0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,

		-0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
		0.5f,   0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
		0.5f,   0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
		0.5f,   0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
		-0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
		-0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f
	};

	glGenVertexArrays(1, &VAO);
	glBindVertexArray(VAO);

	unsigned int vbo;
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);

	SHADER = compile_shader("data/shaders/basic.vert", "data/shaders/basic.frag");

	PROJECTION = mat4_perspective(45, 800.f / 600, 0.1f, 100);

	VIEW = mat4_look_at(new_vec3(0, 50, -50), new_vec3(0, 0, 0), new_vec3(0, 1, 0));
}

void main_loop() {
	// Collision detection
	for (int i = 0; i < NUM_CUBES; i++) {
		if (!ACTIVE_CUBES[i]) {
			continue;
		}

		// Check corners against floor
		for (int j = 0; j < 8; j++) {
			const float x = (j & 1) ? 0.5f : -0.5f;
			const float y = (j & 2) ? 0.5f : -0.5f;
			const float z = (j & 4) ? 0.5f : -0.5f;
			const Vec4 local_point = new_vec4(x, y, z, 1);
			const Vec4 world_point = vec4_mul_mat4(&local_point, &CUBES[i].transform);

			// If the cube is penetrating the ground, and is moving towards it
			// -> Collision
			if (world_point.y < 0 && CUBES[i].velocity.y < 0) {
				// Linear impulse
				const Vec3 ground_normal = new_vec3(0, 1, 0);
				const float ground_normal_len_sqr = vec3_dot(ground_normal, ground_normal);
				const float impulse_magnitude = (-vec3_dot(CUBES[i].velocity, ground_normal) * (COEFFICIENT_OF_RESTITUTION + 1)) / (ground_normal_len_sqr * (1 / CUBE_MASS));
				const Vec3 impulse_force = vec3_scale(&ground_normal, impulse_magnitude / CUBE_MASS);

				CUBES[i].transform.m[1][3] = CUBE_SCALE.y / 2;
				CUBES[i].velocity = vec3_add(&CUBES[i].velocity, &impulse_force);
			// If the cube is penetrating the ground, but not moving towards it
			// -> Interpenetrating
			} else if (world_point.y <= 0) {
			// If the cube is not penetrating the ground
			// -> No collision
			} else {
			}
		}
	}

	// Apply forces
	for (int i = 0; i < NUM_CUBES; i++) {
		if (!ACTIVE_CUBES[i]) {
			continue;
		}

		// TODO: Put this in some force application thing rather than hard coding
		const Vec3 gravity = vec3_scale(&GRAVITY, DELTA_TIME);
		CUBES[i].velocity = vec3_add(&CUBES[i].velocity, &gravity);

		CUBES[i].transform.m[0][3] += CUBES[i].velocity.x * DELTA_TIME;
		CUBES[i].transform.m[1][3] += CUBES[i].velocity.y * DELTA_TIME;
		CUBES[i].transform.m[2][3] += CUBES[i].velocity.z * DELTA_TIME;
	}

	// Rendering
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	shader_set_mat4(SHADER, "view", &VIEW);
	shader_set_mat4(SHADER, "projection", &PROJECTION);

	shader_set_vec3(SHADER, "light_dir", &LIGHT_DIR);
	shader_set_vec3(SHADER, "light_color", &LIGHT_COLOR);
	shader_set_vec3(SHADER, "object_color", &OBJECT_COLOR);

	glBindVertexArray(VAO);
	glUseProgram(SHADER);

	draw_cubes();
}
