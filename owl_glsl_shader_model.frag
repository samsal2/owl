#version 450

layout (set = 1, binding = 0) uniform sampler sampler0;
layout (set = 1, binding = 1) uniform texture2D color_map;
layout (set = 2, binding = 0) uniform sampler sampler1;
layout (set = 2, binding = 1) uniform texture2D normal_map;

layout (set = 0, binding = 0) uniform UBO {
	mat4 projection;
	mat4 view;
  mat4 model;
	vec4 light_position;
} ubo;

layout (set = 4, binding = 0) uniform UBO_PARAMS {
  vec4 light_direction;
	float exposure;
	float gamma;
	float prefiltered_cube_mips;
	float scale_ibl_ambient;
	float debug_view_inputs;
	float dedbug_view_equation;
} ubo_params;

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
} material;

layout (location = 0) in vec3 in_world_pos;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec2 in_uv0;
layout (location = 3) in vec2 in_uv1;
layout (location = 4) in vec3 in_view;
layout (location = 5) in vec3 in_light;

layout (location = 0) out vec4 out_frag_color;

struct pbr_info {
	float normal_dot_light;     // cos angle between normal and light direction
	float normal_dot_view;      // cos angle between normal and view direction
	float normal_dot_half;      // cos angle between normal and half vector
	float light_dot_half;       // cos angle between light direction and half vector
	float view_dot_half;        // cos angle between view direction and half vector
	float perceptual_roughness; // roughness value, as authored by the model creator (input to shader)
	float metalness;            // metallic value at the surface
	vec3 reflectance0;          // full reflectance color (normal incidence angle)
	vec3 reflectance90;         // reflectance color at grazing angle
	float alpha_roughness;      // roughness mapped to a more linear change in the roughness (proposed by [2])
	vec3 diffuse_color;         // color contribution from diffuse lighting
	vec3 specular_color;        // color contribution from specular lighting
};

const float PI = 3.141592653589793;
const float MINIMUM_ROUGHNESS = 0.04;
const int PBR_WORKFLOW_METALLIC_ROUGHNESS = 0;
const int PBR_WORKFLOW_SPECULAR_GLOSINESS = 0;

#define MANUAL_SRGB 1

vec3 Uncharted2Tonemap(vec3 color)
{
	float A = 0.15;
	float B = 0.50;
	float C = 0.10;
	float D = 0.20;
	float E = 0.02;
	float F = 0.30;
	float W = 11.2;
	return ((color*(A*color+C*B)+D*E)/(color*(A*color+B)+D*F))-E/F;
}

vec4 SRGBtoLINEAR(vec4 srgbIn)
{
	#ifdef MANUAL_SRGB
	#ifdef SRGB_FAST_APPROXIMATION
	vec3 linOut = pow(srgbIn.xyz,vec3(2.2));
	#else //SRGB_FAST_APPROXIMATION
	vec3 bLess = step(vec3(0.04045),srgbIn.xyz);
	vec3 linOut = mix( srgbIn.xyz/vec3(12.92), pow((srgbIn.xyz+vec3(0.055))/vec3(1.055),vec3(2.4)), bLess );
	#endif //SRGB_FAST_APPROXIMATION
	return vec4(linOut,srgbIn.w);;
	#else //MANUAL_SRGB
	return srgbIn;
	#endif //MANUAL_SRGB
}

vec3 getNormal()
{
	// Perturb normal, see http://www.thetenthplanet.de/archives/1180
  vec3 tangent_normal;

  if (0 == material.normal_uv_set)
    tangent_normal = texture(sampler2D(normal_map, sampler1), in_uv0).xyz * 2.0 - 1;
  else
    tangent_normal = texture(sampler2D(normal_map, sampler1), in_uv1).xyz * 2.0 - 1;

	vec3 q1 = dFdx(in_world_pos);
	vec3 q2 = dFdy(in_world_pos);
	vec2 st1 = dFdx(in_uv0);
	vec2 st2 = dFdy(in_uv0);

	vec3 N = normalize(in_normal);
	vec3 T = normalize(q1 * st2.t - q2 * st1.t);
	vec3 B = -normalize(cross(N, T));
	mat3 TBN = mat3(T, B, N);

	return normalize(TBN * tangent_normal);
}

#if 1
vec3 getIBLContribution(pbr_info pbr_inputs, vec3 n, vec3 reflection) {
	float lod = (pbr_inputs.perceptual_roughness * ubo_params.prefiltered_cube_mips);
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
	vec3 specular = specular_light * (pbr_inputs.specular_color * brdf.x + brdf.y);

	// For presentation, this allows us to disable IBL terms
	// For presentation, this allows us to disable IBL terms
	diffuse *= ubo_params.scale_ibl_ambient;
	specular *= ubo_params.scale_ibl_ambient;

	return diffuse + specular;
}
#endif

vec4 tonemap(vec4 color)
{
	vec3 outcol = Uncharted2Tonemap(color.rgb * ubo_params.exposure);
	outcol = outcol * (1.0f / Uncharted2Tonemap(vec3(11.2f)));
	return vec4(pow(outcol, vec3(1.0f / ubo_params.gamma)), color.a);
}


void main()
{
	vec4 color = texture(sampler2D(color_map, sampler0), in_uv0);

	vec3 normalized_normal = normalize(in_normal);
	vec3 normalized_light = normalize(in_light);
	vec3 normalized_view = normalize(in_view);
	vec3 reflection = reflect(-normalized_light, normalized_normal);
	vec3 diffuse = max(dot(normalized_normal, normalized_light), 0.5) * vec3(1.0F, 1.0F, 1.0F);
	vec3 specular = pow(max(dot(reflection, normalized_view), 0.0), 16.0) * vec3(0.75);
	out_frag_color = vec4(diffuse * color.rgb + specular, 1.0);
}
