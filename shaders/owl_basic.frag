#version 450

layout(set = 1, binding = 0) uniform sampler sampler0;
layout(set = 1, binding = 1) uniform texture2D texture0;

layout(location = 0) in vec3 in_color;
layout(location = 1) in vec2 in_uv;

layout(location = 0) out vec4 out_color;

void
main() {
  out_color = vec4(in_color, 1.0) *
              texture(sampler2D(texture0, sampler0), in_uv);
}
