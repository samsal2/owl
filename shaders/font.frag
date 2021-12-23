#version 450

layout(set = 1, binding = 0) uniform sampler2D tex;

layout(location = 0) in vec3 in_color; 
layout(location = 1) in vec2 in_uv;

layout(location = 0) out vec4 out_color;

void main() {
  out_color = vec4(in_color, 1.0F) * vec4(texture(tex, in_uv).r);
}
