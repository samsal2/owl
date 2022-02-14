#version 450

layout (location = 0) in vec3 in_position;

// FIXME(samuel): no need to reuse the set layout
layout (set = 0, binding = 0) uniform UBO {
  mat4 projection;
  mat4 view;
  mat4 model;
} ubo;


layout (location = 0) out vec3 out_uvw;

void main() {
  out_uvw = in_position;
  out_uvw.xy *= -1.0F;
  gl_Position = (ubo.projection * ubo.view * vec4(in_position, 1.0F));
}
