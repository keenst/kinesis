#version 330 core
layout (location = 0) in vec3 a_pos;
layout (location = 1) in vec3 a_normal;
layout (location = 2) in vec3 a_color;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 normal;
out vec3 frag_pos;
out vec3 color;

void main() {
	gl_Position = vec4(a_pos, 1) * model * view * projection;
	frag_pos = vec3(model * vec4(a_pos, 1));
	normal = a_normal;
	color = a_color;
}
