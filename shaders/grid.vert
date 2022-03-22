#version 450

layout (set = 0, binding = 0) uniform UBO {
    mat4 projection;
    mat4 view;
    mat4 model;
} ubo;

layout (location = 0) out float near;
layout (location = 1) out float far;
layout (location = 2) out vec3 nearPoint;
layout (location = 3) out vec3 farPoint;
layout (location = 4) out mat4 fragView;
layout (location = 8) out mat4 fragProj;

// Grid position are in clipped space
vec3 gridPlane[6] = vec3[] (
    vec3(1, 1, 0), vec3(-1, -1, 0), vec3(-1, 1, 0),
    vec3(-1, -1, 0), vec3(1, 1, 0), vec3(1, -1, 0)
);

vec3 UnprojectPoint(float x, float y, float z) {
    mat4 viewInv = inverse(ubo.view);
    mat4 projInv = inverse(ubo.projection);
    vec4 unprojectedPoint =  viewInv * projInv * vec4(x, y, z, 1.0);
    return unprojectedPoint.xyz / unprojectedPoint.w;
}

void main() {
    vec3 vertex = gridPlane[gl_VertexIndex];
    nearPoint = UnprojectPoint(vertex.x, vertex.y, 0.0).xyz; // unprojecting on the near plane
    farPoint = UnprojectPoint(vertex.x, vertex.y, 1.0).xyz; // unprojecting on the far plane
    gl_Position = vec4(vertex, 1.0); // using directly the clipped coordinates

    near = 0.01;
    far = 0.01;
    fragView = ubo.view;
    fragProj = ubo.projection;
}
