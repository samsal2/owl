#include "owl_vk_renderer.h"

#include "owl_glyph.h"
#include "owl_internal.h"
#include "owl_io.h"
#include "owl_model.h"
#include "owl_quad.h"
#include "owl_skybox.h"
#include "owl_vector_math.h"
#include "owl_vk_font.h"
#include "owl_vk_image.h"
#include "owl_window.h"

#define OWL_DEFAULT_FRAME_SIZE (1 << 16)
#define OWL_DEFAULT_STAGE_SIZE (1 << 16)

owl_private enum owl_code
owl_vk_renderer_frames_init (
    struct owl_vk_frame    frames[OWL_VK_RENDERER_IN_FLIGHT_FRAME_COUNT],
    struct owl_vk_context *ctx)
{
  owl_i32 i;

  enum owl_code code = OWL_SUCCESS;

  for (i = 0; i < OWL_VK_RENDERER_IN_FLIGHT_FRAME_COUNT; ++i)
  {
    code = owl_vk_frame_init (&frames[i], ctx, OWL_DEFAULT_FRAME_SIZE);
    if (OWL_SUCCESS != code)
      goto error_frames_deinit;
  }

  goto out;

error_frames_deinit:
  for (i = i - 1; i >= 0; --i)
    owl_vk_frame_deinit (frames, ctx);

out:
  return code;
}

owl_private void
owl_vk_renderer_frames_deinit (
    struct owl_vk_frame    frames[OWL_VK_RENDERER_IN_FLIGHT_FRAME_COUNT],
    struct owl_vk_context *ctx)
{
  owl_i32 i;

  for (i = 0; i < OWL_VK_RENDERER_IN_FLIGHT_FRAME_COUNT; ++i)
    owl_vk_frame_deinit (&frames[i], ctx);
}

owl_private enum owl_code
owl_vk_renderer_garbages_init (
    struct owl_vk_garbage  garbages[OWL_VK_RENDERER_IN_FLIGHT_FRAME_COUNT],
    struct owl_vk_context *ctx)
{
  owl_i32 i;

  enum owl_code code = OWL_SUCCESS;

  for (i = 0; i < OWL_VK_RENDERER_IN_FLIGHT_FRAME_COUNT; ++i)
  {
    code = owl_vk_garbage_init (&garbages[i], ctx);
    if (OWL_SUCCESS != code)
      goto error_garbages_deinit;
  }

  goto out;

error_garbages_deinit:
  for (i = i - 1; i >= 0; --i)
    owl_vk_garbage_deinit (&garbages[i], ctx);

out:
  return code;
}

owl_private void
owl_vk_renderer_garbages_deinit (
    struct owl_vk_garbage  garbages[OWL_VK_RENDERER_IN_FLIGHT_FRAME_COUNT],
    struct owl_vk_context *ctx)
{
  owl_i32 i;

  for (i = 0; i < OWL_VK_RENDERER_IN_FLIGHT_FRAME_COUNT; ++i)
    owl_vk_garbage_deinit (&garbages[i], ctx);
}

