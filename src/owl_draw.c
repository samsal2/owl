#include "owl_draw.h"

#include "owl_cloth_simulation.h"
#include "owl_fluid_simulation.h"
#include "owl_internal.h"
#include "owl_model.h"
#include "owl_plataform.h"
#include "owl_renderer.h"
#include "owl_vector_math.h"

#include <stdio.h>

/* TODO(samuel): this can be optimized to a single uniforma allcation, even
 * just a push_constant */
OWLAPI int owl_draw_quad(struct owl_renderer *r, struct owl_quad const *quad)
{
	uint8_t *data;
	VkDescriptorSet descriptor_sets[2];
	VkCommandBuffer command_buffer;
	struct owl_common_uniform uniform;
	struct owl_common_vertex vertices[4];
	struct owl_renderer_vertex_allocation vertex_allocation;
	struct owl_renderer_index_allocation index_allocation;
	struct owl_renderer_uniform_allocation uniform_allocation;
	static uint32_t const indices[] = { 2, 3, 1, 1, 0, 2 };

	command_buffer = r->submit_command_buffers[r->frame];

	vertices[0].position[0] = quad->position0[0];
	vertices[0].position[1] = quad->position0[1];
	vertices[0].position[2] = 0.0F;
	vertices[0].color[0] = quad->color[0];
	vertices[0].color[1] = quad->color[1];
	vertices[0].color[2] = quad->color[2];
	vertices[0].uv[0] = quad->uv0[0];
	vertices[0].uv[1] = quad->uv0[1];

	vertices[1].position[0] = quad->position1[0];
	vertices[1].position[1] = quad->position0[1];
	vertices[1].position[2] = 0.0F;
	vertices[1].color[0] = quad->color[0];
	vertices[1].color[1] = quad->color[1];
	vertices[1].color[2] = quad->color[2];
	vertices[1].uv[0] = quad->uv1[0];
	vertices[1].uv[1] = quad->uv0[1];

	vertices[2].position[0] = quad->position0[0];
	vertices[2].position[1] = quad->position1[1];
	vertices[2].position[2] = 0.0F;
	vertices[2].color[0] = quad->color[0];
	vertices[2].color[1] = quad->color[1];
	vertices[2].color[2] = quad->color[2];
	vertices[2].uv[0] = quad->uv0[0];
	vertices[2].uv[1] = quad->uv1[1];

	vertices[3].position[0] = quad->position1[0];
	vertices[3].position[1] = quad->position1[1];
	vertices[3].position[2] = 0.0F;
	vertices[3].color[0] = quad->color[0];
	vertices[3].color[1] = quad->color[1];
	vertices[3].color[2] = quad->color[2];
	vertices[3].uv[0] = quad->uv1[0];
	vertices[3].uv[1] = quad->uv1[1];

	OWL_M4_IDENTITY(uniform.projection);
	OWL_M4_IDENTITY(uniform.view);
	OWL_M4_IDENTITY(uniform.model);

	data = owl_renderer_vertex_allocate(r, sizeof(vertices),
					    &vertex_allocation);
	if (!data)
		return OWL_ERROR_NO_FRAME_MEMORY;
	OWL_MEMCPY(data, vertices, sizeof(vertices));

	data = owl_renderer_index_allocate(r, sizeof(indices),
					   &index_allocation);
	if (!data)
		return OWL_ERROR_NO_FRAME_MEMORY;
	OWL_MEMCPY(data, indices, sizeof(indices));

	data = owl_renderer_uniform_allocate(r, sizeof(uniform),
					     &uniform_allocation);
	if (!data)
		return OWL_ERROR_NO_FRAME_MEMORY;
	OWL_MEMCPY(data, &uniform, sizeof(uniform));

	descriptor_sets[0] = uniform_allocation.common_descriptor_set;
	descriptor_sets[1] = quad->texture->descriptor_set;

	vkCmdBindVertexBuffers(command_buffer, 0, 1, &vertex_allocation.buffer,
			       &vertex_allocation.offset);

	vkCmdBindIndexBuffer(command_buffer, index_allocation.buffer,
			     index_allocation.offset, VK_INDEX_TYPE_UINT32);

	vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
				r->common_pipeline_layout, 0,
				OWL_ARRAY_SIZE(descriptor_sets),
				descriptor_sets, 1, &uniform_allocation.offset);

	vkCmdDrawIndexed(command_buffer, OWL_ARRAY_SIZE(indices), 1, 0, 0, 0);

	return OWL_OK;
}

