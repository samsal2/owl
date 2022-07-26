#version 450

layout(location = 0) in vec3 inPos;

layout(set = 0, binding = 0) uniform UBO
{
	mat4 projection;
	mat4 view; /* unused */
	mat4 model;
}
ubo;

layout(location = 0) out vec3 outUVW;

void main()
{
	outUVW = inPos;
	// Convert cubemap coordinates into Vulkan coordinate space
	outUVW.xy *= -1.0;
	gl_Position = ubo.projection * ubo.view * vec4(inPos.xyz, 1.0);
}
