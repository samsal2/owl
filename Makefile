SRCDIR  = owl
OBJDIR  = build
SPVDIR  = owl

CC      = clang
RMF     = rm -f
RMRF    = rm -rf
MKDIRP  = mkdir -p
GLSLANG = glslangValidator

LIBRARY = libowl.a

INCS    = -Ilibraries/glfw/macos/include \
          -I$(VULKAN_SDK)/include        

CFLAGS  = -std=c99                       \
          -O0                            \
          -g                             \
          -fstrict-aliasing              \
          -fsanitize=address             \
          -fsanitize=undefined           \
          -fsanitize=bounds              \
          $(INCS)

LIBS    = -Llibraries/glfw/macos/lib-x86_64      \
          -rpath libraries/glfw/macos/lib-x86_64 \
          -lglfw3                                \
          -L$(VULKAN_SDK)/lib                    \
          -rpath $(VULKAN_SDK)/lib               \
          -lvulkan                               \
          -lm																		 \
          -framework Cocoa                       \
          -framework IOKit	                     \

LDFLAGS = -fsanitize=address                    \
          -fsanitize=undefined                  \
          -fsanitize=bounds                     \
          $(LIBS)

SRCS = $(wildcard $(SRCDIR)/*.c)
OBJS = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SRCS)) 
DEPS = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.d,$(SRCS)) 

GLSLVSRC = $(wildcard $(SRCDIR)/*.vert)
GLSLVSPV = $(GLSLVSRC:.vert=.vert.spv.u32)

GLSLFSRC = $(wildcard $(SRCDIR)/*.frag)
GLSLFSPV = $(GLSLFSRC:.frag=.frag.spv.u32)

EXSRCS = $(wildcard examples/*.c)
EXOUTS = $(patsubst examples/%.c, $(OBJDIR)/%.out, $(EXSRCS)) 

all: examples library shaders

.PHONY: library
-include $(DEPS)

library: $(OBJDIR)/$(LIBRARY)

$(OBJDIR)/$(LIBRARY): $(OBJS)
	$(AR) -cqsv $@ $^

$(OBJS): $(GLSLVSPV) $(GLSLFSPV) | $(OBJDIR)

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) -MMD $(CFLAGS) -o $@ -c $<

$(OBJDIR):
	$(MKDIRP) $(OBJDIR)

.PHONY: examples
examples: $(EXOUTS)

$(EXOUTS): $(OBJDIR)/$(LIBRARY)

$(OBJDIR)/%.out: examples/%.c
	$(CC) $(CFLAGS) -I. -o $@ $< $(LDFLAGS) -L$(OBJDIR) -lowl

.PHONY: shaders
shaders: $(GLSLVSPV) $(GLSLFSPV)

%.vert.spv.u32: %.vert
	$(GLSLANG) -V -x -o $@ $<

%.frag.spv.u32: %.frag
	$(GLSLANG) -V -x -o $@ $<

.PHONY: clean
clean:
	$(RMF) $(GLSLFSPV)
	$(RMF) $(GLSLVSPV)
	$(RMF) $(OBJS)
	$(RMF) $(EXOUTS)
	$(RMRF) $(OBJDIR)

