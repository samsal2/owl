CC = clang
GLSLANG_VALIDATOR = glslangValidator

PREFIX = /usr/local/
PROJECT_NAME = owl

LIBRARY	= libowl.a

CFLAGS = -std=c99
CFLAGS += -O0
CFLAGS += -g
CFLAGS += -Wall
CFLAGS += -Werror
CFLAGS += -Wextra
CFLAGS += -Wshadow
CFLAGS += -pedantic
CFLAGS += -pedantic-errors
CFLAGS += -Ilibraries/glfw/macos/include
CFLAGS += -I$(VULKAN_SDK)/include
CFLAGS += -fstrict-aliasing							
CFLAGS += -fsanitize=address
CFLAGS += -fsanitize=undefined

LDFLAGS =-Llibraries/glfw/macos/lib-universal
LDFLAGS +=-lglfw3
LDFLAGS +=-L$(VULKAN_SDK)/lib
LDFLAGS +=-lvulkan
LDFLAGS +=-framework Cocoa
LDFLAGS +=-framework IOKit
LDFLAGS +=-fsanitize=address
LDFLAGS +=-fsanitize=undefined

glsl_vert_src = $(wildcard *.vert)
glsl_vert_spv_u32 = $(glsl_vert_src:.vert=.vert.spv.u32)

glsl_frag_src = $(wildcard *.frag)
glsl_frag_spv_u32 = $(glsl_frag_src:.frag=.frag.spv.u32)

c_src = $(wildcard *.c)
c_obj = $(c_src:.c=.o)

c_examples_src = $(wildcard examples/*.c)
c_examples_out = $(c_examples_src:.c=.out)

all: shaders library examples

.PHONY: examples
examples: $(c_examples_out)

.PHONY: library
library: $(LIBRARY)

.PHONY: shaders
shaders: $(glsl_vert_spv_u32) $(glsl_frag_spv_u32)

$(LIBRARY): $(c_obj)
	$(AR) -cqsv $@ $^

%.vert.spv.u32: %.vert
	$(GLSLANG_VALIDATOR) -V -x -o $@ $<

%.frag.spv.u32: %.frag
	$(GLSLANG_VALIDATOR) -V -x -o $@ $<

%.o: %.c $(glsl_vert_spv_u32) $(glsl_frag_spv_u32)
	$(CC) $(CFLAGS) -o $@ -c $<

%.out: %.c $(LIBRARY)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) -L. -l$(PROJECT_NAME) -I.

.PHONY: clean_shaders
clean_shaders:
	$(RMF_CMD) $(glsl_frag_spv_u32)
	$(RMF_CMD) $(glsl_vert_spv_u32)

.PHONY: clean_examples
clean_examples:
	$(RMF_CMD) $(c_examples_out)

.PHONY: clean_library
clean_library:
	$(RMF_CMD) $(c_obj)
	$(RMF_CMD) $(LIBRARY)

.PHONY: clean
clean: clean_library clean_examples clean_shaders

