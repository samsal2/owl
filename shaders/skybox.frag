#version 450

layout (set = 1, binding = 0) uniform samplerCube sampler_cube_map;

layout (location = 0) in vec3 in_uvw;

layout (location = 0) out vec4 out_frag_color;

void main() {
	out_frag_color = texture(sampler_cube_map, in_uvw);
}
