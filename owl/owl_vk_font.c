#include "owl_vk_font.h"

#include "owl_internal.h"
#include "owl_plataform.h"
#include "owl_vector_math.h"
#include "owl_vk_renderer.h"

#include "stb_truetype.h"

#define OWL_FONT_ATLAS_WIDTH 1024
#define OWL_FONT_ATLAS_HEIGHT 1024
#define OWL_FONT_ATLAS_SIZE (OWL_FONT_ATLAS_HEIGHT * OWL_FONT_ATLAS_WIDTH)

owl_public owl_code
owl_vk_font_load(struct owl_vk_renderer *vk, uint64_t size, char const *path) {
  owl_code code;
  uint8_t *bitmap;
  stbtt_pack_context pack;
  struct owl_plataform_file file;
  struct owl_vk_texture_desc texture_desc;
  
  int stb_result;
  int const width = OWL_FONT_ATLAS_WIDTH;
  int const height = OWL_FONT_ATLAS_HEIGHT;
  int const first = OWL_FONT_FIRST_CHAR;
  int const num = OWL_FONT_NUM_CHARS;

  if (vk->font_loaded)
    owl_vk_font_unload(vk);

  code = owl_plataform_load_file(path, &file);
  if (code)
    return code;


  bitmap = owl_calloc(OWL_FONT_ATLAS_SIZE, sizeof(uint8_t));
  if (!bitmap) {
    code = OWL_ERROR_NO_MEMORY;
    goto error_unload_file;
  }

  stb_result = stbtt_PackBegin(&pack, bitmap, width, height, 0, 1, NULL);
  if (!stb_result) {
    code = OWL_ERROR_FATAL;
    goto error_free_bitmap;
  }

  stbtt_PackSetOversampling(&pack, 2, 2);

  stb_result = stbtt_PackFontRange(&pack, file.data, 0, size, first, num,
                                   (stbtt_packedchar *)(&vk->font_chars[0]));
  if (!stb_result) {
    code = OWL_ERROR_FATAL;
    goto error_pack_end;
  }

  texture_desc.src = OWL_TEXTURE_SRC_DATA;
  texture_desc.src_data = bitmap;
  texture_desc.src_data_width = OWL_FONT_ATLAS_WIDTH;
  texture_desc.src_data_height = OWL_FONT_ATLAS_HEIGHT;
  texture_desc.src_data_pixel_format = OWL_PIXEL_FORMAT_R8_UNORM;

  code = owl_vk_texture_init(&vk->font_atlas, vk, &texture_desc);
  if (!code)
    vk->font_loaded = 1;
  
error_pack_end:
  stbtt_PackEnd(&pack);

error_free_bitmap:
  owl_free(bitmap);

error_unload_file:
  owl_plataform_unload_file(&file);

  return code;
}

owl_public void
owl_vk_font_unload(struct owl_vk_renderer *vk) {
  owl_vk_texture_deinit(&vk->font_atlas, vk);
  vk->font_loaded = 0;
}

owl_public owl_code
owl_vk_font_fill_glyph(struct owl_vk_renderer *vk, char c, owl_v2 offset,
                       struct owl_vk_glyph *glyph) {
  stbtt_aligned_quad quad;
  owl_code code = OWL_OK;

  stbtt_GetPackedQuad((stbtt_packedchar *)(&vk->font_chars[0]),
                      OWL_FONT_ATLAS_WIDTH, OWL_FONT_ATLAS_HEIGHT,
                      c - OWL_FONT_FIRST_CHAR, &offset[0], &offset[1], &quad,
                      1);

  owl_v3_set(glyph->positions[0], quad.x0, quad.y0, 0.0F);
  owl_v3_set(glyph->positions[1], quad.x1, quad.y0, 0.0F);
  owl_v3_set(glyph->positions[2], quad.x0, quad.y1, 0.0F);
  owl_v3_set(glyph->positions[3], quad.x1, quad.y1, 0.0F);

  owl_v2_set(glyph->uvs[0], quad.s0, quad.t0);
  owl_v2_set(glyph->uvs[1], quad.s1, quad.t0);
  owl_v2_set(glyph->uvs[2], quad.s0, quad.t1);
  owl_v2_set(glyph->uvs[3], quad.s1, quad.t1);

  return code;
}
