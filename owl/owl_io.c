#include "owl_io.h"

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"

float owl_io_framerate(void) { return igGetIO()->Framerate; }

float owl_io_delta_time(void) { return igGetIO()->DeltaTime; }
