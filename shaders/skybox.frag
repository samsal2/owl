#version 450

layout (set = 1, binding = 0) uniform sampler current_sampler;
layout (set = 1, binding = 1) uniform textureCube texture_data;

layout (location = 0) in vec3 in_uvw;

layout (location = 0) out vec4 out_frag_color;

void main() {
	out_frag_color = texture(samplerCube(texture_data, current_sampler), in_uvw);
}
