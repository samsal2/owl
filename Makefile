VERSION = 0.0.1

SRCDIR = owl
BUILDDIR = build

MV = mv
RMF = rm -f
RMRF = rm -rf
MKDIR = mkdir
BEAR = bear

VULKANINC = $(VULKAN_SDK)/include
VULKANLIB = $(VULKAN_SDK)/lib
GLSLANG = $(VULKAN_SDK)/bin/glslangValidator

GLFWINC = libs/glfw/macos/include
GLFWLIB = libs/glfw/macos/lib-x86_64/

INCS = -I$(VULKANINC) -I$(GLFWINC) -I$(BUILDDIR)

LIBS = -rpath $(GLFWLIB) -L$(GLFWLIB) -lglfw3      \
       -framework Cocoa -framework IOKit           \
       -rpath $(VULKANLIB) -L$(VULKANLIB) -lvulkan \
       -lm

DEFS = -DOWL_ENABLE_VALIDATION

OWLCFLAGS = -Wall -Wextra -Wshadow -Werror \
            -pedantic -pedantic-errors     \
            $(INCS) $(DEFS) $(CFLAGS)

OWLLDFLAGS = $(LIBS) $(LDFLAGS)
 
EXSRCS = $(wildcard examples/*.c)
EXOUTS = $(EXSRCS:examples/%.c=$(BUILDDIR)/%.out) 

SRCS = $(wildcard $(SRCDIR)/*.c)
OBJS = $(SRCS:$(SRCDIR)/%.c=$(BUILDDIR)/%.o) 
DEPS = $(SRCS:$(SRCDIR)/%.c=$(BUILDDIR)/%.d)

GLSLVSRC = $(wildcard $(SRCDIR)/*.vert)
GLSLVSPV = $(GLSLVSRC:$(SRCDIR)/%.vert=$(BUILDDIR)/%.vert.spv.u32)

GLSLFSRC = $(wildcard $(SRCDIR)/*.frag)
GLSLFSPV = $(GLSLFSRC:$(SRCDIR)/%.frag=$(BUILDDIR)/%.frag.spv.u32)

all: examples library shaders

.PHONY: examples
examples: $(EXOUTS)

$(EXOUTS): $(BUILDDIR)/libowl.a

$(BUILDDIR)/%.out: examples/%.c | $(BUILDDIR)
	$(CC) -I. $(OWLCFLAGS) -o $@ $< $(OWLLDFLAGS) -L$(BUILDDIR) -lowl

.PHONY: library
-include $(DEPS)
library: $(BUILDDIR)/libowl.a

$(BUILDDIR)/libowl.a: $(OBJS)
	$(AR) -cqsv $@ $^

$(OBJS): $(GLSLVSPV) $(GLSLFSPV)

$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) -MMD $(OWLCFLAGS) -o $@ -c $<

.PHONY: shaders
shaders: $(GLSLVSPV) $(GLSLFSPV)

$(BUILDDIR)/%.vert.spv.u32: $(SRCDIR)/%.vert | $(BUILDDIR)
	$(GLSLANG) -V -x -o $@ $<

$(BUILDDIR)/%.frag.spv.u32: $(SRCDIR)/%.frag | $(BUILDDIR)
	$(GLSLANG) -V -x -o $@ $<

$(BUILDDIR):
	$(MKDIR) $(BUILDDIR)

.PHONY: bear
bear: $(BUILDDIR)/compile_commands.json

$(BUILDDIR)/compile_commands.json:
	$(BEAR) -- $(MAKE) -j
	$(MV) compile_commands.json $@

.PHONY: clean
clean:
	$(RMRF) $(BUILDDIR)