owl_public enum owl_code
owl_vk_renderer_init (struct owl_vk_renderer *vkr, struct owl_window *window)
{
  owl_i32       w;
  owl_i32       h;
  float         ratio;
  enum owl_code code;

  owl_window_get_framebuffer_size (window, &w, &h);
  ratio = (float)w / (float)h;

  vkr->current_time  = 0;
  vkr->previous_time = 0;
  vkr->width         = w;
  vkr->height        = h;

  code = owl_camera_init (&vkr->camera, ratio);
  if (OWL_SUCCESS != code)
    goto out;

  code = owl_vk_context_init (&vkr->context, window);
  if (OWL_SUCCESS != code)
    goto error_camera_deinit;

  code = owl_vk_attachment_init (&vkr->color_attachment, &vkr->context, w, h,
                                 OWL_VK_ATTACHMENT_TYPE_COLOR);
  if (OWL_SUCCESS != code)
    goto error_context_deinit;

  code = owl_vk_attachment_init (&vkr->depth_attachment, &vkr->context, w, h,
                                 OWL_VK_ATTACHMENT_TYPE_DEPTH_STENCIL);
  if (OWL_SUCCESS != code)
    goto error_color_attachment_deinit;

  code
      = owl_vk_swapchain_init (&vkr->swapchain, &vkr->context,
                               &vkr->color_attachment, &vkr->depth_attachment);
  if (OWL_SUCCESS != code)
    goto error_depth_attachment_deinit;

  code = owl_vk_pipelines_init (&vkr->pipelines, &vkr->context,
                                &vkr->swapchain);
  if (OWL_SUCCESS != code)
    goto error_swapchain_deinit;

  code = owl_vk_renderer_garbages_init (vkr->garbages, &vkr->context);
  if (OWL_SUCCESS != code)
    goto error_pipelines_deinit;

  code = owl_vk_stage_heap_init (&vkr->stage_heap, &vkr->context,
                                 OWL_DEFAULT_STAGE_SIZE);
  if (OWL_SUCCESS != code)
    goto error_garbages_deinit;

  vkr->font  = NULL;
  vkr->frame = 0;

  code = owl_vk_renderer_frames_init (vkr->frames, &vkr->context);
  if (OWL_SUCCESS != code)
    goto error_stage_heap_deinit;

  goto out;

error_stage_heap_deinit:
  owl_vk_stage_heap_deinit (&vkr->stage_heap, &vkr->context);

error_garbages_deinit:
  owl_vk_renderer_garbages_deinit (vkr->garbages, &vkr->context);

error_pipelines_deinit:
  owl_vk_pipelines_deinit (&vkr->pipelines, &vkr->context);

error_swapchain_deinit:
  owl_vk_swapchain_deinit (&vkr->swapchain, &vkr->context);

error_depth_attachment_deinit:
  owl_vk_attachment_deinit (&vkr->depth_attachment, &vkr->context);

error_color_attachment_deinit:
  owl_vk_attachment_deinit (&vkr->color_attachment, &vkr->context);

error_context_deinit:
  owl_vk_context_deinit (&vkr->context);

error_camera_deinit:
  owl_camera_deinit (&vkr->camera);

out:
  return code;
}

owl_public void
owl_vk_renderer_deinit (struct owl_vk_renderer *vkr)
{
  owl_vk_renderer_frames_deinit (vkr->frames, &vkr->context);
  owl_vk_stage_heap_deinit (&vkr->stage_heap, &vkr->context);
  owl_vk_renderer_garbages_deinit (vkr->garbages, &vkr->context);
  owl_vk_pipelines_deinit (&vkr->pipelines, &vkr->context);
  owl_vk_swapchain_deinit (&vkr->swapchain, &vkr->context);
  owl_vk_attachment_deinit (&vkr->depth_attachment, &vkr->context);
  owl_vk_attachment_deinit (&vkr->color_attachment, &vkr->context);
  owl_vk_context_deinit (&vkr->context);
  owl_camera_deinit (&vkr->camera);
}

