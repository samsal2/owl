#version 450

layout(set = 1, binding = 0) uniform sampler sampler0;
layout(set = 1, binding = 1) uniform textureCube texture0;

layout(location = 0) in vec3 inUVW;

layout(location = 0) out vec4 outFragColor;

void
main() {
  outFragColor = texture(samplerCube(texture0, sampler0), inUVW);
}
