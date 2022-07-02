#version 450

layout(set = 1, binding = 0) uniform sampler sampler0;
layout(set = 1, binding = 1) uniform texture2D color_map;
layout(set = 1, binding = 2) uniform texture2D normal_map;
layout(set = 1, binding = 3) uniform texture2D physical_descriptor_map;

layout(set = 0, binding = 0) uniform UBO {
  mat4 projection;
  mat4 view;
  mat4 model;
  vec4 camera_position;
  vec4 light_direction;
  vec4 light_position;
  float exposure;
  float gamma;
  float prefiltered_cube_mips;
  float scale_ibl_ambient;
  float debug_view_inputs;
  float debug_view_equation;
}
ubo;

layout (push_constant) uniform MATERIAL {
  vec4 base_color_factor;
  vec4 emissive_factor;
  vec4 diffuse_factor;
  vec4 specular_factor;
  float workflow;
  int base_color_uv_set;
  int physical_descriptor_uv_set;
  int normal_uv_set;
  int occlusion_uv_set;
  int emissive_uv_set;
  float metallic_factor;
  float roughness_factor;
  float alpha_mask;
  float alpha_mask_cutoff;
}
material;

layout(location = 0) in vec3 in_world_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_uv0;
layout(location = 3) in vec2 in_uv1;
layout(location = 4) in vec3 in_view;
layout(location = 5) in vec3 in_light;

layout(location = 0) out vec4 out_frag_color;

struct pbr_info {
  float normal_dot_light; // cos angle between normal and light direction
  float normal_dot_view;  // cos angle between normal and view direction
  float normal_dot_half;  // cos angle between normal and half vector
  float light_dot_half;   // cos angle between light direction and half vector
  float view_dot_half;    // cos angle between view direction and half vector
  float perceptual_roughness; // roughness value, as authored by the model
                              // creator (input to shader)
  float metalness;            // metallic value at the surface
  vec3 reflectance0;     // full reflectance color (normal incidence angle)
  vec3 reflectance90;    // reflectance color at grazing angle
  float alpha_roughness; // roughness mapped to a more linear change in the
                         // roughness (proposed by [2])
  vec3 diffuse_color;    // color contribution from diffuse lighting
  vec3 specular_color;   // color contribution from specular lighting
};

const float PI = 3.141592653589793;
const float MINIMUM_ROUGHNESS = 0.04;
const int PBR_WORKFLOW_METALLIC_ROUGHNESS = 0;
const int PBR_WORKFLOW_SPECULAR_GLOSINESS = 0;

#define MANUAL_SRGB 1

vec3
Uncharted2Tonemap(vec3 color) {
  float A = 0.15;
  float B = 0.50;
  float C = 0.10;
  float D = 0.20;
  float E = 0.02;
  float F = 0.30;
  float W = 11.2;
  return ((color * (A * color + C * B) + D * E) /
          (color * (A * color + B) + D * F)) -
         E / F;
}

vec4
SRGBtoLINEAR(vec4 srgbIn) {
#ifdef MANUAL_SRGB
#ifdef SRGB_FAST_APPROXIMATION
  vec3 linOut = pow(srgbIn.xyz, vec3(2.2));
#else  // SRGB_FAST_APPROXIMATION
  vec3 bLess = step(vec3(0.04045), srgbIn.xyz);
  vec3 linOut = mix(srgbIn.xyz / vec3(12.92),
                    pow((srgbIn.xyz + vec3(0.055)) / vec3(1.055), vec3(2.4)),
                    bLess);
#endif // SRGB_FAST_APPROXIMATION
  return vec4(linOut, srgbIn.w);
  ;
#else  // MANUAL_SRGB
  return srgbIn;
#endif // MANUAL_SRGB
}

vec3
getNormal() {
  // Perturb normal, see http://www.thetenthplanet.de/archives/1180
  vec3 tangent_normal;

  if (0 == material.normal_uv_set)
    tangent_normal = texture(sampler2D(normal_map, sampler0), in_uv0).xyz *
                         2.0 -
                     1;
  else
    tangent_normal = texture(sampler2D(normal_map, sampler0), in_uv1).xyz *
                         2.0 -
                     1;

  vec3 q1 = dFdx(in_world_position);
  vec3 q2 = dFdy(in_world_position);
  vec2 st1 = dFdx(in_uv0);
  vec2 st2 = dFdy(in_uv0);

  vec3 N = normalize(in_normal);
  vec3 T = normalize(q1 * st2.t - q2 * st1.t);
  vec3 B = -normalize(cross(N, T));
  mat3 TBN = mat3(T, B, N);

  return normalize(TBN * tangent_normal);
}

