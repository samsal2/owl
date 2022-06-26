#version 450

/* using compute shaders is probably a better way to do this, however as 
   im just looking to get something working ill do it using only framebuffer
   attachments */

/* TODO(samuel): use optimal pixel formats for each attribute */
layout (set = 1, binding = 0) uniform sampler sampler0;
layout (set = 1, binding = 1) uniform texture2D velocity0;
layout (set = 1, binding = 2) uniform texture2D velocity1;
layout (set = 1, binding = 3) uniform texture2D dye0;
layout (set = 1, binding = 4) uniform texture2D dye1;
layout (set = 1, binding = 5) uniform texture2D divergence;
layout (set = 1, binding = 6) uniform texture2D curl;
layout (set = 1, binding = 7) uniform texture2D pressure0;
layout (set = 1, binding = 8) uniform texture2D pressure1;

#define OWL_WORKFLOW_ADVECTION 0
#define OWL_WORKFLOW_DIVERGENCE 1
#define OWL_WORKFLOW_CURL 2
#define OWL_WORKFLOW_VORTICITY 3
#define OWL_WORKFLOW_PRESSURE 4
#define OWL_WORKFLOW_GRADIENT_SUBTRACT 5
#define OWL_WORKFLOW_SPLAT 6
#define OWL_SOURCE_VELOCITY 0
#define OWL_SOURCE_DYE 1

layout (set = 2, binding = 0) uniform UBO {
  int workflow;
  int source;
  int velocity_index;
  int dye_index;
  int pressure_index;
  float radius;
  vec3 color;
  vec2 point;
  float aspect_ratio;
  vec2 texel_size;
  float dt;
  float dissipation;
  vec2 dye_texel_size;
  float curl;
} ubo;

layout (location = 0) in vec2 uv;
layout (location = 1) in vec2 v_left;
layout (location = 2) in vec2 v_right;
layout (location = 3) in vec2 v_top;
layout (location = 4) in vec2 v_bottom;

layout (location = 0) out vec4 out_velocity0;
layout (location = 1) out vec4 out_velocity1;
layout (location = 2) out vec4 out_dye0;
layout (location = 3) out vec4 out_dye1;
layout (location = 4) out vec4 out_divergence;
layout (location = 5) out vec4 out_curl;
layout (location = 6) out vec4 out_pressure0;
layout (location = 7) out vec4 out_pressure1;

vec4 bilerp(texture2D tex, vec2 uv, vec2 tsize) {
  vec2 st = uv / tsize - 0.5;
  vec2 iuv = floor(st);
  vec2 fuv = fract(st);
  vec4 a = texture(sampler2D(tex, sampler0), (iuv + vec2(0.5, 0.5)) * tsize);
  vec4 b = texture(sampler2D(tex, sampler0), (iuv + vec2(1.5, 0.5)) * tsize);
  vec4 c = texture(sampler2D(tex, sampler0), (iuv + vec2(0.5, 1.5)) * tsize);
  vec4 d = texture(sampler2D(tex, sampler0), (iuv + vec2(1.5, 1.5)) * tsize);
  return mix(mix(a, b, fuv.x), mix(c, d, fuv.x), fuv.y); 
}