static int owl_draw_glyph(struct owl_renderer *r, struct owl_glyph *glyph,
			  owl_v3 const color)
{
	struct owl_quad quad;

	quad.texture = &r->font.atlas;

	quad.position0[0] = glyph->positions[0][0];
	quad.position0[1] = glyph->positions[0][1];

	quad.position1[0] = glyph->positions[3][0];
	quad.position1[1] = glyph->positions[3][1];

	quad.color[0] = color[0];
	quad.color[1] = color[1];
	quad.color[2] = color[2];

	quad.uv0[0] = glyph->uvs[0][0];
	quad.uv0[1] = glyph->uvs[0][1];

	quad.uv1[0] = glyph->uvs[3][0];
	quad.uv1[1] = glyph->uvs[3][1];

	return owl_draw_quad(r, &quad);
}

OWLAPI int owl_draw_text(struct owl_renderer *r, char const *text,
			 owl_v3 const position, owl_v3 const color)
{
	char const *letter;
	owl_v2 offset;
	int ret;
	VkCommandBuffer command_buffer;

	command_buffer = r->submit_command_buffers[r->frame];

	vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			  r->text_pipeline);

	offset[0] = position[0] * r->width;
	offset[1] = position[1] * r->height;

	for (letter = &text[0]; '\0' != *letter; ++letter) {
		struct owl_glyph glyph;

		ret = owl_font_fill_glyph(&r->font, *letter, offset, r->width,
					  r->height, &glyph);
		if (ret)
			return ret;

		ret = owl_draw_glyph(r, &glyph, color);
		if (ret)
			return ret;
	}

	return OWL_OK;
}

static int owl_draw_model_node(struct owl_renderer *r, int32_t id,
			       struct owl_model const *m, owl_m4 matrix)
{
	int32_t i;
	int ret;
	uint8_t *data;
	int32_t p;
	struct owl_model_node const *node;
	struct owl_model_mesh const *mesh;
	struct owl_model_joints_ssbo *ssbo;
	struct owl_model_uniform uniform;
	struct owl_renderer_uniform_allocation uniform_allocation;
	VkCommandBuffer command_buffer = r->submit_command_buffers[r->frame];

	node = &m->nodes[id];

	for (i = 0; i < node->num_children; ++i) {
		ret = owl_draw_model_node(r, node->children[i], m, matrix);
		if (ret)
			return ret;
	}

	if (-1 == node->mesh)
		return OWL_OK;

	mesh = &m->meshes[node->mesh];
	ssbo = mesh->mapped_ssbos[r->frame];

	OWL_M4_COPY(node->matrix, ssbo->matrix);

	for (p = node->parent; - 1 != p; p = m->nodes[p].parent)
		owl_m4_multiply(m->nodes[p].matrix, ssbo->matrix, ssbo->matrix);

	OWL_M4_COPY(r->projection, uniform.projection);
	OWL_M4_COPY(matrix, uniform.model);
	OWL_M4_COPY(r->view, uniform.view);

	uniform.light_direction[0] = -1.0F;
	uniform.light_direction[1] = 0.0F;
	uniform.light_direction[2] = 0.0F;
	uniform.light_direction[3] = 0.0F;
	uniform.camera_position[0] = r->camera_eye[0];
	uniform.camera_position[1] = r->camera_eye[1];
	uniform.camera_position[2] = r->camera_eye[2];
	uniform.exposure = 4.5F;
	uniform.gamma = 2.2F;
	uniform.prefiltered_cube_mip_levels = r->prefiltered_map_mipmaps;
	uniform.scale_ibl_ambient = 1.0F;
	uniform.debug_view_inputs = 0.0F;
	uniform.debug_view_equation = 0.0F;