owl_public enum owl_code
owl_vk_renderer_resize (struct owl_vk_renderer *vkr, owl_i32 w, owl_i32 h)
{
  enum owl_code        code  = OWL_SUCCESS;
  struct owl_vk_frame *frame = owl_vk_renderer_get_frame (vkr);

  code = owl_vk_context_device_wait (&vkr->context);
  if (OWL_SUCCESS != code)
    goto out;

  /* FIXME(samuel): could in theory init first, deinit second. But if init
   *                it would probably just lead to an outdate swapchain spam
   *                and all would already would be too broken to continue
   */

  code = owl_vk_frame_resync (frame, &vkr->context);
  if (OWL_SUCCESS != code)
    goto out;

  owl_vk_pipelines_deinit (&vkr->pipelines, &vkr->context);
  owl_vk_swapchain_deinit (&vkr->swapchain, &vkr->context);
  owl_vk_attachment_deinit (&vkr->depth_attachment, &vkr->context);
  owl_vk_attachment_deinit (&vkr->color_attachment, &vkr->context);
  owl_camera_deinit (&vkr->camera);

  /* FIXME (samuel): maybe, don't reset the camera when resizing? */
  code = owl_camera_init (&vkr->camera, (float)w / (float)h);
  if (OWL_SUCCESS != code)
    goto out;

  code = owl_vk_attachment_init (&vkr->color_attachment, &vkr->context, w, h,
                                 OWL_VK_ATTACHMENT_TYPE_COLOR);
  if (OWL_SUCCESS != code)
    goto error_camera_deinit;

  code = owl_vk_attachment_init (&vkr->depth_attachment, &vkr->context, w, h,
                                 OWL_VK_ATTACHMENT_TYPE_DEPTH_STENCIL);
  if (OWL_SUCCESS != code)
    goto error_color_attachment_deinit;

  code
      = owl_vk_swapchain_init (&vkr->swapchain, &vkr->context,
                               &vkr->color_attachment, &vkr->depth_attachment);
  if (OWL_SUCCESS != code)
    goto error_depth_attachment_deinit;

  code = owl_vk_pipelines_init (&vkr->pipelines, &vkr->context,
                                &vkr->swapchain);
  if (OWL_SUCCESS != code)
    goto error_swapchain_deinit;

  goto out;

  /* FIXME(samuel): if anything fails, then vkr is in an invalid state */
error_swapchain_deinit:
  owl_vk_swapchain_deinit (&vkr->swapchain, &vkr->context);

error_depth_attachment_deinit:
  owl_vk_attachment_deinit (&vkr->depth_attachment, &vkr->context);

error_color_attachment_deinit:
  owl_vk_attachment_deinit (&vkr->color_attachment, &vkr->context);

error_camera_deinit:
  owl_camera_deinit (&vkr->camera);

out:
  return code;
}

owl_private struct owl_vk_garbage *
owl_vk_renderer_garbage_get (struct owl_vk_renderer *vkr)
{
  owl_paranoid_assert (vkr->frame >= 0);
  owl_paranoid_assert (vkr->frame < OWL_VK_RENDERER_IN_FLIGHT_FRAME_COUNT);
  return &vkr->garbages[vkr->frame];
}

owl_public void *
owl_vk_renderer_frame_allocate (struct owl_vk_renderer         *vkr,
                                owl_u64                         size,
                                struct owl_vk_frame_allocation *alloc)
{
  struct owl_vk_frame   *frame   = owl_vk_renderer_get_frame (vkr);
  struct owl_vk_garbage *garbage = owl_vk_renderer_garbage_get (vkr);
  return owl_vk_frame_allocate (frame, &vkr->context, garbage, size, alloc);
}

owl_public void *
owl_vk_renderer_stage_allocate (struct owl_vk_renderer         *vkr,
                                owl_u64                         size,
                                struct owl_vk_stage_allocation *alloc)
{
  return owl_vk_stage_heap_allocate (&vkr->stage_heap, &vkr->context, size,
                                     alloc);
}

owl_public void
owl_vk_renderer_stage_free (struct owl_vk_renderer *vkr)
{
  owl_vk_stage_heap_free (&vkr->stage_heap, &vkr->context);
}

owl_public void
owl_vk_renderer_frame_free (struct owl_vk_renderer *vkr)
{
  owl_vk_frame_free (owl_vk_renderer_get_frame (vkr), &vkr->context);
}

owl_public void
owl_vk_renderer_set_font (struct owl_vk_renderer *vkr,
                          struct owl_vk_font     *font)
{
  vkr->font = font;
}

owl_public enum owl_code
owl_vk_renderer_begin_frame (struct owl_vk_renderer *vkr)
{
  enum owl_code code = OWL_SUCCESS;

  struct owl_vk_frame   *frame   = owl_vk_renderer_get_frame (vkr);
  struct owl_vk_garbage *garbage = owl_vk_renderer_garbage_get (vkr);

  code = owl_vk_frame_begin (frame, &vkr->context, &vkr->swapchain);
  if (OWL_SUCCESS != code)
    return code;

  code = owl_vk_garbage_clear (garbage, &vkr->context);
  if (OWL_SUCCESS != code)
    return code;

  return code;
}