void main() {
  vec4 result;

  switch (ubo.workflow) {
  case OWL_WORKFLOW_SPLAT: {
    float px = (uv.x - ubo.point.x) * ubo.aspect_ratio;
    float py = (uv.y - ubo.point.y);
    vec2 p = vec2(px, py);
    vec3 splat = exp(-dot(p, p) / ubo.radius) * ubo.color;
 
    vec3 base;
    switch (ubo.source) {
    case OWL_SOURCE_VELOCITY: {
      if (0 == ubo.velocity_index)
        base = texture(sampler2D(velocity0, sampler0), uv).xyz;
      else 
        base = texture(sampler2D(velocity1, sampler0), uv).xyz;
    } break;
    case OWL_SOURCE_DYE: {
      if (0 == ubo.dye_index)
        base = texture(sampler2D(dye0, sampler0), uv).xyz;
      else 
        base = texture(sampler2D(dye1, sampler0), uv).xyz;
    } break;

      result = vec4(base + splat, 1.0);
    }
    
  } break;
  case OWL_WORKFLOW_ADVECTION: {
    vec2 velocity;
    if (0 == ubo.velocity_index)
      velocity = texture(sampler2D(velocity0, sampler0), uv).xy;
    else 
      velocity = texture(sampler2D(velocity1, sampler0), uv).xy;
    
    vec2 coord = uv - ubo.dt * velocity * ubo.texel_size;
    float decay = 1.0 + ubo.dissipation * ubo.dt;

    vec4 source;
    switch (ubo.source) {
    case OWL_SOURCE_VELOCITY: {
      if (0 == ubo.velocity_index)
        source = texture(sampler2D(velocity0, sampler0), uv);
      else 
        source = texture(sampler2D(velocity1, sampler0), uv);
    } break;
    case OWL_SOURCE_DYE: {
      if (0 == ubo.dye_index)
        source = texture(sampler2D(dye0, sampler0), uv);
      else 
        source = texture(sampler2D(dye1, sampler0), uv);
    } break;
    }

    result = source / decay;
  } break;
  case OWL_WORKFLOW_DIVERGENCE: {
    float left;
    float right;
    float top;
    float bottom;
    vec2 c;

    if (0 == ubo.velocity_index) {
      left = texture(sampler2D(velocity0, sampler0), v_left).x;
      right = texture(sampler2D(velocity0, sampler0), v_right).x;
      top = texture(sampler2D(velocity0, sampler0), v_top).y;
      bottom = texture(sampler2D(velocity0, sampler0), v_bottom).y;
      c = texture(sampler2D(velocity0, sampler0), uv).xy;
    } else {
      left = texture(sampler2D(velocity1, sampler0), v_left).x;
      right = texture(sampler2D(velocity1, sampler0), v_right).x;
      top = texture(sampler2D(velocity1, sampler0), v_top).y;
      bottom = texture(sampler2D(velocity1, sampler0), v_bottom).y;
      c = texture(sampler2D(velocity1, sampler0), uv).xy;
    }

    if (v_left.x < 0.0) { left = -c.x; }
    if (v_right.x > 1.0) { right = -c.x; }
    if (v_top.y > 1.0) { top = -c.y; }
    if (v_bottom.y < 0.0) { bottom = -c.y; }

    float div = 0.5 * (right - left + top - bottom);
    result = vec4(div, 0.0, 0.0, 1.0);
  } break;
  case OWL_WORKFLOW_CURL: {
    float left;
    float right;
    float top;
    float bottom;

    if (0 == ubo.velocity_index) {
      left = texture(sampler2D(velocity0, sampler0), v_left).y;
      right = texture(sampler2D(velocity0, sampler0), v_right).y;
      top = texture(sampler2D(velocity0, sampler0), v_top).x;
      bottom = texture(sampler2D(velocity0, sampler0), v_bottom).x;
    } else {
      left = texture(sampler2D(velocity1, sampler0), v_left).y;
      right = texture(sampler2D(velocity1, sampler0), v_right).y;
      top = texture(sampler2D(velocity1, sampler0), v_top).x;
      bottom = texture(sampler2D(velocity1, sampler0), v_bottom).x;
    }

    float vorticity = right - left - top + bottom;
    result = vec4(0.5 * vorticity, 0.0, 0.0, 1.0);
  } break;
  case OWL_WORKFLOW_VORTICITY: {
    float left = texture(sampler2D(curl, sampler0), v_left).x;
    float right = texture(sampler2D(curl, sampler0), v_right).x;
    float top = texture(sampler2D(curl, sampler0), v_top).x;
    float bottom = texture(sampler2D(curl, sampler0), v_bottom).x;
    float c = texture(sampler2D(curl, sampler0), uv).x;
    vec2 force = 0.5 * vec2(abs(top) - abs(bottom), abs(right) - abs(left));
    force /= length(force) + 0.0001;
    force *= ubo.curl * c;
    force.y *= -1.0;

    vec2 velocity;
    if (0 == ubo.velocity_index) {
      velocity = texture(sampler2D(velocity0, sampler0), uv).xy;
    } else {
      velocity = texture(sampler2D(velocity1, sampler0), uv).xy;
    }
    velocity += force * ubo.dt;
    velocity = min(max(velocity, -1000.0), 1000.0);
    result = vec4(velocity, 0.0, 1.0);
  } break;
  case OWL_WORKFLOW_PRESSURE: {
    float left;
    float right;
    float top;
    float bottom;
    float c;
    if (0 == ubo.pressure_index) {
      left = texture(sampler2D(pressure0, sampler0), v_left).x;
      right = texture(sampler2D(pressure0, sampler0),v_right).x;
      top = texture(sampler2D(pressure0, sampler0), v_top).x;
      bottom = texture(sampler2D(pressure0, sampler0), v_bottom).x;
      c = texture(sampler2D(pressure0, sampler0), uv).x;
    } else {
      left = texture(sampler2D(pressure1, sampler0), v_left).x;
      right = texture(sampler2D(pressure1, sampler0), v_right).x;
      top = texture(sampler2D(pressure1, sampler0),v_top).x;
      bottom = texture(sampler2D(pressure1, sampler0),v_bottom).x;
      c = texture(sampler2D(pressure1, sampler0), uv).x;
    }
    float d = texture(sampler2D(divergence, sampler0), uv).x;
    float p = (left + right + bottom + top - d) * 0.25;
    result = vec4(p, 0.0, 0.0, 1.0);
  } break;
  case OWL_WORKFLOW_GRADIENT_SUBTRACT: {
    float left;
    float right;
    float top;
    float bottom;
    float c;
    if (0 == ubo.pressure_index) {
      left = texture(sampler2D(pressure0, sampler0), v_left).x;
      right = texture(sampler2D(pressure0, sampler0), v_right).x;
      top = texture(sampler2D(pressure0, sampler0), v_top).x;
      bottom = texture(sampler2D(pressure0, sampler0), v_bottom).x;
    } else {
      left = texture(sampler2D(pressure1, sampler0), v_left).x;
      right = texture(sampler2D(pressure1, sampler0), v_right).x;
      top = texture(sampler2D(pressure1, sampler0), v_top).x;
      bottom = texture(sampler2D(pressure1, sampler0), v_bottom).x;
    }

    vec2 v;
    if (0 == ubo.velocity_index) {
      v = texture(sampler2D(velocity0, sampler0), uv).xy;
    } else {
      v = texture(sampler2D(velocity1, sampler0), uv).xy;
    }

    v.xy -= vec2(right - left, top - bottom);
    result = vec4(v, 0.0, 1.0);
  } break;
  }
}
