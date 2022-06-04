#include "owl_vk_font.h"

#include "owl_internal.h"
#include "owl_vector_math.h"
#include "owl_vk_renderer.h"

#include "stb_truetype.h"

#include <stdio.h>

owl_static_assert(
    sizeof(struct owl_vk_packed_char) == sizeof(stbtt_packedchar),
    "owl_packed_char and stbtt_packedchar must represent the same struct");

owl_private owl_code
owl_vk_load_file(char const *path, uint8_t **file)
{
  FILE *fp;

  owl_code code = OWL_OK;

  fp = fopen(path, "rb");
  if (fp) {
    uint64_t size;

    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    *file = owl_malloc(size);
    if (*file) {
      fread(*file, size, 1, fp);
    } else {
      code = OWL_ERROR_NO_MEMORY;
    }

    fclose(fp);
  } else {
    code = OWL_ERROR_NOT_FOUND;
  }

  return code;
}

owl_private void
owl_vk_unload_file(uint8_t *data)
{
  owl_free(data);
}

#define OWL_VK_FONT_ATLAS_WIDTH 1024
#define OWL_VK_FONT_ATLAS_HEIGHT 1024
#define OWL_VK_FONT_ATLAS_SIZE                                                \
  (OWL_VK_FONT_ATLAS_HEIGHT * OWL_VK_FONT_ATLAS_WIDTH)

owl_public owl_code
owl_vk_font_load(struct owl_vk_renderer *vk, uint64_t size, char const *path)
{
  int res;
  uint8_t *file;
  uint8_t *bitmap;

  stbtt_pack_context pack;
  struct owl_vk_texture_desc desc;

  owl_code code = OWL_OK;

  if (vk->font_loaded)
    owl_vk_font_unload(vk);

  code = owl_vk_load_file(path, &file);
  if (OWL_OK != code)
    goto out;

  bitmap = owl_calloc(OWL_VK_FONT_ATLAS_SIZE, sizeof(uint8_t));
  if (!bitmap) {
    code = OWL_ERROR_FATAL;
    goto error_unload_file;
  }

  res = stbtt_PackBegin(&pack, bitmap, OWL_VK_FONT_ATLAS_WIDTH,
                        OWL_VK_FONT_ATLAS_HEIGHT, 0, 1, NULL);
  if (!res) {
    code = OWL_ERROR_FATAL;
    goto error_free_bitmap;
  }

  stbtt_PackSetOversampling(&pack, 2, 2);

  res = stbtt_PackFontRange(&pack, file, 0, size, OWL_FONT_FIRST_CHAR,
                            OWL_FONT_NUM_CHARS,
                            (stbtt_packedchar *)(&vk->font_chars[0]));
  if (!res) {
    code = OWL_ERROR_FATAL;
    goto error_pack_end;
  }

  stbtt_PackEnd(&pack);

  desc.src = OWL_TEXTURE_SRC_DATA;
  desc.src_data = bitmap;
  desc.src_data_width = OWL_VK_FONT_ATLAS_WIDTH;
  desc.src_data_height = OWL_VK_FONT_ATLAS_HEIGHT;
  desc.src_data_pixel_format = OWL_PIXEL_FORMAT_R8_UNORM;

  code = owl_vk_texture_init(&vk->font_atlas, vk, &desc);
  if (code) {
    code = OWL_ERROR_FATAL;
    goto error_pack_end;
  }

  owl_free(bitmap);

  owl_vk_unload_file(file);

  vk->font_loaded = 1;

  goto out;

error_pack_end:
  stbtt_PackEnd(&pack);

error_free_bitmap:
  owl_free(bitmap);

error_unload_file:
  owl_vk_unload_file(file);

out:
  return code;
}

owl_public void
owl_vk_font_unload(struct owl_vk_renderer *vk)
{
  owl_vk_texture_deinit(&vk->font_atlas, vk);
  vk->font_loaded = 0;
}

owl_public owl_code
owl_vk_font_fill_glyph(struct owl_vk_renderer *vk, char c, owl_v2 offset,
                       struct owl_vk_glyph *glyph)
{
  stbtt_aligned_quad quad;
  owl_code code = OWL_OK;

  stbtt_GetPackedQuad((stbtt_packedchar *)(&vk->font_chars[0]),
                      OWL_VK_FONT_ATLAS_WIDTH, OWL_VK_FONT_ATLAS_HEIGHT,
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
