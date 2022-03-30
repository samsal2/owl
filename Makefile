CC = clang
GLSLANG_VALIDATOR = glslangValidator

CFLAGS = -std=c99
CFLAGS += -O0
CFLAGS += -g
CFLAGS += -Ilibraries/glfw/macos/include
CFLAGS += -I$(VULKAN_SDK)/include
CFLAGS += -fstrict-aliasing							
CFLAGS += -fsanitize=address
CFLAGS += -fsanitize=undefined

CFLAGS += -Wall
CFLAGS += -Werror
CFLAGS += -Wextra
CFLAGS += -Wshadow
CFLAGS += -pedantic
CFLAGS += -pedantic-errors

LDFLAGS =-Llibraries/glfw/macos/lib-universal
LDFLAGS +=-lglfw3

LDFLAGS +=-L$(VULKAN_SDK)/lib
LDFLAGS +=-lvulkan

LDFLAGS +=-framework Cocoa
LDFLAGS +=-framework IOKit
LDFLAGS +=-fsanitize=address
LDFLAGS +=-fsanitize=undefined

RMF_CMD = rm -f

PREFIX = /usr/local/
PROJECT_NAME = owl

LIBRARY	= libowl.a

GLSL_VERT_SRC = $(wildcard *.vert)
GLSL_VERT_SPV_U32 = $(GLSL_VERT_SRC:.vert=.vert.spv.u32)

GLSL_FRAG_SRC = $(wildcard *.frag)
GLSL_FRAG_SPV_U32 = $(GLSL_FRAG_SRC:.frag=.frag.spv.u32)

SRCS = $(wildcard *.c)
OBJS = $(SRCS:.c=.o)

EXAMPLE_SRCS = $(wildcard examples/*.c)
EXAMPLE_OUTS = $(EXAMPLE_SRCS:.c=.out)

%.vert.spv.u32: %.vert
	$(GLSLANG_VALIDATOR) -V -x -o $@ $<

%.frag.spv.u32: %.frag
	$(GLSLANG_VALIDATOR) -V -x -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

%.out: %.c 
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) -L. -l$(PROJECT_NAME) -I.

$(EXAMPLE_OUTS): $(LIBRARY)

$(LIBRARY): $(OBJS)
	$(AR) -cqsv $@ $^

$(OBJS): $(GLSL_VERT_SPV_U32) $(GLSL_FRAG_SPV_U32)

.PHONY: clean
clean: 
	$(RMF_CMD) $(GLSL_FRAG_SPV_U32)
	$(RMF_CMD) $(GLSL_VERT_SPV_U32)
	$(RMF_CMD) $(OBJS)
	$(RMF_CMD) $(LIBRARY)
	$(RMF_CMD) $(EXAMPLE_OUTS)

