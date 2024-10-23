#include "glad/gl.h"
#include "shader.h"
#include "matrix.h"
#include <stdbool.h>

Shader SHADER;
unsigned int VAO;

Mat4 VIEW;
Mat4 PROJECTION;

Vec3 LIGHT_COLOR;
Vec3 LIGHT_DIR;
Vec3 OBJECT_COLOR;

#define NUM_CUBES 256
Vec3 CUBES[NUM_CUBES] = {};
bool ACTIVE_CUBES[NUM_CUBES] = {};

void draw_cubes() {
	for (int i = 0; i < NUM_CUBES; i++) {

		if (!ACTIVE_CUBES[i]) {
			continue;
		}

		const Vec3* const cube = &CUBES[i];

		Mat4 model = MAT4_IDENTITY;
		model = mat4_translate(model, *cube);

		shader_set_mat4(SHADER, "model", &model);
		glDrawArrays(GL_TRIANGLES, 0, 36);
	}
}

void startup() {
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

	VIEW = mat4_look_at(new_vec3(0, 10, -10), new_vec3(0, 0, 0), new_vec3(0, 1, 0));

	CUBES[0]= new_vec3(0, 0, 0);
	ACTIVE_CUBES[0] = true;

	CUBES[1] = new_vec3(2, 0, 1);
	ACTIVE_CUBES[1] = true;

	LIGHT_COLOR = new_vec3(1, 1, 1);
	LIGHT_DIR = new_vec3(-0.2f, -0.1f, -0.3f);
	OBJECT_COLOR = new_vec3(0.5f, 0.5f, 0.9f);
}

void main_loop() {
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
