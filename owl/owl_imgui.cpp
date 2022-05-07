#include "owl_imgui.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include "owl_client.h"
#include "owl_renderer.h"
#include "owl_types.h"

#include <cstdarg>

enum owl_code owl_imgui_init(struct owl_client *c, struct owl_renderer *r,
                             struct owl_imgui *im) {
  auto code = OWL_SUCCESS;

  im->data_ = nullptr;

  ImGui::CreateContext();
#if 0
  ImGui::StyleColorsDark();
#else
  ImGui::StyleColorsClassic();
#endif

  ImGui_ImplGlfw_InitForVulkan(static_cast<GLFWwindow *>(c->window), true);

  auto info = ImGui_ImplVulkan_InitInfo();

  info.Instance = r->instance;
  info.PhysicalDevice = r->physical_device;
  info.Device = r->device;
  info.QueueFamily = r->graphics_queue_family;
  info.Queue = r->graphics_queue;
  info.PipelineCache = VK_NULL_HANDLE;
  info.DescriptorPool = r->common_set_pool;
  info.Subpass = 0;
  info.MinImageCount = OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT;
  info.ImageCount = r->swapchain_images_count;
  info.MSAASamples = r->msaa_sample_count;
  info.Allocator = nullptr;
  info.CheckVkResultFn = nullptr;
  ImGui_ImplVulkan_Init(&info, r->main_render_pass);

  if (OWL_SUCCESS != (code = owl_renderer_immidiate_command_buffer_init(r))) {
    OWL_ASSERT(0);
    return code;
  }

  if (OWL_SUCCESS != (code = owl_renderer_immidiate_command_buffer_begin(r))) {
    OWL_ASSERT(0);
    return code;
  }

  ImGui::GetIO().Fonts->AddFontFromFileTTF(
      "../../assets/Inconsolata-Regular.ttf", 16.0F);

  ImGui_ImplVulkan_CreateFontsTexture(r->immidiate_command_buffer);

  if (OWL_SUCCESS != (code = owl_renderer_immidiate_command_buffer_end(r))) {
    OWL_ASSERT(0);
    return code;
  }

  if (OWL_SUCCESS != (code = owl_renderer_immidiate_command_buffer_submit(r))) {
    OWL_ASSERT(0);
    return code;
  }

  owl_renderer_immidiate_command_buffer_deinit(r);

  ImGui_ImplVulkan_DestroyFontUploadObjects();

  return code;
}

void owl_imgui_deinit(struct owl_renderer *r, struct owl_imgui *im) {
  OWL_UNUSED(r);
  OWL_UNUSED(im);

  vkDeviceWaitIdle(r->device);

  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
}

void owl_imgui_begin_frame(struct owl_imgui *im) {
  OWL_UNUSED(im);

  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
}

void owl_imgui_end_frame(struct owl_imgui *im) { OWL_UNUSED(im); }

void owl_imgui_begin(struct owl_imgui *im, char const *title,
                     owl_imgui_window_flag_bits bits) {
  OWL_UNUSED(im);

  ImGui::Begin(title, NULL, bits);
}

void owl_imgui_end(struct owl_imgui *im) {
  OWL_UNUSED(im);

  ImGui::End();
}

void owl_imgui_text(struct owl_imgui *im, char const *fmt, ...) {

  OWL_UNUSED(im);

  va_list args;
  va_start(args, fmt);

  OWL_LOCAL_PERSIST char buffer[256];
  vsnprintf(buffer, sizeof(buffer), fmt, args);

  ImGui::Text("%s", buffer);

  va_end(args);
}

void owl_imgui_checkbox(struct owl_imgui *im, char const *text, int *state) {
  OWL_UNUSED(im);

  auto value = false;
  ImGui::Checkbox(text, &value);
  *state = static_cast<int>(value);
}

void owl_imgui_slider_float(struct owl_imgui *im, char const *text,
                            float *state, float begin, float end) {
  OWL_UNUSED(im);

  ImGui::SliderFloat(text, state, begin, end);
}

void owl_imgui_color_edit_v3(struct owl_imgui *im, char const *text,
                             owl_v3 color) {
  OWL_UNUSED(im);

  ImGui::ColorEdit3(text, color);
}

int owl_imgui_button(struct owl_imgui *im, char const *text) {
  OWL_UNUSED(im);

  return static_cast<int>(ImGui::Button(text));
}

void owl_imgui_same_line(struct owl_imgui *im) {
  OWL_UNUSED(im);

  ImGui::SameLine();
}

void owl_imgui_render(struct owl_imgui *im, struct owl_renderer *r) {
  OWL_UNUSED(im);

  ImGui::Render();

  auto *data = ImGui::GetDrawData();
  ImGui_ImplVulkan_RenderDrawData(data, r->active_frame_command_buffer);
}

float owl_imgui_framerate(struct owl_imgui *im) {
  OWL_UNUSED(im);

  return ImGui::GetIO().Framerate;
}

void owl_imgui_demo_window(struct owl_imgui *im) {
  OWL_UNUSED(im);

  ImGui::ShowDemoWindow();
}

void owl_imgui_set_next_window_position(struct owl_imgui *im, owl_v2 pos,
                                        owl_imgui_cond_flag_bits cond,
                                        owl_v2 pivot) {
  OWL_UNUSED(im);

  ImGui::SetNextWindowPos(ImVec2(pos[0], pos[1]), cond,
                          ImVec2(pivot[0], pivot[1]));
}

void owl_imgui_set_next_window_size(struct owl_imgui *im, owl_v2 size,
                                    owl_imgui_cond_flag_bits cond) {
  OWL_UNUSED(im);

  ImGui::SetNextWindowSize(ImVec2(size[0], size[1]), cond);
}