owl_public enum owl_code
owl_vk_renderer_end_frame (struct owl_vk_renderer *vkr)
{
  enum owl_code code;

  struct owl_vk_frame *frame = owl_vk_renderer_get_frame (vkr);

  code = owl_vk_frame_end (frame, &vkr->context, &vkr->swapchain);
  if (OWL_SUCCESS != code)
    return code;

  if (OWL_VK_RENDERER_IN_FLIGHT_FRAME_COUNT == ++vkr->frame)
    vkr->frame = 0;

  vkr->previous_time = vkr->current_time;
  vkr->current_time  = owl_io_time_stamp_get ();

  code = owl_vk_renderer_bind_pipeline (vkr, OWL_VK_PIPELINE_ID_NONE);
  if (OWL_SUCCESS != code)
    return code;

  return code;
}
owl_public struct owl_vk_frame *
owl_vk_renderer_get_frame (struct owl_vk_renderer *vkr)
{
  owl_paranoid_assert (vkr->frame >= 0);
  owl_paranoid_assert (vkr->frame < OWL_VK_RENDERER_IN_FLIGHT_FRAME_COUNT);
  return &vkr->frames[vkr->frame];
}

owl_public enum owl_code
owl_vk_renderer_bind_pipeline (struct owl_vk_renderer *vkr,
                               enum owl_vk_pipeline_id id)
{
  struct owl_vk_frame *frame = owl_vk_renderer_get_frame (vkr);
  return owl_vk_pipelines_bind (&vkr->pipelines, id, frame);
}

owl_public enum owl_code
owl_vk_renderer_draw_quad (struct owl_vk_renderer *vkr,
                           struct owl_quad const  *q,
                           owl_m4 const            matrix)
{
  owl_byte                      *data;
  VkDescriptorSet                sets[2];
  struct owl_vk_frame           *frame;
  struct owl_pvm_ubo             ubo;
  struct owl_pcu_vertex          vertices[4];
  struct owl_vk_frame_allocation valloc;
  struct owl_vk_frame_allocation ialloc;
  struct owl_vk_frame_allocation ualloc;

  owl_local_persist owl_u32 const indices[] = { 2, 3, 1, 1, 0, 2 };

  vertices[0].position[0] = q->position0[0];
  vertices[0].position[1] = q->position0[1];
  vertices[0].position[2] = 0.0F;
  vertices[0].color[0]    = q->color[0];
  vertices[0].color[1]    = q->color[1];
  vertices[0].color[2]    = q->color[2];
  vertices[0].uv[0]       = q->uv0[0];
  vertices[0].uv[1]       = q->uv0[1];

  vertices[1].position[0] = q->position1[0];
  vertices[1].position[1] = q->position0[1];
  vertices[1].position[2] = 0.0F;
  vertices[1].color[0]    = q->color[0];
  vertices[1].color[1]    = q->color[1];
  vertices[1].color[2]    = q->color[2];
  vertices[1].uv[0]       = q->uv1[0];
  vertices[1].uv[1]       = q->uv0[1];

  vertices[2].position[0] = q->position0[0];
  vertices[2].position[1] = q->position1[1];
  vertices[2].position[2] = 0.0F;
  vertices[2].color[0]    = q->color[0];
  vertices[2].color[1]    = q->color[1];
  vertices[2].color[2]    = q->color[2];
  vertices[2].uv[0]       = q->uv0[0];
  vertices[2].uv[1]       = q->uv1[1];

  vertices[3].position[0] = q->position1[0];
  vertices[3].position[1] = q->position1[1];
  vertices[3].position[2] = 0.0F;
  vertices[3].color[0]    = q->color[0];
  vertices[3].color[1]    = q->color[1];
  vertices[3].color[2]    = q->color[2];
  vertices[3].uv[0]       = q->uv1[0];
  vertices[3].uv[1]       = q->uv1[1];

  owl_m4_copy (vkr->camera.projection, ubo.projection);
  owl_m4_copy (vkr->camera.view, ubo.view);
  owl_m4_copy (matrix, ubo.model);

  data = owl_vk_renderer_frame_allocate (vkr, sizeof (vertices), &valloc);
  if (!data)
    return OWL_ERROR_NO_FRAME_MEMORY;
  owl_memcpy (data, vertices, sizeof (vertices));

  data = owl_vk_renderer_frame_allocate (vkr, sizeof (indices), &ialloc);
  if (!data)
    return OWL_ERROR_NO_FRAME_MEMORY;
  owl_memcpy (data, indices, sizeof (indices));

  data = owl_vk_renderer_frame_allocate (vkr, sizeof (ubo), &ualloc);
  if (!data)
    return OWL_ERROR_NO_FRAME_MEMORY;
  owl_memcpy (data, &ubo, sizeof (ubo));

  sets[0] = ualloc.vk_pvm_ubo_set;
  sets[1] = q->texture->vk_set;

  frame = owl_vk_renderer_get_frame (vkr);
  vkCmdBindVertexBuffers (frame->vk_command_buffer, 0, 1, &valloc.vk_buffer,
                          &valloc.offset);

  vkCmdBindIndexBuffer (frame->vk_command_buffer, ialloc.vk_buffer,
                        ialloc.offset, VK_INDEX_TYPE_UINT32);

  vkCmdBindDescriptorSets (frame->vk_command_buffer,
                           VK_PIPELINE_BIND_POINT_GRAPHICS,
                           owl_vk_pipelines_get_layout (&vkr->pipelines), 0,
                           owl_array_size (sets), sets, 1, &ualloc.offset32);

  vkCmdDrawIndexed (frame->vk_command_buffer, owl_array_size (indices), 1, 0,
                    0, 0);

  return OWL_SUCCESS;
}

