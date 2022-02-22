#version 450

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec2 in_uv0;
layout (location = 3) in vec2 in_uv1;
layout (location = 4) in vec4 in_joint0;
layout (location = 5) in vec4 in_weight0;

layout (set = 0, binding = 0) uniform UBO 
{
	mat4 projection;
	mat4 model;
	mat4 view;
} ubo;

#define MAX_NUM_JOINTS 128

layout (set = 2, binding = 0) uniform UBONode {
	mat4 matrix;
	mat4 joints[MAX_NUM_JOINTS];
	float joints_count;
} node;

layout (location = 0) out vec3 out_world_pos;
layout (location = 1) out vec3 out_normal;
layout (location = 2) out vec2 out_uv0;
layout (location = 3) out vec2 out_uv1;

out gl_PerVertex
{
	vec4 gl_Position;
};

void main() 
{
	vec4 loc_pos;
	if (node.joints_count > 0.0) {
		// Mesh is skinned
		mat4 skin_matrix = 
			in_weight0.x * node.joints[int(in_joint0.x)] +
			in_weight0.y * node.joints[int(in_joint0.y)] +
			in_weight0.z * node.joints[int(in_joint0.z)] +
			in_weight0.w * node.joints[int(in_joint0.w)];

		loc_pos = ubo.model * node.matrix * skin_matrix * vec4(in_position, 1.0);
		out_normal = normalize(transpose(inverse(mat3(ubo.model * node.matrix * skin_matrix))) * in_normal);
	} else {
		loc_pos = ubo.model * node.matrix * vec4(in_position, 1.0);
		out_normal = normalize(transpose(inverse(mat3(ubo.model * node.matrix))) * in_normal);
	}
	loc_pos.y = -loc_pos.y;
	out_world_pos = loc_pos.xyz / loc_pos.w;
	out_uv0 = in_uv0;
	out_uv1 = in_uv1;
	gl_Position =  ubo.projection * ubo.view * vec4(out_world_pos, 1.0);
}
