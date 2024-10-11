#include "glad/gl.h"
#include "shader.h"
#include "matrix.h"
#include <stdbool.h>

Shader SHADER;
unsigned int VAO;

Mat4 VIEW;
Mat4 PROJECTION;

typedef struct {
	Vec3 position;
	bool is_active;
} Cube;

#define NUM_CUBES 256
Cube CUBES[NUM_CUBES] = {};

void draw_cubes() {
	for (int i = 0; i < NUM_CUBES; i++) {
		const Cube* const cube = &CUBES[i];

		if (!cube->is_active) {
			continue;
		}

		Mat4 model = MAT4_IDENTITY;
		model = mat4_translate(model, cube->position);

		shader_set_mat4(SHADER, "model", &model);
		glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
	}
}

void startup() {
	glClearColor(0.2f, 0.3f, 0.3f, 1);

	const float vertices[] = {
		-0.5f, -0.5f,  0.5f,
		0.5f,  -0.5f,  0.5f,
		0.5f,   0.5f,  0.5f,
		-0.5f,  0.5f,  0.5f,

		-0.5f, -0.5f, -0.5f,
		0.5f,  -0.5f, -0.5f,
		0.5f,   0.5f, -0.5f,
		-0.5f,  0.5f, -0.5f,
	};

	const unsigned int indices[] = {
		0, 1, 2, 2, 3, 0,
		4, 5, 6, 6, 7, 4,
		4, 0, 3, 3, 7, 4,
		1, 5, 6, 6, 2, 1,
		3, 2, 6, 6, 7, 3,
		4, 5, 1, 1, 0, 4
	};

	glGenVertexArrays(1, &VAO);
	glBindVertexArray(VAO);

	unsigned int vbo;
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	unsigned int ebo;
	glGenBuffers(1, &ebo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);

	SHADER = compile_shader("data/shaders/basic.vert", "data/shaders/basic.frag");

	PROJECTION = mat4_perspective(45, 800.f / 600, 0.1f, 100);

	VIEW = mat4_look_at(new_vec3(0, 10, -10), new_vec3(0, 0, 0), new_vec3(0, 1, 0));

	CUBES[0].position = new_vec3(0, 0, 0);
	CUBES[0].is_active = true;

	CUBES[1].position = new_vec3(2, 0, 1);
	CUBES[1].is_active = true;
}

void main_loop() {
	glClear(GL_COLOR_BUFFER_BIT);

	shader_set_mat4(SHADER, "view", &VIEW);
	shader_set_mat4(SHADER, "projection", &PROJECTION);

	glBindVertexArray(VAO);
	glUseProgram(SHADER);

	draw_cubes();
}
