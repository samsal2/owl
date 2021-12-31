#ifndef OWL_SOURCE_IMG_H_
#define OWL_SOURCE_IMG_H_

#include "../include/owl/code.h"
#include "../include/owl/types.h"
#include <vulkan/vulkan.h>

struct owl_vk_renderer;
struct owl_dyn_buf_ref;
struct owl_img_desc;

struct owl_img {
  OwlU32 width;
  OwlU32 height;
  OwlU32 mips;
  VkImage img;
  VkImageView view;
  VkDeviceMemory mem;
  VkDescriptorSet set;
  VkSampler sampler;
};

enum owl_code owl_init_vk_img_from_ref(struct owl_vk_renderer *renderer,
                                       struct owl_img_desc const *attr,
                                       struct owl_dyn_buf_ref const *ref,
                                       struct owl_img *img);

enum owl_code owl_init_vk_img(struct owl_vk_renderer *renderer,
                              struct owl_img_desc const *attr,
                              OwlByte const *data, struct owl_img *img);

enum owl_code owl_init_vk_img_from_file(struct owl_vk_renderer *renderer,
                                        struct owl_img_desc *attr,
                                        char const *path, struct owl_img *img);

void owl_deinit_vk_img(struct owl_vk_renderer const *renderer,
                       struct owl_img *img);

#endif
