#version 450

layout (location = 0) in vec4 vertex;

layout (location = 0) out vec4 out_frag_color;

void main(void) {
  vec2 p = vertex.xz / vertex.w;
  vec2 g = 0.5 * abs(fract(p) - 0.5) / fwidth(p);
  float a = min(min(g.x, g.y), 1.0);
  out_frag_color = vec4(vec3(a), 1.0 - a); 
}