owl_public enum owl_code
owl_vk_renderer_draw_glyph (struct owl_vk_renderer *vkr,
                            struct owl_glyph const *glyph,
                            owl_v3 const            color)
{
  owl_m4          matrix;
  owl_v3          scale;
  struct owl_quad quad;

  scale[0] = 2.0F / (float)vkr->height;
  scale[1] = 2.0F / (float)vkr->height;
  scale[2] = 2.0F / (float)vkr->height;

  owl_m4_identity (matrix);
  owl_m4_scale_v3 (matrix, scale, matrix);

  quad.texture = &vkr->font->atlas;

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

  return owl_vk_renderer_draw_quad (vkr, &quad, matrix);
}

owl_public enum owl_code
owl_vk_renderer_draw_text (struct owl_vk_renderer *vkr,
                           char const             *text,
                           owl_v3 const            position,
                           owl_v3 const            color)
{
  char const *c;
  owl_v2      offset;

  enum owl_code code;

  offset[0] = position[0] * (float)vkr->width;
  offset[1] = position[1] * (float)vkr->height;

  for (c = &text[0]; '\0' != *c; ++c)
  {
    struct owl_glyph glyph;

    code = owl_vk_font_fill_glyph (vkr->font, *c, offset, &glyph);
    if (OWL_SUCCESS != code)
      return code;

    code = owl_vk_renderer_draw_glyph (vkr, &glyph, color);
    if (OWL_SUCCESS != code)
      return code;
  }

  return OWL_SUCCESS;
}

