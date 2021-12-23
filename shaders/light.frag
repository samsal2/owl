#version 450

layout (set = 1, binding = 0) uniform sampler2D tex;

layout (set = 2, binding = 0) uniform UniformBufferObject {
  vec3 light_pos;
  vec3 view_pos;
} ubo;

layout(location = 0) in vec3 in_pos; 
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_uv;

layout(location = 0) out vec4 out_color;

void main() {
  vec3 color = texture(tex, in_uv).rgb;
  vec3 ambient = 0.05F * color; 
  vec3 light_dir = normalize(ubo.light_pos - in_pos);
  vec3 diffuse = max(dot(light_dir, in_normal), 0.0F) * color;
  vec3 view_dir = normalize(ubo.view_pos - in_pos);
  vec3 reflect_dir = reflect(light_dir, in_normal);
  vec3 halfway = normalize(light_dir + view_dir);
  vec3 specular = pow(max(dot(in_normal, halfway), 0.0F), 32.0F) * vec3(0.3);
  out_color = vec4(ambient + diffuse + specular, 1.0F);
}
