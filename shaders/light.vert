#version 450

layout(set = 0, binding = 0) uniform UniformBufferObject { 
  mat4 proj; 
  mat4 view;
  mat4 model; /* unused */
} ubo;

layout (location = 0) in vec3 in_pos; 
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec2 in_uv;

layout (location = 0) out vec3 out_pos;
layout (location = 1) out vec3 out_normal;
layout (location = 2) out vec2 out_uv;

void main() {
  out_pos = in_pos; 
  out_normal = in_normal;
  out_uv = in_uv;
  
  gl_Position = ubo.proj * ubo.view * vec4(in_pos, 1.0F);
}