owl_private enum owl_code
owl_vk_renderer_draw_model_node (struct owl_vk_renderer *vkr,
                                 owl_model_node_id       id,
                                 struct owl_model const *model,
                                 owl_m4 const            matrix)
{
  owl_i32                        i;
  owl_byte                      *data;
  owl_model_node_id              parent;
  struct owl_model_node const   *node;
  struct owl_model_mesh const   *mesh;
  struct owl_model_skin const   *skin;
  struct owl_model_skin_ssbo    *ssbo;
  struct owl_model_ubo1          ubo1;
  struct owl_model_ubo2          ubo2;
  struct owl_vk_frame           *frame;
  struct owl_vk_frame_allocation u1alloc;
  struct owl_vk_frame_allocation u2alloc;

  enum owl_code code;

  node  = &model->nodes[id];
  frame = owl_vk_renderer_get_frame (vkr);

  for (i = 0; i < node->child_count; ++i)
  {
    code = owl_vk_renderer_draw_model_node (vkr, node->children[i], model,
                                            matrix);
    if (OWL_SUCCESS != code)
      return code;
  }

  if (OWL_MODEL_MESH_NONE == node->mesh)
    return OWL_SUCCESS;

  mesh = &model->meshes[node->mesh];
  skin = &model->skins[node->skin];
  ssbo = skin->ssbos[vkr->frame];

  owl_m4_copy (node->matrix, ssbo->matrix);

  for (parent = node->parent; OWL_MODEL_NODE_PARENT_NONE != parent;
       parent = model->nodes[parent].parent)
    owl_m4_multiply (model->nodes[parent].matrix, ssbo->matrix, ssbo->matrix);

  owl_m4_copy (vkr->camera.projection, ubo1.projection);
  owl_m4_copy (vkr->camera.view, ubo1.view);
  owl_m4_copy (matrix, ubo1.model);
  owl_v4_zero (ubo1.light);
  owl_v4_zero (ubo2.light_direction);

  data = owl_vk_renderer_frame_allocate (vkr, sizeof (ubo1), &u1alloc);
  if (!data)
    return OWL_ERROR_NO_FRAME_MEMORY;
  owl_memcpy (data, &ubo1, sizeof (ubo1));

  vkCmdBindDescriptorSets (frame->vk_command_buffer,
                           VK_PIPELINE_BIND_POINT_GRAPHICS,
                           owl_vk_pipelines_get_layout (&vkr->pipelines), 0, 1,
                           &u1alloc.vk_model_ubo1_set, 1, &u1alloc.offset32);

  data = owl_vk_renderer_frame_allocate (vkr, sizeof (ubo2), &u2alloc);
  if (!data)
    return OWL_ERROR_NO_FRAME_MEMORY;
  owl_memcpy (data, &ubo2, sizeof (ubo2));

  vkCmdBindDescriptorSets (frame->vk_command_buffer,
                           VK_PIPELINE_BIND_POINT_GRAPHICS,
                           owl_vk_pipelines_get_layout (&vkr->pipelines), 5, 1,
                           &u1alloc.vk_model_ubo2_set, 1, &u1alloc.offset32);

  for (i = 0; i < mesh->primitive_count; ++i)
  {
    VkDescriptorSet                         sets[4];
    struct owl_model_primitive const       *primitive;
    struct owl_model_material const        *material;
    struct owl_model_texture const         *base_color_texture;
    struct owl_model_texture const         *normal_texture;
    struct owl_model_texture const         *physical_desc_texture;
    struct owl_model_image const           *base_color_image;
    struct owl_model_image const           *normal_image;
    struct owl_model_image const           *physical_desc_image;
    struct owl_model_material_push_constant push_constant;

    primitive = &model->primitives[mesh->primitives[i]];

    if (!primitive->count)
    {
      continue;
    }

    material = &model->materials[primitive->material];

    base_color_texture    = &model->textures[material->base_color_texture];
    normal_texture        = &model->textures[material->normal_texture];
    physical_desc_texture = &model->textures[material->physical_desc_texture];

    base_color_image    = &model->images[base_color_texture->image];
    normal_image        = &model->images[normal_texture->image];
    physical_desc_image = &model->images[physical_desc_texture->image];

    sets[0] = base_color_image->image.vk_set;
    sets[1] = normal_image->image.vk_set;
    sets[2] = physical_desc_image->image.vk_set;
    sets[3] = skin->ssbo_sets[vkr->frame];

    vkCmdBindDescriptorSets (frame->vk_command_buffer,
                             VK_PIPELINE_BIND_POINT_GRAPHICS,
                             owl_vk_pipelines_get_layout (&vkr->pipelines), 1,
                             owl_array_size (sets), sets, 0, NULL);

    push_constant.base_color_factor[0] = material->base_color_factor[0];
    push_constant.base_color_factor[1] = material->base_color_factor[1];
    push_constant.base_color_factor[2] = material->base_color_factor[2];
    push_constant.base_color_factor[3] = material->base_color_factor[3];

    push_constant.emissive_factor[0] = 0.0F;
    push_constant.emissive_factor[1] = 0.0F;
    push_constant.emissive_factor[2] = 0.0F;
    push_constant.emissive_factor[3] = 0.0F;

    push_constant.diffuse_factor[0] = 0.0F;
    push_constant.diffuse_factor[1] = 0.0F;
    push_constant.diffuse_factor[2] = 0.0F;
    push_constant.diffuse_factor[3] = 0.0F;

    push_constant.specular_factor[0] = 0.0F;
    push_constant.specular_factor[1] = 0.0F;
    push_constant.specular_factor[2] = 0.0F;
    push_constant.specular_factor[3] = 0.0F;

    push_constant.workflow = 0;

    /* FIXME(samuel): add uv sets in material */
    if (OWL_MODEL_TEXTURE_NONE == material->base_color_texture)
    {
      push_constant.base_color_uv_set = -1;
    }
    else
    {
      push_constant.base_color_uv_set = 0;
    }

    if (OWL_MODEL_TEXTURE_NONE == material->physical_desc_texture)
    {
      push_constant.physical_desc_uv_set = -1;
    }
    else
    {
      push_constant.physical_desc_uv_set = 0;
    }

    if (OWL_MODEL_TEXTURE_NONE == material->normal_texture)
    {
      push_constant.normal_uv_set = -1;
    }
    else
    {
      push_constant.normal_uv_set = 0;
    }

    if (OWL_MODEL_TEXTURE_NONE == material->occlusion_texture)
    {
      push_constant.occlusion_uv_set = -1;
    }
    else
    {
      push_constant.occlusion_uv_set = 0;
    }

    if (OWL_MODEL_TEXTURE_NONE == material->emissive_texture)
    {
      push_constant.emissive_uv_set = -1;
    }
    else
    {
      push_constant.emissive_uv_set = 0;
    }

    push_constant.metallic_factor   = 0.0F;
    push_constant.roughness_factor  = 0.0F;
    push_constant.roughness_factor  = 0.0F;
    push_constant.alpha_mask        = 0.0F;
    push_constant.alpha_mask_cutoff = material->alpha_cutoff;

    vkCmdPushConstants (frame->vk_command_buffer,
                        owl_vk_pipelines_get_layout (&vkr->pipelines),
                        VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                        sizeof (push_constant), &push_constant);

    vkCmdDrawIndexed (frame->vk_command_buffer, primitive->count, 1,
                      primitive->first, 0, 0);
  }

  return OWL_SUCCESS;
}

