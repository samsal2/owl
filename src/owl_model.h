#ifndef OWL_MODEL_H
#define OWL_MODEL_H

#include "owl_definitions.h"
#include "owl_renderer.h"
#include "owl_texture.h"

#include <vulkan/vulkan.h>

OWL_BEGIN_DECLARATIONS

/* TODO(samuel): rewrite this ret, most of it is garbage */

#define OWL_ALPHA_MODE_OPAQUE 0
#define OWL_ALPHA_MODE_MASK 1
#define OWL_ALPHA_MODE_BLEND 2

struct owl_model_vertex {
	owl_v3 position;
	owl_v3 normal;
	owl_v2 uv0;
	owl_v2 uv1;
	owl_v4 joints0;
	owl_v4 weights0;
	owl_v4 color0;
};

struct owl_model_uniform {
	owl_m4 projection;
	owl_m4 model;
	owl_m4 view;
	owl_v4 light_direction;
	owl_v3 camera_position;
	float exposure;
	float gamma;
	float prefiltered_cube_mip_levels;
	float scale_ibl_ambient;
	float debug_view_inputs;
	float debug_view_equation;
};

struct owl_model_bbox {
	owl_v3 min;
	owl_v3 max;
	int32_t valid;
};

struct owl_model_material {
	int32_t alpha_mode;
	float alpha_cutoff;
	float metallic_factor;
	float roughness_factor;
	owl_v4 base_color_factor;
	owl_v4 emissive_factor;
	int32_t base_color_texture;
	int32_t metallic_roughness_texture;
	int32_t normal_texture;
	int32_t occlusion_texture;
	int32_t emissive_texture;
	int32_t double_sided;
	int32_t base_color_texcoord;
	int32_t metallic_roughness_texcoord;
	int32_t normal_texcoord;
	int32_t occlusion_texcoord;
	int32_t emissive_texcoord;
	int32_t specular_glossiness_texture;
	int32_t specular_glossiness_texcoord;
	int32_t diffuse_texture;
	owl_v4 diffuse_factor;
	owl_v4 specular_factor;
	int32_t metallic_roughness_enable;
	int32_t specular_glossiness_enable;
	VkDescriptorSet descriptor_set;
};

struct owl_model_primitive {
	uint32_t first;
	uint32_t num_indices;
	uint32_t num_vertices;
	int32_t material;
	int32_t has_indices;
	struct owl_model_bbox bbox;
};

struct owl_model_joints_ssbo {
	owl_m4 matrix;
	owl_m4 joints[128];
	int32_t num_joints;
};

struct owl_model_mesh {
	int32_t num_primitives;
	int32_t primitives[128];
	struct owl_model_bbox bb;
	struct owl_model_bbox aabb;

	VkBuffer ssbos[OWL_NUM_IN_FLIGHT_FRAMES];
	VkDeviceMemory ssbo_memory;
	VkDescriptorSet ssbo_descriptor_sets[OWL_NUM_IN_FLIGHT_FRAMES];
	struct owl_model_joints_ssbo *mapped_ssbos[OWL_NUM_IN_FLIGHT_FRAMES];
};

struct owl_model_skin {
	char name[256];
	int32_t root;

	int32_t num_inverse_bind_matrices;
	owl_m4 inverse_bind_matrices[128];

	int32_t num_joints;
	int32_t joints[128];
};

struct owl_model_node {
	char name[256];
	int32_t parent;
	int32_t num_children;
	int32_t children[128];
	int32_t mesh;
	int32_t skin;
	owl_m4 matrix;
	owl_v3 translation;
	owl_v3 scale;
	owl_v4 rotation;
	struct owl_model_bbox bvh;
	struct owl_model_bbox aabb;
};

#define OWL_ANIMATION_PATH_TRANSLATION 1
#define OWL_ANIMATION_PATH_ROTATION 2
#define OWL_ANIMATION_PATH_SCALE 3
#define OWL_ANIMATION_PATH_WEIGHTS 4

struct owl_model_animation_channel {
	int32_t path;
	int32_t node;
	int32_t sampler;
};

#define OWL_ANIMATION_INTERPOLATION_LINEAR 0
#define OWL_ANIMATION_INTERPOLATION_STEP 1
#define OWL_ANIMATION_INTERPOLATION_CUBICSPLINE 2

struct owl_model_animation_sampler {
	int32_t interpolation;

	int32_t num_inputs;
	float inputs[128];

	int32_t num_outputs;
	owl_v4 outputs[128];
};

struct owl_model_animation {
	char name[256];

	int32_t num_samplers;
	int32_t samplers[128];

	int32_t num_channels;
	int32_t channels[128];

	float time;
	float start;
	float end;
};

struct owl_model_push_constant {
	owl_v4 base_color_factor;
	owl_v4 emissive_factor;
	owl_v4 diffuse_factor;
	owl_v4 specular_factor;
	float workflow;
	int base_color_uv_set;
	int physical_desc_uv_set;
	int normal_uv_set;
	int occlusion_uv_set;
	int emissive_uv_set;
	float metallic_factor;
	float roughness_factor;
	float alpha_mask;
	float alpha_mask_cutoff;
};

struct owl_model_image {
	struct owl_texture texture;
};

struct owl_model_texture {
	int32_t image;
};

struct owl_model {
	char path[256];
	char directory[256];

	VkBuffer vertex_buffer;
	VkDeviceMemory vertex_memory;

	int32_t has_indices;
	VkBuffer index_buffer;
	VkDeviceMemory index_memory;

	int32_t active_animation;

	int32_t num_roots;
	int32_t roots[128];

	struct owl_texture empty_texture;

	int num_nodes;
	struct owl_model_node nodes[128];

	int num_images;
	struct owl_model_image images[128];

	int num_textures;
	struct owl_model_texture textures[128];

	int num_materials;
	struct owl_model_material materials[128];

	int num_meshes;
	struct owl_model_mesh meshes[128];

	int num_primitives;
	struct owl_model_primitive primitives[256];

	int num_skins;
	struct owl_model_skin skins[128];

	int num_samplers;
	struct owl_model_animation_sampler samplers[128];

	int num_channels;
	struct owl_model_animation_channel channels[128];

	int num_animations;
	struct owl_model_animation animations[128];
};

OWLAPI int owl_model_init(struct owl_model *model, struct owl_renderer *r,
			  char const *path);

OWLAPI void owl_model_deinit(struct owl_model *model, struct owl_renderer *r);

OWLAPI int owl_model_update_animation(struct owl_renderer *r,
				      struct owl_model *m, float dt,
				      int32_t animation);

OWL_END_DECLARATIONS

#endif