	data = owl_renderer_uniform_allocate(r, sizeof(uniform),
					     &uniform_allocation);
	if (!data)
		return OWL_ERROR_NO_MEMORY;
	OWL_MEMCPY(data, &uniform, sizeof(uniform));

	vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
				r->model_pipeline_layout, 0, 1,
				&uniform_allocation.model_descriptor_set, 1,
				&uniform_allocation.offset);

	for (i = 0; i < mesh->num_primitives; ++i) {
		VkDescriptorSet descriptors_sets[3];
		struct owl_model_primitive const *primitive;
		struct owl_model_material const *material;
		struct owl_model_push_constant push_constant;

		push_constant.workflow = 0.0F;
		push_constant.base_color_uv_set = -1;
		push_constant.physical_desc_uv_set = -1;
		push_constant.normal_uv_set = -1;
		push_constant.occlusion_uv_set = -1;
		push_constant.emissive_uv_set = -1;

		primitive = &m->primitives[mesh->primitives[i]];

		if (!primitive->num_vertices)
			continue;

		material = &m->materials[primitive->material];

		descriptors_sets[0] = mesh->ssbo_descriptor_sets[r->frame];
		descriptors_sets[1] = material->descriptor_set;
		descriptors_sets[2] = r->environment_descriptor_set;

		vkCmdBindDescriptorSets(command_buffer,
					VK_PIPELINE_BIND_POINT_GRAPHICS,
					r->model_pipeline_layout, 1,
					OWL_ARRAY_SIZE(descriptors_sets),
					descriptors_sets, 0, NULL);

		push_constant.emissive_factor[0] = material->emissive_factor[0];
		push_constant.emissive_factor[1] = material->emissive_factor[1];
		push_constant.emissive_factor[2] = material->emissive_factor[2];
		push_constant.emissive_factor[3] = material->emissive_factor[3];

		push_constant.diffuse_factor[0] = material->diffuse_factor[0];
		push_constant.diffuse_factor[1] = material->diffuse_factor[1];
		push_constant.diffuse_factor[2] = material->diffuse_factor[2];
		push_constant.diffuse_factor[3] = material->diffuse_factor[3];

		push_constant.specular_factor[0] = material->specular_factor[0];
		push_constant.specular_factor[1] = material->specular_factor[1];
		push_constant.specular_factor[2] = material->specular_factor[2];
		push_constant.specular_factor[3] = material->specular_factor[3];

		if (-1 == material->base_color_texture)
			push_constant.base_color_uv_set = -1;
		else {
			push_constant.base_color_uv_set =
				material->base_color_texcoord;

			OWL_ASSERT(0 == push_constant.base_color_uv_set);
		}

		if (-1 == material->normal_texcoord)
			push_constant.normal_uv_set = -1;
		else
			push_constant.normal_uv_set = material->normal_texcoord;

		if (-1 == material->occlusion_texture)
			push_constant.occlusion_uv_set = -1;
		else
			push_constant.occlusion_uv_set =
				material->occlusion_texcoord;

		if (-1 == material->emissive_texture)
			push_constant.emissive_uv_set = -1;
		else
			push_constant.emissive_uv_set =
				material->emissive_texcoord;

		push_constant.physical_desc_uv_set = -1;

		push_constant.alpha_mask = material->alpha_mode ==
					   OWL_ALPHA_MODE_MASK;
		push_constant.alpha_mask_cutoff = material->alpha_cutoff;

		if (material->specular_glossiness_enable) {
			push_constant.workflow = 1;

			if (-1 == material->specular_glossiness_texture)
				push_constant.physical_desc_uv_set = -1;
			else
				push_constant.physical_desc_uv_set =
					material->specular_glossiness_texture;

			if (-1 == material->diffuse_texture)
				push_constant.base_color_uv_set = -1;
			else
				push_constant.base_color_uv_set =
					material->base_color_texcoord;
		}

		if (material->metallic_roughness_enable) {
			push_constant.workflow = 0;
			push_constant.base_color_factor[0] =
				material->base_color_factor[0];
			push_constant.base_color_factor[1] =
				material->base_color_factor[1];
			push_constant.base_color_factor[2] =
				material->base_color_factor[2];
			push_constant.base_color_factor[3] =
				material->base_color_factor[3];

			push_constant.metallic_factor =
				material->metallic_factor;
			push_constant.roughness_factor =
				material->roughness_factor;

			if (-1 == material->metallic_roughness_texture)
				push_constant.physical_desc_uv_set = -1;
			else
				push_constant.physical_desc_uv_set =
					material->metallic_roughness_texcoord;
		}

		vkCmdPushConstants(command_buffer, r->model_pipeline_layout,
				   VK_SHADER_STAGE_FRAGMENT_BIT, 0,
				   sizeof(push_constant), &push_constant);

		if (primitive->has_indices)
			vkCmdDrawIndexed(command_buffer, primitive->num_indices,
					 1, primitive->first, 0, 0);
		else
			vkCmdDraw(command_buffer, primitive->num_vertices, 1,
				  primitive->first, 0);
	}

	return OWL_OK;
}

