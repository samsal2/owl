#include "owl_ui.h"

#include "owl_renderer.h"
#include "owl_vector_math.h"
#include "owl_model.h"

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"
#include "cimgui_impl.h"

void owl_ui_begin(struct owl_renderer *r) {
  OWL_UNUSED(r);

  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  igNewFrame();
}

void owl_ui_end(struct owl_renderer *r) {
  igRender();
  igEndFrame();

  ImGui_ImplVulkan_RenderDrawData(
      igGetDrawData(), r->active_frame_command_buffer, VK_NULL_HANDLE);
}


void owl_ui_renderer_stats_draw(struct owl_ui_renderer_state *rs, struct owl_renderer *r) {
  OWL_UNUSED(rs);


  igBegin("Stats", NULL, 0);
  {
    if (igCollapsingHeader_TreeNodeFlags("Frame", 0)) {
      igText("Framerate: %.2f", igGetIO()->Framerate);
    }

    if (igCollapsingHeader_TreeNodeFlags("Renderer", 0)) {
      igText("frame_heap_offset: %lu", r->frame_heap_offset);
      igText("frame_heap_buffer_size: %lu", r->frame_heap_buffer_size);
      igText("frame_heap_buffer_alignment: %lu",
             r->frame_heap_buffer_alignment);
      igText("frame_heap_buffer_aligned_size: %lu",
             r->frame_heap_buffer_aligned_size);
    }
  }
  igEnd();
  igShowDemoWindow(NULL);
}

void owl_ui_model_stats_draw(struct owl_ui_model_state *ms, struct owl_model *m) {
  owl_i32 i;

  OWL_UNUSED(m);

  igBegin("Model", NULL, 0);

  if (igCollapsingHeader_TreeNodeFlags("Textures", 0)) {
    for (i = 0; i < ms->ui_sets_count; ++i) {
      ImVec2 sz;
      ImVec2 uv0;
      ImVec2 uv1;
      ImVec4 tint;
      ImVec4 border;
   
      sz.x = 200.0F;
      sz.y = sz.x;

      uv0.x = 0.0F;
      uv0.y = 0.0F;

      uv1.x = 1.0F;
      uv1.y = 1.0F;
    
      tint.x = 1.0F;
      tint.y = 1.0F;
      tint.z = 1.0F;
      tint.w = 1.0F;

      border.x = 1.0F;
      border.y = 1.0F;
      border.z = 1.0F;
      border.w = 1.0F;


      igImage(ms->ui_sets[i], sz, uv0, uv1, tint, border);
      igSeparator();
    }
  }

  igEnd();
}

enum owl_code owl_ui_renderer_state_init(struct owl_renderer *r, struct owl_ui_renderer_state *rs) {
  enum owl_code code = OWL_SUCCESS;  

  OWL_UNUSED(r);
  OWL_UNUSED(rs);

  return code;
}

void owl_ui_renderer_state_deinit(struct owl_renderer *r, struct owl_ui_renderer_state *rs) {
  OWL_UNUSED(r);
  OWL_UNUSED(rs);
}

enum owl_code owl_ui_model_state_init(struct owl_renderer const *r, struct owl_model const *m, struct owl_ui_model_state *ms) {
  owl_i32 i;   
  
  enum owl_code code = OWL_SUCCESS;  

  for (i = 0; i < m->images_count; ++i) {
    owl_i32 const slot = m->images[i].image.slot;  
    
    ms->ui_sets[i] = ImGui_ImplVulkan_AddTexture(r->image_pool_samplers[slot], 
                                r->image_pool_image_views[slot], 
                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  }

  ms->ui_sets_count = m->images_count;

  return code;
}


void owl_ui_model_state_deinit(struct owl_renderer const *r, struct owl_model const *m, struct owl_ui_model_state *ms) {
  OWL_UNUSED(r);
  OWL_UNUSED(m);
  OWL_UNUSED(ms);
}
