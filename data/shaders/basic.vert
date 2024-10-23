#version 330 core
layout (location = 0) in vec3 a_pos;
layout (location = 1) in vec3 a_normal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 normal;
out vec3 frag_pos;

void main() {
	gl_Position = vec4(a_pos, 1) * model * view * projection;
	frag_pos = vec3(model * vec4(a_pos, 1));
	normal = a_normal;
}