#if 1
vec3
getIBLContribution(pbr_info pbr_inputs, vec3 n, vec3 reflection) {
  float lod = (pbr_inputs.perceptual_roughness * ubo.prefiltered_cube_mips);
  // retrieve a scale and bias to F0. See [1], Figure 3
#if 0
	vec3 brdf = (texture(samplerBRDFLUT, vec2(pbrInputs.NdotV, 1.0 - pbrInputs.perceptualRoughness))).rgb;
#else
  vec3 brdf = vec3(1.0);
#endif

#if 0
	vec3 diffuseLight = SRGBtoLINEAR(tonemap(texture(samplerIrradiance, n))).rgb;
#else
  vec3 diffuse_light = vec3(1.0);
#endif

#if 0
	vec3 specularLight = SRGBtoLINEAR(tonemap(textureLod(prefilteredMap, reflection, lod))).rgb;
#else
  vec3 specular_light = vec3(1.0);
#endif

  vec3 diffuse = diffuse_light * pbr_inputs.diffuse_color;
  vec3 specular = specular_light *
                  (pbr_inputs.specular_color * brdf.x + brdf.y);

  // For presentation, this allows us to disable IBL terms
  // For presentation, this allows us to disable IBL terms
  diffuse *= ubo.scale_ibl_ambient;
  specular *= ubo.scale_ibl_ambient;

  return diffuse + specular;
}
#endif

vec4
tonemap(vec4 color) {
  vec3 outcol = Uncharted2Tonemap(color.rgb * ubo.exposure);
  outcol = outcol * (1.0f / Uncharted2Tonemap(vec3(11.2f)));
  return vec4(pow(outcol, vec3(1.0f / ubo.gamma)), color.a);
}

const float M_PI = 3.141592653589793;

vec3
diffuse(pbr_info pbr_inputs) {
  return pbr_inputs.diffuse_color / M_PI;
}

vec3
specularReflection(pbr_info pbr_inputs) {
  return pbr_inputs.reflectance0 +
         (pbr_inputs.reflectance90 - pbr_inputs.reflectance0) *
             pow(clamp(1.0 - pbr_inputs.view_dot_half, 0.0, 1.0), 5.0);
}

float
geometricOcclusion(pbr_info pbr_inputs) {
  float normal_dot_light = pbr_inputs.normal_dot_light;
  float normal_dot_view = pbr_inputs.normal_dot_view;
  float r = pbr_inputs.alpha_roughness;

  float attenuation_light = 2.0 * normal_dot_light /
                            (normal_dot_light +
                             sqrt(r * r + (1.0 - r * r) * (normal_dot_light *
                                                           normal_dot_light)));
  float attenuation_view = 2.0 * normal_dot_view /
                           (normal_dot_view +
                            sqrt(r * r + (1.0 - r * r) * (normal_dot_view *
                                                          normal_dot_view)));
  return attenuation_light * attenuation_view;
}

float
microfacetDistribution(pbr_info pbr_inputs) {
  float roughness_squared = pbr_inputs.alpha_roughness *
                            pbr_inputs.alpha_roughness;
  float f = (pbr_inputs.normal_dot_half * roughness_squared -
             pbr_inputs.normal_dot_half) *
                pbr_inputs.normal_dot_half +
            1.0;
  return roughness_squared / (M_PI * f * f);
}

float
convertMetallic(vec3 diffuse, vec3 specular, float maxSpecular) {
  float perceivedDiffuse = sqrt(0.299 * diffuse.r * diffuse.r +
                                0.587 * diffuse.g * diffuse.g +
                                0.114 * diffuse.b * diffuse.b);
  float perceivedSpecular = sqrt(0.299 * specular.r * specular.r +
                                 0.587 * specular.g * specular.g +
                                 0.114 * specular.b * specular.b);
  if (perceivedSpecular < MINIMUM_ROUGHNESS) {
    return 0.0;
  }
  float a = MINIMUM_ROUGHNESS;
  float b = perceivedDiffuse * (1.0 - maxSpecular) /
                (1.0 - MINIMUM_ROUGHNESS) +
            perceivedSpecular - 2.0 * MINIMUM_ROUGHNESS;
  float c = MINIMUM_ROUGHNESS - perceivedSpecular;
  float D = max(b * b - 4.0 * a * c, 0.0);
  return clamp((-b + sqrt(D)) / (2.0 * a), 0.0, 1.0);
}

