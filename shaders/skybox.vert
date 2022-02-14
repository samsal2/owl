#version 450

layout (location = 0) in vec3 in_position;

layout (binding = 0) uniform UBO {
  mat4 projection;
  mat4 model;
} ubo;


layout (location = 0) out vec3 out_uvw;

void main() {
  out_uvw = in_pos;
  out_uvw.xy *= -1.0;
  gl_Position = ubo.projection * ubo.model * vec4(in_position, 1.0F);
}
