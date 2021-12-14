#ifndef OWL_TEXTURE_INL_
#define OWL_TEXTURE_INL_

#include <owl/fwd.h>
#include <owl/texture.h>
#include <owl/types.h>
#include <vulkan/vulkan.h>

#define OWL_VK_TEXTURE_COUNT 16

struct owl_tmp_submit_mem_ref;

typedef OwlU32 OtterVkMipLevels;

struct owl_vk_texture {
  VkImage img;
  VkImageView view;
  VkDeviceMemory mem;
  VkDescriptorSet set;
  OtterVkMipLevels mips;
  struct owl_extent size;
};

struct owl_vk_texture_manager {
  int current;
  int slots[OWL_VK_TEXTURE_COUNT];
  struct owl_vk_texture textures[OWL_VK_TEXTURE_COUNT];
};

enum owl_code owl_init_texture(struct owl_renderer *renderer, int width,
                               int height, OwlByte const *data,
                               enum owl_pixel_format format,
                               enum owl_sampler_type sampler,
                               struct owl_vk_texture *texture);

enum owl_code owl_init_texture_with_ref(
    struct owl_renderer *renderer, int width, int height,
    struct owl_tmp_submit_mem_ref const *ref, enum owl_pixel_format format,
    enum owl_sampler_type sampler, struct owl_vk_texture *texture);

enum owl_code owl_create_texture_with_ref(
    struct owl_renderer *renderer, int width, int height,
    struct owl_tmp_submit_mem_ref const *ref, enum owl_pixel_format format,
    enum owl_sampler_type sampler, OwlTexture *texture);

void owl_deinit_texture(struct owl_renderer const *renderer,
                        struct owl_vk_texture *texture);

enum owl_code
owl_init_texture_manager(struct owl_renderer const *renderer,
                         struct owl_vk_texture_manager *manager);

void owl_deinit_texture_manager(struct owl_renderer const *renderer,
                                struct owl_vk_texture_manager *manager);

struct owl_vk_texture *owl_get_texture(OwlTexture texture);

#endif
