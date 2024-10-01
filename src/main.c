#include "glad/gl.h"
#include "shader.h"
#include "matrix.h"

Shader SHADER;
unsigned int VAO;

void startup() {
	const float vertices[] = {
		-0.5f, -0.5f,  0.5f,
		 0.5f, -0.5f,  0.5f,
		 0.5f,  0.5f,  0.5f,
		-0.5f,  0.5f,  0.5f,

		-0.5f, -0.5f, -0.5f,
		 0.5f, -0.5f, -0.5f,
		 0.5f,  0.5f, -0.5f,
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
}

void main_loop() {
	glClearColor(0.2f, 0.3f, 0.3f, 1);
	glClear(GL_COLOR_BUFFER_BIT);

	Mat4 view = MAT4_IDENTITY;
	view = mat4_translate(view, new_vec3(0, 0, -3));

	const Mat4 projection = mat4_perspective(45, 800.f / 600, 0.1f, 100);

	Mat4 model = MAT4_IDENTITY;
	model = mat4_rotate(model, -55, new_vec3(1, 0, 0));

	const int model_loc = glGetUniformLocation(SHADER, "model");
	glUniformMatrix4fv(model_loc, 1, GL_FALSE, mat4_flatten(&model));

	const int view_loc = glGetUniformLocation(SHADER, "view");
	glUniformMatrix4fv(view_loc, 1, GL_FALSE, mat4_flatten(&view));

	const int projection_loc = glGetUniformLocation(SHADER, "projection");
	glUniformMatrix4fv(projection_loc, 1, GL_FALSE, mat4_flatten(&projection));

	glBindVertexArray(VAO);
	glUseProgram(SHADER);
	glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
}
