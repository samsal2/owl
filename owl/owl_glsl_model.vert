#version 450

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec2 in_uv0;
layout (location = 3) in vec2 in_uv1;
layout (location = 4) in vec4 in_joints0;
layout (location = 5) in vec4 in_weights0;

layout (set = 0, binding = 0) uniform UBO
{
  mat4 projection;
  mat4 view;
  mat4 model;
  vec4 light_position;
}
ubo;

#define OWL_MODEL_SKIN_MAX_JOINT_COUNT 128

layout (std430, set = 4, binding = 0) readonly buffer SSBO
{
  mat4 matrix;
  mat4 joint_matrices[OWL_MODEL_SKIN_MAX_JOINT_COUNT];
  int  joint_matrice_count;
}
node;

layout (location = 0) out vec3 out_world_position;
layout (location = 1) out vec3 out_normal;
layout (location = 2) out vec2 out_uv0;
layout (location = 3) out vec2 out_uv1;
layout (location = 4) out vec3 out_view;
layout (location = 5) out vec3 out_light;

void
main ()
{
  out_normal = in_normal;
  out_uv0    = in_uv0;
  out_uv1    = in_uv1;

  // Calculate skinned matrix from weights and joint indices of the current
  // vertex
  vec4 local_position;
  if (0 != node.joint_matrice_count) {
    mat4 skin_matrix =
        in_weights0.x * node.joint_matrices[int (in_joints0.x)] +
        in_weights0.y * node.joint_matrices[int (in_joints0.y)] +
        in_weights0.z * node.joint_matrices[int (in_joints0.z)] +
        in_weights0.w * node.joint_matrices[int (in_joints0.w)];

    local_position =
        ubo.model * node.matrix * skin_matrix * vec4 (in_position, 1.0);
    out_normal =
        normalize (transpose (inverse (mat3 (ubo.view * ubo.model *
                                             node.matrix * skin_matrix))) *
                   in_normal);
  } else {
    local_position = ubo.model * node.matrix * vec4 (in_position, 1.0);
    out_normal     = normalize (
            transpose (inverse (mat3 (ubo.view * ubo.model * node.matrix))) *
            in_normal);
  }

  // flip the y coordinate
  local_position.y   = -local_position.y;
  out_world_position = local_position.xyz / local_position.w;
  gl_Position = ubo.projection * ubo.view * vec4 (out_world_position, 1.0);

  vec4 position       = ubo.view * vec4 (in_position, 1.0);
  vec3 light_position = mat3 (ubo.view) * ubo.light_position.xyz;
  out_light           = light_position - position.xyz;
  out_view            = -position.xyz;
}
