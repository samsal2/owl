
#include "vulkan/vulkan_core.h"
#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_vulkan.h>

extern "C" {
#include "client.h"
#include "imgui.h"
#include "internal.h"
#include "renderer.h"
}

extern "C" enum owl_code owl_imgui_init(struct owl_client const *c,
                                        struct owl_renderer const *r,
                                        char const *font) {
  enum owl_code code = OWL_SUCCESS;

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  // ImGui::StyleColorsDark();

  ImGui_ImplGlfw_InitForVulkan(static_cast<GLFWwindow *>(c->window), true);

  auto init_info = ImGui_ImplVulkan_InitInfo();
  init_info.Instance = r->instance;
  init_info.PhysicalDevice = r->physical_device;
  init_info.Device = r->device;
  init_info.QueueFamily = r->graphics_queue_family_index;
  init_info.Queue = r->graphics_queue;
  init_info.PipelineCache = VK_NULL_HANDLE;
  init_info.DescriptorPool = r->common_set_pool;
  init_info.Subpass = 0;
  init_info.MinImageCount = OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT;
  init_info.ImageCount = r->swapchain_images_count;
  init_info.MSAASamples =
      static_cast<VkSampleCountFlagBits>(r->msaa_sample_count);
  init_info.Allocator = NULL;
  init_info.CheckVkResultFn = NULL;
  ImGui_ImplVulkan_Init(&init_info, r->main_render_pass);

  ImGui::GetIO().Fonts->AddFontFromFileTTF(font, 16.0F);

  auto sucb = owl_single_use_command_buffer();
  owl_renderer_init_single_use_command_buffer(r, &sucb);
  ImGui_ImplVulkan_CreateFontsTexture(sucb.command_buffer);
  owl_renderer_deinit_single_use_command_buffer(r, &sucb);
  ImGui_ImplVulkan_DestroyFontUploadObjects();

  return code;
}

extern "C" void owl_imgui_deinit(struct owl_renderer const *r) {
  OWL_VK_CHECK(vkDeviceWaitIdle(r->device));
  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplGlfw_Shutdown();
}

extern "C" enum owl_code owl_imgui_handle_resize(struct owl_client const *c,
                                                 struct owl_renderer const *r) {
  enum owl_code code = OWL_SUCCESS;

  OWL_UNUSED(c);
  OWL_UNUSED(r);

  ImGui_ImplVulkan_SetMinImageCount(OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT);

  return code;
}

extern "C" enum owl_code owl_imgui_begin_frame(struct owl_renderer const *r) {
  enum owl_code code = OWL_SUCCESS;

  OWL_UNUSED(r);

  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  return code;
}

extern "C" enum owl_code owl_imgui_end_frame(struct owl_renderer const *r) {
  enum owl_code code = OWL_SUCCESS;

  ImGui::Render();

  auto *draw_data = ImGui::GetDrawData();
  ImGui_ImplVulkan_RenderDrawData(draw_data, r->active_frame_command_buffer);

  return code;
}

extern "C" void owl_imgui_begin(char const *title) { ImGui::Begin(title); }

extern "C" void owl_imgui_end(void) { ImGui::End(); }

extern "C" void owl_imgui_text(char const *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  ImGui::Text(fmt, args);
  va_end(args);
}