owl_public enum owl_code
owl_vk_renderer_draw_model (struct owl_vk_renderer *vkr,
                            struct owl_model const *model,
                            owl_m4 const            matrix)
{
  owl_i32 i;

  owl_u64       offset = 0;
  enum owl_code code   = OWL_SUCCESS;

  struct owl_vk_frame *frame = owl_vk_renderer_get_frame (vkr);

  code = owl_vk_renderer_bind_pipeline (vkr, OWL_VK_PIPELINE_ID_MODEL);
  if (OWL_SUCCESS != code)
    return code;

  vkCmdBindVertexBuffers (frame->vk_command_buffer, 0, 1,
                          &model->vk_vertex_buffer, &offset);

  vkCmdBindIndexBuffer (frame->vk_command_buffer, model->vk_index_buffer, 0,
                        VK_INDEX_TYPE_UINT32);

  for (i = 0; i < model->root_count; ++i)
  {
    owl_model_node_id const root = model->roots[i];
    code = owl_vk_renderer_draw_model_node (vkr, root, model, matrix);
    if (OWL_SUCCESS != code)
      return code;
  }

  return OWL_SUCCESS;
}

owl_public enum owl_code
owl_vk_renderer_draw_skybox (struct owl_vk_renderer  *vkr,
                             struct owl_skybox const *sb)
{
  owl_byte                      *data;
  struct owl_vk_frame_allocation valloc;
  struct owl_vk_frame_allocation ialloc;
  struct owl_vk_frame_allocation ualloc;

