#include "owl_vk_font.h"

#include "owl_internal.h"
#include "owl_vector_math.h"
#include "owl_vk_context.h"
#include "owl_vk_pipeline_manager.h"
#include "owl_vk_stage_heap.h"
#include "stb_truetype.h"

#include <stdio.h>

owl_static_assert (
    sizeof (struct owl_packed_char) == sizeof (stbtt_packedchar),
    "owl_packed_char and stbtt_packedchar must represent the same struct");

owl_private enum owl_code
owl_vk_font_file_init (char const *path, owl_byte **data) {
  owl_u64 sz;
  FILE *file;

  enum owl_code code = OWL_SUCCESS;

  file = fopen (path, "rb");
  if (!file) {
    code = OWL_ERROR_UNKNOWN;
    goto out;
  }

  fseek (file, 0, SEEK_END);

  sz = ftell (file);

  fseek (file, 0, SEEK_SET);

  *data = owl_malloc (sz);
  if (!*data) {
    code = OWL_ERROR_BAD_ALLOCATION;
    goto out_file_close;
  }

  fread (*data, sz, 1, file);

out_file_close:
  fclose (file);

out:
  return code;
}

owl_private void
owl_vk_font_file_deinit (owl_byte *data) {
  owl_free (data);
}

#define OWL_VK_FONT_ATLAS_WIDTH 1024
#define OWL_VK_FONT_ATLAS_HEIGHT 1024
#define OWL_VK_FONT_ATLAS_SIZE                                                \
  (OWL_VK_FONT_ATLAS_HEIGHT * OWL_VK_FONT_ATLAS_WIDTH)

owl_public enum owl_code
owl_vk_font_init (struct owl_vk_font *font, struct owl_vk_context *ctx,
                  struct owl_vk_stage_heap *heap, char const *path,
                  owl_i32 sz) {
  owl_b32 stb_result;
  owl_byte *file;
  owl_byte *bitmap;

  stbtt_pack_context pack;
  struct owl_vk_image_desc desc;

  enum owl_code code = OWL_SUCCESS;

  code = owl_vk_font_file_init (path, &file);
  if (OWL_SUCCESS != code)
    goto out;

  bitmap = owl_calloc (OWL_VK_FONT_ATLAS_SIZE, sizeof (owl_byte));
  if (!bitmap) {
    code = OWL_ERROR_UNKNOWN;
    goto out_error_file_deinit;
  }

  stb_result = stbtt_PackBegin (&pack, bitmap, OWL_VK_FONT_ATLAS_WIDTH,
                                OWL_VK_FONT_ATLAS_HEIGHT, 0, 1, NULL);
  if (!stb_result) {
    code = OWL_ERROR_UNKNOWN;
    goto out_error_bitmap_deinit;
  }

  stbtt_PackSetOversampling (&pack, 2, 2);

  stb_result = stbtt_PackFontRange (&pack, file, 0, sz, OWL_VK_FONT_FIRST_CHAR,
                                    OWL_VK_FONT_CHAR_COUNT,
                                    (stbtt_packedchar *)(&font->chars[0]));
  if (!stb_result) {
    code = OWL_ERROR_UNKNOWN;
    goto out_error_pack_deinit;
  }

  stbtt_PackEnd (&pack);

  desc.src_type = OWL_VK_IMAGE_SRC_TYPE_DATA;
  desc.src_path = NULL;
  desc.src_data = bitmap;
  desc.src_data_width = OWL_VK_FONT_ATLAS_WIDTH;
  desc.src_data_height = OWL_VK_FONT_ATLAS_HEIGHT;
  desc.src_data_pixel_format = OWL_PIXEL_FORMAT_R8_UNORM;
  desc.use_default_sampler = 0;
  desc.sampler_mip_mode = OWL_SAMPLER_MIP_MODE_NEAREST;
  desc.sampler_min_filter = OWL_SAMPLER_FILTER_NEAREST;
  desc.sampler_mag_filter = OWL_SAMPLER_FILTER_NEAREST;
  desc.sampler_wrap_u = OWL_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  desc.sampler_wrap_v = OWL_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  desc.sampler_wrap_w = OWL_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

  code = owl_vk_image_init (&font->atlas, ctx, heap, &desc);
  if (OWL_SUCCESS != code) {
    code = OWL_ERROR_UNKNOWN;
    goto out_error_pack_deinit;
  }

  owl_free (bitmap);
  owl_vk_font_file_deinit (file);

  goto out;

out_error_pack_deinit:
  stbtt_PackEnd (&pack);

out_error_bitmap_deinit:
  owl_free (bitmap);

out_error_file_deinit:
  owl_vk_font_file_deinit (file);

out:
  return code;
}

owl_public void
owl_vk_font_deinit (struct owl_vk_font *font,
                    struct owl_vk_context const *ctx) {
  owl_vk_image_deinit (&font->atlas, ctx);
}

owl_public enum owl_code
owl_vk_font_fill_glyph (struct owl_vk_font *font, char c, owl_v2 offset,
                        struct owl_glyph *glyph) {
  stbtt_aligned_quad quad;
  enum owl_code code = OWL_SUCCESS;

  stbtt_GetPackedQuad ((stbtt_packedchar *)(&font->chars[0]),
                       OWL_VK_FONT_ATLAS_WIDTH, OWL_VK_FONT_ATLAS_HEIGHT,
                       c - OWL_VK_FONT_FIRST_CHAR, &offset[0], &offset[1],
                       &quad, 1);

  owl_v3_set (glyph->positions[0], quad.x0, quad.y0, 0.0F);
  owl_v3_set (glyph->positions[1], quad.x1, quad.y0, 0.0F);
  owl_v3_set (glyph->positions[2], quad.x0, quad.y1, 0.0F);
  owl_v3_set (glyph->positions[3], quad.x1, quad.y1, 0.0F);

  owl_v2_set (glyph->uvs[0], quad.s0, quad.t0);
  owl_v2_set (glyph->uvs[1], quad.s1, quad.t0);
  owl_v2_set (glyph->uvs[2], quad.s0, quad.t1);
  owl_v2_set (glyph->uvs[3], quad.s1, quad.t1);

  return code;
}
