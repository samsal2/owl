#version 450

layout (set = 0, binding = 0) uniform UBO {
    mat4 projection;
    mat4 view;
    mat4 model; // UNUSED
} ubo;

layout (location = 0) out vec4 vertex;

// Grid position are in clipped space
vec4 global_vertices[12] = vec4[] (
  vec4(-1, 0,-1, 0);
  vec4(-1, 0, 1, 0);
  vec4( 0, 0, 0, 1);
  vec4(-1, 0, 1, 0);
  vec4( 1, 0, 1, 0);
  vec4( 0, 0, 0, 1);
  vec4( 1, 0, 1, 0);
  vec4( 1, 0,-1, 0);
  vec4( 0, 0, 0, 1);
  vec4( 1, 0,-1, 0);
  vec4(-1, 0,-1, 0);
  vec4( 0, 0, 0, 1);
);

void main() {
  vertex = global_vertices[gl_VertexIndex];
  gl_Position = ubo.projection * ubo.view * ubo.model * vertex; 
}
