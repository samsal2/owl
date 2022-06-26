#version 450

layout (set = 0, binding = 0) uniform UBO {
  vec2 texel_size;
} ubo;

layout (location = 0) in vec2 in_position;

layout (location = 0) out vec2 uv;
layout (location = 1) out vec2 v_left;
layout (location = 2) out vec2 v_right;
layout (location = 3) out vec2 v_top;
layout (location = 4) out vec2 v_bottom;

void main() {
  uv = in_position * 0.5 + 0.5;
  v_left = uv - vec2(ubo.texel_size.x, 0.0);
  v_right = uv + vec2(ubo.texel_size.x, 0.0);
  v_top = uv + vec2(ubo.texel_size.y, 0.0);
  v_bottom = uv - vec2(ubo.texel_size.y, 0.0);
  gl_Position = vec4(in_position, 0.0, 1.0);
}
