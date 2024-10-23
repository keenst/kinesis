#include <stdio.h>
#include <stdlib.h>
#include "matrix.h"
#include "glad/gl.h"
#include "shader.h"

const char* const read_source_file(const char* const path) {
	FILE* file;
	fopen_s(&file, path, "rb");

	if (!file) {
		printf("Failed to open shader source file\n");
		return 0;
	}

	// Get the size of the file
	fseek(file, 0, SEEK_END);
	const long file_size = ftell(file);
	rewind(file);

	// Allocate memory for file contents
	char* const content = (char*)malloc(file_size + 1);

	// Read file
	const size_t read_size = fread(content, 1, file_size, file);
	if (read_size != file_size) {
		printf("Something went wrong when reading shader source file\n");
		free(content);
		return 0;
	}

	content[read_size] = '\0';

	return content;
}

Shader compile_shader(const char* const vertex_path, const char* const fragment_path) {
	const char* const vertex_source = read_source_file(vertex_path);
	const char* const fragment_source = read_source_file(fragment_path);

	const unsigned int vertex_shader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertex_shader, 1, &vertex_source, 0);
	glCompileShader(vertex_shader);

	int success;
	char info_log[512];

	glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
	if (!success) {
		glGetShaderInfoLog(vertex_shader, 512, NULL, info_log);
		printf("%s", info_log);
	}

	const unsigned int fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragment_shader, 1, &fragment_source, 0);
	glCompileShader(fragment_shader);

	glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
	if (!success) {
		glGetShaderInfoLog(vertex_shader, 512, NULL, info_log);
		printf("%s", info_log);
	}

	const Shader shader_program = glCreateProgram();

	glAttachShader(shader_program, vertex_shader);
	glAttachShader(shader_program, fragment_shader);
	glLinkProgram(shader_program);

	glGetProgramiv(shader_program, GL_LINK_STATUS, &success);
	if (!success) {
		glGetProgramInfoLog(shader_program, 512, NULL, info_log);
		printf("%s", info_log);
	}

	return shader_program;
}

void shader_set_mat4(Shader shader, const char* const name, const Mat4* const mat4) {
	const int mat4_location = glGetUniformLocation(shader, name);
	glUniformMatrix4fv(mat4_location, 1, GL_FALSE, mat4_flatten(mat4));
}

void shader_set_vec3(Shader shader, const char* const name, const Vec3* const vec3) {
	const int vec3_location = glGetUniformLocation(shader, name);
	glUniform3fv(vec3_location, 1, vec3_flatten(vec3));
}
