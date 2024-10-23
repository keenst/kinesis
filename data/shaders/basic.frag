#version 330 core

uniform vec3 light_dir;
uniform vec3 light_color;
uniform vec3 object_color;

in vec3 normal;
in vec3 frag_pos;

out vec4 frag_color;

void main() {
	// Ambient lighting
	float ambient_strength = 0.3;
	vec3 ambient = ambient_strength * light_color;

	// Diffuse lighting
	vec3 normalized_normal = normalize(normal);
	vec3 normalized_light_dir = normalize(-light_dir);
	float diff = max(dot(normalized_normal, normalized_light_dir), 0);
	vec3 diffuse = diff * light_color;

	// Result
	vec3 result = (ambient + diffuse) * object_color;
	frag_color = vec4(result, 1);
}
