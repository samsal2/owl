CC								= clang
GLSLANG_VALIDATOR = glslangValidator

PREFIX							= /usr/local/
PROJECT_NAME				= owl
MKDIRP_CMD					= mkdir -p
RMF_CMD							= rm -f
RMRF_CMD						= rm -rf

LIBRARY							= libowl.a

CFLAGS							=																													\
	-std=c99																																		\
	-Ofast	  																																	\
	-Wall																																				\
	-Werror																																			\
	-Wextra																																			\
	-pedantic																																		\
	-pedantic-errors																														\
	-flto																														            \
  -Ilibraries/glfw/macos/include																							\
	-Ilibraries/vulkan/macos/include

LDFLAGS							=																													\
	-Llibraries/glfw/macos/lib-universal																				\
	-lglfw3																																			\
  -Llibraries/vulkan/macos/lib																								\
	-lvulkan																																		\
	-framework Cocoa																														\
	-framework IOKit

glsl_vert_src = $(wildcard *.vert)
glsl_vert_spv_u32 = $(glsl_vert_src:.vert=.vert.spv.u32)

glsl_frag_src = $(wildcard *.frag)
glsl_frag_spv_u32 = $(glsl_frag_src:.frag=.frag.spv.u32)

c_src = $(wildcard *.c)
c_obj = $(c_src:.c=.o)

c_examples_src = $(wildcard examples/*.c)
c_examples_out = $(c_examples_src:.c=.out)

all: shaders library examples

.PHONY: shaders
shaders: $(glsl_vert_spv_u32) $(glsl_frag_spv_u32)

.PHONY: library
library: shaders $(LIBRARY)

.PHONY: examples
examples: $(c_examples_out)

$(LIBRARY): $(c_obj)
	$(AR) -cqsv $@ $^

%.vert.spv.u32: %.vert
	$(GLSLANG_VALIDATOR) -V -x -o $@ $<

%.frag.spv.u32: %.frag
	$(GLSLANG_VALIDATOR) -V -x -o $@ $<

%.d: %.c
	$(CC) $(CFLAGS) $@ -c $<

%.out: %.c library
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) -L. -l$(PROJECT_NAME) -I.

.PHONY: clean
clean:
	$(RMF_CMD) $(glsl_frag_spv_u32)
	$(RMF_CMD) $(glsl_vert_spv_u32)
	$(RMF_CMD) $(c_obj)
	$(RMF_CMD) $(LIBRARY)
	$(RMF_CMD) $(c_examples_out)