OWLAPI int owl_draw_model(struct owl_renderer *r, struct owl_model const *model,
			  owl_m4 matrix)
{
	int i;

	uint64_t offset = 0;
	int ret = OWL_OK;
	VkCommandBuffer command_buffer;

	command_buffer = r->submit_command_buffers[r->frame];

	vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			  r->model_pipeline);

	vkCmdBindVertexBuffers(command_buffer, 0, 1, &model->vertex_buffer,
			       &offset);

	vkCmdBindIndexBuffer(command_buffer, model->index_buffer, 0,
			     VK_INDEX_TYPE_UINT32);

	for (i = 0; i < model->num_roots; ++i) {
		int32_t root = model->roots[i];
		ret = owl_draw_model_node(r, root, model, matrix);
		if (ret)
			return ret;
	}

	return OWL_OK;
}

OWLAPI int owl_draw_skybox(struct owl_renderer *r)
{
	uint8_t *data;
	struct owl_renderer_vertex_allocation vertex_allocation;
	struct owl_renderer_index_allocation index_allocation;
	struct owl_renderer_uniform_allocation uniform_allocation;
	VkDescriptorSet descriptor_sets[2];
	VkCommandBuffer command_buffer;
	struct owl_common_uniform uniform;
	/*
     *    4----5
     *  / |  / |
     * 0----1  |
     * |  | |  |
     * |  6-|--7
     * | /  | /
     * 2----3
     */
	static struct owl_skybox_vertex const vertices[] = {
		{ -1.0F, -1.0F, -1.0F }, /* 0 */
		{ 1.0F, -1.0F, -1.0F }, /* 1 */
		{ -1.0F, 1.0F, -1.0F }, /* 2 */
		{ 1.0F, 1.0F, -1.0F }, /* 3 */
		{ -1.0F, -1.0F, 1.0F }, /* 4 */
		{ 1.0F, -1.0F, 1.0F }, /* 5 */
		{ -1.0F, 1.0F, 1.0F }, /* 6 */
		{ 1.0F, 1.0F, 1.0F }
	}; /* 7 */
	static uint32_t const indices[] = {
		2, 3, 1, 1, 0, 2, /* face 0 ...............*/
		3, 7, 5, 5, 1, 3, /* face 1 */
		6, 2, 0, 0, 4, 6, /* face 2 */
		7, 6, 4, 4, 5, 7, /* face 3 */
		3, 2, 6, 6, 7, 3, /* face 4 */
		4, 0, 1, 1, 5, 4
	}; /* face 5 */

	command_buffer = r->submit_command_buffers[r->frame];

	vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			  r->skybox_pipeline);

	data = owl_renderer_vertex_allocate(r, sizeof(vertices),
					    &vertex_allocation);
	if (!data)
		return OWL_ERROR_NO_FRAME_MEMORY;
	OWL_MEMCPY(data, vertices, sizeof(vertices));

	data = owl_renderer_index_allocate(r, sizeof(indices),
					   &index_allocation);
	if (!data)
		return OWL_ERROR_NO_FRAME_MEMORY;
	OWL_MEMCPY(data, indices, sizeof(indices));

	OWL_M4_COPY(r->projection, uniform.projection);
	OWL_M4_IDENTITY(uniform.view);
	OWL_M3_COPY(r->view, uniform.view);
	OWL_M4_IDENTITY(uniform.model);

	data = owl_renderer_uniform_allocate(r, sizeof(uniform),
					     &uniform_allocation);
	if (!data)
		return OWL_ERROR_NO_FRAME_MEMORY;
	OWL_MEMCPY(data, &uniform, sizeof(uniform));

	descriptor_sets[0] = uniform_allocation.common_descriptor_set;
	descriptor_sets[1] = r->skybox.descriptor_set;

	vkCmdBindVertexBuffers(command_buffer, 0, 1, &vertex_allocation.buffer,
			       &vertex_allocation.offset);

	vkCmdBindIndexBuffer(command_buffer, index_allocation.buffer,
			     index_allocation.offset, VK_INDEX_TYPE_UINT32);

	vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
				r->common_pipeline_layout, 0,
				OWL_ARRAY_SIZE(descriptor_sets),
				descriptor_sets, 1, &uniform_allocation.offset);

	vkCmdDrawIndexed(command_buffer, OWL_ARRAY_SIZE(indices), 1, 0, 0, 0);

	return OWL_OK;
}

