#pragma once

#include "matrix.h"

typedef unsigned int Shader;

Shader compile_shader(const char* const vertex_path, const char* const fragment_path);
void shader_set_mat4(Shader shader, const char* const name, const Mat4* const mat4);
void shader_set_vec3(Shader shader, const char* const name, const Vec3* const vec3);
