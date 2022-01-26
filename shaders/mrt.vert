#version 450

layout (location = 0) in vec4 in_position;
layout (location = 1) in vec3 in_color;
layout (location = 2) in vec2 in_uv;
layout (location = 3) in vec3 in_normal;
layout (location = 4) in vec3 in_tangent;

layout (binding = 0) uniform UBO 
{
	mat4 projection;
	mat4 view;
	mat4 model;
} ubo;

layout (location = 0) out vec3 out_normal;
layout (location = 1) out vec3 out_color;
layout (location = 2) out vec2 out_uv;
layout (location = 3) out vec3 out_world_pos;
layout (location = 4) out vec3 out_tangent;

void main() {
  vec4 tmp = in_position + ubo.instances[gl_InstanceIndex];

  gl_Position = ubo.projection * ubo.view * ubo.model * tmp;

  out_uv = in_uv;

  out_world_pos = vec3(ubo.model * tmp);

  mat3 normal = transpose(inverse(mat3(ubo.model)));
  out_normal = normal * normalize(in_normal);
  out_tangent = normal * normalize(in_tangent);

  out_color = in_color;
}