void
main() {
  float perceptual_roughness;
  float metallic;
  vec3 diffuse_color;
  vec4 base_color;

  vec3 f0 = vec3(0.04);

  if (material.alpha_mask == 1.0F) {
    if (0 == material.base_color_uv_set) {
      base_color = SRGBtoLINEAR(
                       texture(sampler2D(color_map, sampler0), in_uv0)) *
                   material.base_color_factor;
    } else if (1 == material.base_color_uv_set) {
      base_color = SRGBtoLINEAR(
                       texture(sampler2D(color_map, sampler0), in_uv1)) *
                   material.base_color_factor;
    }

    if (base_color.a < material.alpha_mask_cutoff)
      discard;
  }

  if (PBR_WORKFLOW_METALLIC_ROUGHNESS == material.workflow) {
    perceptual_roughness = material.roughness_factor;
    metallic = material.metallic_factor;

    if (0 == material.physical_descriptor_uv_set) {
      vec4 sample_metallic_roughness = texture(
          sampler2D(physical_descriptor_map, sampler0), in_uv0);
      perceptual_roughness = sample_metallic_roughness.g *
                             perceptual_roughness;
      metallic = sample_metallic_roughness.b * metallic;
    } else if (1 == material.physical_descriptor_uv_set) {
      vec4 sample_metallic_roughness = texture(
          sampler2D(physical_descriptor_map, sampler0), in_uv1);
      perceptual_roughness = sample_metallic_roughness.g *
                             perceptual_roughness;
      metallic = sample_metallic_roughness.b * metallic;
    } else {
      perceptual_roughness = clamp(perceptual_roughness, MINIMUM_ROUGHNESS,
                                   1.0);
      metallic = clamp(metallic, 0.0, 1.0);
    }

    if (0 == material.base_color_uv_set) {
      base_color = SRGBtoLINEAR(
                       texture(sampler2D(color_map, sampler0), in_uv0)) *
                   material.base_color_factor;
    } else {
      base_color = material.base_color_factor;
    }
  }

  if (PBR_WORKFLOW_SPECULAR_GLOSINESS == material.workflow) {
    if (0 == material.physical_descriptor_uv_set) {
      perceptual_roughness =
          1.0 -
          texture(sampler2D(physical_descriptor_map, sampler0), in_uv0).a;
    } else if (1 == material.physical_descriptor_uv_set) {
      perceptual_roughness =
          1.0 -
          texture(sampler2D(physical_descriptor_map, sampler0), in_uv1).a;
    } else {
      perceptual_roughness = 0.0F;
    }

    const float epsilon = 1e-6;

    vec4 diffuse = SRGBtoLINEAR(
        texture(sampler2D(color_map, sampler0), in_uv0));
    vec3 specular = SRGBtoLINEAR(
                        texture(sampler2D(physical_descriptor_map, sampler0),
                                in_uv0))
                        .rgb;

    const float max_specular = max(max(specular.r, specular.g), specular.b);

    metallic = convertMetallic(diffuse.rgb, specular, max_specular);

    vec3 base_color_diffuse_part = diffuse.rgb *
                                   ((1.0 - max_specular) /
                                    (1 - MINIMUM_ROUGHNESS) /
                                    max(1 - metallic, epsilon)) *
                                   material.diffuse_factor.rgb;
    vec3 base_color_specular_part = specular -
                                    (vec3(MINIMUM_ROUGHNESS) * (1 - metallic) *
                                     (1 / max(metallic, epsilon))) *
                                        material.specular_factor.rgb;
    base_color = vec4(mix(base_color_diffuse_part, base_color_specular_part,
                          metallic * metallic),
                      diffuse.a);
  }

  diffuse_color = base_color.rgb * (vec3(1.0) - f0);
  diffuse_color *= 1.0 - metallic;

  float alpha_roughness = perceptual_roughness * perceptual_roughness;
  vec3 specular_color = mix(f0, base_color.rgb, metallic);

  float reflectance = max(max(specular_color.r, specular_color.g),
                          specular_color.b);
  float reflectance90 = clamp(reflectance * 25.0, 0.0, 1.0);
  vec3 specular_environment_r0 = specular_color.rgb;
  vec3 specular_environment_r90 = vec3(1.0, 1.0, 1.0) * reflectance90;

  vec3 n = (material.normal_uv_set > -1) ? getNormal() : normalize(in_normal);
  vec3 v = normalize(ubo.camera_position.xyz - in_world_position);
  vec3 l = normalize(ubo.light_direction.xyz);
  vec3 h = normalize(l + v);
  vec3 reflection = -normalize(reflect(v, n));
  reflection.y *= -1.0;

  float normal_dot_light = clamp(dot(n, l), 0.001, 1.0);
  float normal_dot_view = clamp(abs(dot(n, v)), 0.001, 1.0);
  float normal_dot_half = clamp(dot(n, h), 0.0, 1.0);
  float light_dot_half = clamp(dot(l, h), 0.0, 1.0);
  float view_dot_half = clamp(dot(v, h), 0.0, 1.0);

  pbr_info inputs = pbr_info(normal_dot_light, normal_dot_view,
                             normal_dot_half, light_dot_half, view_dot_half,
                             perceptual_roughness, metallic,
                             specular_environment_r0, specular_environment_r90,
                             alpha_roughness, diffuse_color, specular_color);

  vec4 color = texture(sampler2D(color_map, sampler0), in_uv0);

  vec3 normalized_normal = normalize(in_normal);
  vec3 normalized_light = normalize(in_light);
  vec3 normalized_view = normalize(in_view);
  vec3 diffuse = max(dot(normalized_normal, normalized_light), 0.5) *
                 vec3(1.0F, 1.0F, 1.0F);
  vec3 specular = pow(max(dot(reflection, normalized_view), 0.0), 16.0) *
                  vec3(0.75);
  out_frag_color = vec4(diffuse * color.rgb + specular, 1.0);
}
