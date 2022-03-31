CC = clang
RMF = rm -f
GLSLANG = glslangValidator

LIBRARY = libowl.a

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
CFLAGS += -Wvla
CFLAGS += -Wstrict-prototypes
CFLAGS += -pedantic
CFLAGS += -pedantic-errors
CFLAGS += -DOWL_ENABLE_VALIDATION

LDFLAGS =-Llibraries/glfw/macos/lib-universal
LDFLAGS +=-lglfw3
LDFLAGS +=-framework Cocoa
LDFLAGS +=-framework IOKit
LDFLAGS +=-L$(VULKAN_SDK)/lib
LDFLAGS +=-lvulkan
LDFLAGS +=-fsanitize=address
LDFLAGS +=-fsanitize=undefined

GLSLVSRC = $(wildcard *.vert)
GLSLVSPV = $(GLSLVSRC:.vert=.vert.spv.u32)

GLSLFSRC = $(wildcard *.frag)
GLSLFSPV = $(GLSLFSRC:.frag=.frag.spv.u32)

SRCS = $(wildcard *.c)
OBJS = $(SRCS:.c=.o)
DEPS = $(SRCS:.c=.d)

EXSRCS = $(wildcard examples/*.c)
EXOUTS = $(EXSRCS:.c=.out)

all: examples library

.PHONY: examples
examples: $(EXOUTS)

$(EXOUTS): $(LIBRARY)

%.out: %.c $(LIBRARY)
	$(CC) $(CFLAGS) -I. -o $@ $< $(LDFLAGS) -L. -lowl

.PHONY: library
-include $(DEPS)

library: $(LIBRARY)

$(LIBRARY): $(OBJS)
	$(AR) -cqsv $@ $^

$(OBJS): $(GLSLVSPV) $(GLSLFSPV)

%.o: %.c
	$(CC) -MMD $(CFLAGS) -o $@ -c $<

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