OWLAPI int owl_draw_renderer_state(struct owl_renderer *r)
{
	char buffer[256];
	owl_v3 position = { -0.8F, -0.8F, 0.0F };
	owl_v3 color = { 1.0F, 1.0F, 1.0F };
	static double previous_time = 0.0;
	static double current_time = 0.0;
	float fps = 0.0;

	previous_time = current_time;
	current_time = owl_plataform_get_time(r->plataform);
	fps = 1.0 / (current_time - previous_time);

	snprintf(buffer, sizeof(buffer), "fps: %.2f", fps);

	owl_draw_text(r, buffer, position, color);

	position[1] += 0.05F;

	snprintf(buffer, sizeof(buffer), "vertex_buffer_size: %llu",
		 r->vertex_buffer_size);

	owl_draw_text(r, buffer, position, color);

	position[1] += 0.05F;

	snprintf(buffer, sizeof(buffer), "index_buffer_size: %llu",
		 r->index_buffer_size);

	owl_draw_text(r, buffer, position, color);

	position[1] += 0.05F;

	snprintf(buffer, sizeof(buffer), "uniform_buffer_size: %llu",
		 r->uniform_buffer_size);

	owl_draw_text(r, buffer, position, color);

	position[1] += 0.05F;

	snprintf(buffer, sizeof(buffer), "upload_buffer_size: %llu",
		 r->upload_buffer_size);

	owl_draw_text(r, buffer, position, color);

	position[1] += 0.05F;

	snprintf(buffer, sizeof(buffer), "vertex_buffer_last_offset: %llu",
		 r->vertex_buffer_last_offset);

	owl_draw_text(r, buffer, position, color);

	position[1] += 0.05F;

	snprintf(buffer, sizeof(buffer), "index_buffer_last_offset: %llu",
		 r->index_buffer_last_offset);

	owl_draw_text(r, buffer, position, color);

	position[1] += 0.05F;

	snprintf(buffer, sizeof(buffer), "uniform_buffer_last_offset: %llu",
		 r->uniform_buffer_last_offset);

	owl_draw_text(r, buffer, position, color);

	position[1] += 0.05F;

	snprintf(buffer, sizeof(buffer), "color_memory_size: %llu",
		 r->color_memory_size);

	owl_draw_text(r, buffer, position, color);

	position[1] += 0.05F;

	snprintf(buffer, sizeof(buffer), "depth_memory_size: %llu",
		 r->depth_memory_size);

	owl_draw_text(r, buffer, position, color);

	return OWL_OK;
}

