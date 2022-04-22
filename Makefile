RMF     = rm -f
RMRF    = rm -rf
GLSLANG = $(VULKAN_SDK)/bin/glslangValidator

LIBRARY = libowl.a

SRCDIR  = owl

INCS    = -Ilibs/glfw/macos/include \
          -I$(VULKAN_SDK)/include        

DEFS    = -DOWL_ENABLE_VALIDATION

CFLAGS  = -std=c99                       \
          -O0                            \
          -g                             \
          -Wall                          \
          -Wextra                        \
          -Wshadow                       \
          -Wvla                          \
          -Walloca                       \
          -pedantic                      \
          -pedantic-errors               \
          -fstrict-aliasing              \
          -fsanitize=address             \
          -fsanitize=undefined           \
          -fsanitize=bounds              \
          $(INCS)                        \
          $(DEFS)

LIBS    = -Llibs/glfw/macos/lib-x86_64      \
          -rpath libs/glfw/macos/lib-x86_64 \
          -lglfw3                           \
          -L$(VULKAN_SDK)/lib               \
          -rpath $(VULKAN_SDK)/lib          \
          -lvulkan                          \
          -lm																\
          -framework Cocoa                  \
          -framework IOKit

LDFLAGS = -fsanitize=address                    \
          -fsanitize=undefined                  \
          -fsanitize=bounds                     \
          $(LIBS)

SRCS = $(wildcard $(SRCDIR)/*.c)
OBJS = $(SRCS:.c=.o) 
DEPS = $(SRCS:.c=.d)

GLSLVSRC = $(wildcard $(SRCDIR)/*.vert)
GLSLVSPV = $(GLSLVSRC:.vert=.vert.spv.u32)

GLSLFSRC = $(wildcard $(SRCDIR)/*.frag)
GLSLFSPV = $(GLSLFSRC:.frag=.frag.spv.u32)

EXSRCS = $(wildcard examples/*.c)
EXOUTS = $(EXSRCS:.c=.out) 

all: examples library shaders

.PHONY: library
-include $(DEPS)

library: $(LIBRARY)

$(LIBRARY): $(OBJS)
	$(AR) -cqsv $@ $^

$(OBJS): $(GLSLVSPV) $(GLSLFSPV)

%.o: %.c
	$(CC) -MMD $(CFLAGS) -o $@ -c $<

.PHONY: examples
examples: $(EXOUTS)

$(EXOUTS): $(LIBRARY)

%.out: %.c
	$(CC) -I. $(CFLAGS) -o $@ $< $(LDFLAGS) -L. -lowl

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
	$(RMF) $(DEPS)
	$(RMF) $(LIBRARY)
	$(RMF) $(EXOUTS)