  VkDescriptorSet sets[2];

  struct owl_pvm_ubo ubo;

  /*
   *    4----5
   *  / |  / |
   * 0----1  |
   * |  | |  |
   * |  6-|--7
   * | /  | /
   * 2----3
   * i reaaaalllly hate italics
   */
  owl_local_persist struct owl_p_vertex const vertices[] = {
    {-1.0F, -1.0F, -1.0F}, /* 0 */
    { 1.0F, -1.0F, -1.0F}, /* 1 */
    {-1.0F,  1.0F, -1.0F}, /* 2 */
    { 1.0F,  1.0F, -1.0F}, /* 3 */
    {-1.0F, -1.0F,  1.0F}, /* 4 */
    { 1.0F, -1.0F,  1.0F}, /* 5 */
    {-1.0F,  1.0F,  1.0F}, /* 6 */
    { 1.0F,  1.0F,  1.0F}
  }; /* 7 */

  owl_local_persist owl_u32 const indices[]
      = { 2, 3, 1, 1, 0, 2,   /* face 0 ....*/
          3, 7, 5, 5, 1, 3,   /* face 1 */
          6, 2, 0, 0, 4, 6,   /* face 2 */
          7, 6, 4, 4, 5, 7,   /* face 3 */
          3, 2, 6, 6, 7, 3,   /* face 4 */
          4, 0, 1, 1, 5, 4 }; /* face 5 */

  struct owl_vk_frame *frame = owl_vk_renderer_get_frame (vkr);

  owl_vk_renderer_bind_pipeline (vkr, OWL_VK_PIPELINE_ID_SKYBOX);

  data = owl_vk_renderer_frame_allocate (vkr, sizeof (vertices), &valloc);
  if (!data)
    return OWL_ERROR_NO_FRAME_MEMORY;
  owl_memcpy (data, vertices, sizeof (vertices));

  data = owl_vk_renderer_frame_allocate (vkr, sizeof (indices), &ialloc);
  if (!data)
    return OWL_ERROR_NO_FRAME_MEMORY;
  owl_memcpy (data, indices, sizeof (indices));

  owl_m4_copy (vkr->camera.projection, ubo.projection);
  owl_m4_identity (ubo.view);
  owl_m3_copy (vkr->camera.view, ubo.view);
  owl_m4_identity (ubo.model);

  data = owl_vk_renderer_frame_allocate (vkr, sizeof (ubo), &ualloc);
  if (!data)
    return OWL_ERROR_NO_FRAME_MEMORY;
  owl_memcpy (data, &ubo, sizeof (ubo));

  sets[0] = ualloc.vk_pvm_ubo_set;
  sets[1] = sb->vk_set;

  vkCmdBindVertexBuffers (frame->vk_command_buffer, 0, 1, &valloc.vk_buffer,
                          &valloc.offset);

  vkCmdBindIndexBuffer (frame->vk_command_buffer, ialloc.vk_buffer,
                        ialloc.offset, VK_INDEX_TYPE_UINT32);

  vkCmdBindDescriptorSets (frame->vk_command_buffer,
                           VK_PIPELINE_BIND_POINT_GRAPHICS,
                           owl_vk_pipelines_get_layout (&vkr->pipelines), 0,
                           owl_array_size (sets), sets, 1, &ualloc.offset32);

  vkCmdDrawIndexed (frame->vk_command_buffer, owl_array_size (indices), 1, 0,
                    0, 0);

  return OWL_SUCCESS;
}