OWLAPI int owl_draw_cloth_simulation(struct owl_renderer *r,
				     struct owl_cloth_simulation *sim)
{
	int32_t i;
	int32_t j;

	uint32_t num_indices;
	uint32_t *indices;
	struct owl_renderer_index_allocation index_allocation;

	uint64_t num_vertices;
	struct owl_common_vertex *vertices;
	struct owl_renderer_vertex_allocation vertex_allocation;

	VkDescriptorSet descriptor_sets[2];
	struct owl_common_uniform *uniform;
	struct owl_renderer_uniform_allocation uniform_allocation;

	VkCommandBuffer command_buffer;

	command_buffer = r->submit_command_buffers[r->frame];

	num_indices = (sim->width - 1) * (sim->height - 1) * 6;
	indices = owl_renderer_index_allocate(r, num_indices * sizeof(*indices),
					      &index_allocation);
	if (!indices)
		return OWL_ERROR_NO_FRAME_MEMORY;

	for (i = 0; i < (sim->height - 1); ++i) {
		for (j = 0; j < (sim->width - 1); ++j) {
			int32_t const k = i * sim->width + j;
			int32_t const fixed_k = (i * (sim->width - 1) + j) * 6;

			indices[fixed_k + 0] = k + sim->width;
			indices[fixed_k + 1] = k + sim->width + 1;
			indices[fixed_k + 2] = k + 1;
			indices[fixed_k + 3] = k + 1;
			indices[fixed_k + 4] = k;
			indices[fixed_k + 5] = k + sim->width;
		}
	}

	num_vertices = sim->width * sim->height;
	vertices = owl_renderer_vertex_allocate(
		r, num_vertices * sizeof(*vertices), &vertex_allocation);
	if (!vertices)
		return OWL_ERROR_NO_FRAME_MEMORY;

	for (i = 0; i < sim->height; ++i) {
		for (j = 0; j < sim->width; ++j) {
			int32_t k = i * sim->width + j;

			struct owl_cloth_particle *particle =
				&sim->particles[k];
			struct owl_common_vertex *vertex = &vertices[k];

			OWL_V3_COPY(particle->position, vertex->position);
			OWL_V3_SET(vertex->color, 1.0F, 1.0F, 1.0F);
			vertex->uv[0] = (float)j / (float)sim->width;
			vertex->uv[1] = (float)i / (float)sim->height;
		}
	}

	uniform = owl_renderer_uniform_allocate(r, sizeof(*uniform),
						&uniform_allocation);
	if (!uniform)
		return OWL_ERROR_NO_FRAME_MEMORY;

	OWL_M4_COPY(r->projection, uniform->projection);
	OWL_M4_COPY(r->view, uniform->view);
	OWL_M4_COPY(sim->model, uniform->model);

	descriptor_sets[0] = uniform_allocation.common_descriptor_set;
	descriptor_sets[1] = sim->material.descriptor_set;

	vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			  r->basic_pipeline);

	vkCmdBindVertexBuffers(command_buffer, 0, 1, &vertex_allocation.buffer,
			       &vertex_allocation.offset);

	vkCmdBindIndexBuffer(command_buffer, index_allocation.buffer,
			     index_allocation.offset, VK_INDEX_TYPE_UINT32);

	vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
				r->common_pipeline_layout, 0,
				OWL_ARRAY_SIZE(descriptor_sets),
				descriptor_sets, 1, &uniform_allocation.offset);

	vkCmdDrawIndexed(command_buffer, num_indices, 1, 0, 0, 0);

	return OWL_OK;
}
