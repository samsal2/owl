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

INC = -I$(VULKANINC) -I$(GLFWINC) -I$(BUILDDIR)

LIB = -rpath $(GLFWLIB) -L$(GLFWLIB) -lglfw3      \
      -framework Cocoa -framework IOKit -lm       \
      -rpath $(VULKANLIB) -L$(VULKANLIB) -lvulkan

DEF = -DOWL_ENABLE_VALIDATION

WARN = -Wall -Wextra -Wshadow -Werror -pedantic -pedantic-errors

OWLCFLAGS = $(WARN) $(INC) $(DEF) $(CFLAGS)
OWLLDFLAGS = $(LIB) $(LDFLAGS)
 
OWLCXXFLAGS = -std=c++11 $(INC) $(DEF) $(CXXFLAGS)

EXSRC = $(wildcard examples/*.c)
EXOUT = $(EXSRC:examples/%.c=$(BUILDDIR)/%.out) 

CSRC = $(wildcard $(SRCDIR)/*.c)
COBJ = $(CSRC:$(SRCDIR)/%.c=$(BUILDDIR)/%.o) 
CDEP = $(CSRC:$(SRCDIR)/%.c=$(BUILDDIR)/%.d)

CXXSRC = $(wildcard $(SRCDIR)/*.cpp)
CXXOBJ = $(CXXSRC:$(SRCDIR)/%.cpp=$(BUILDDIR)/%.o) 
CXXDEP = $(CXXSRC:$(SRCDIR)/%.cpp=$(BUILDDIR)/%.d)

GLSLVSRC = $(wildcard $(SRCDIR)/*.vert)
GLSLVSPV = $(GLSLVSRC:$(SRCDIR)/%.vert=$(BUILDDIR)/%.vert.spv.u32)

GLSLFSRC = $(wildcard $(SRCDIR)/*.frag)
GLSLFSPV = $(GLSLFSRC:$(SRCDIR)/%.frag=$(BUILDDIR)/%.frag.spv.u32)

all: examples library shaders

.PHONY: examples
examples: $(EXOUT)

$(EXOUT): $(BUILDDIR)/libowl.a

$(BUILDDIR)/%.out: examples/%.c | $(BUILDDIR)
	$(CC) -I. $(OWLCFLAGS) -o $@ $< $(OWLLDFLAGS) -L$(BUILDDIR) -lowl -lstdc++

.PHONY: library
-include $(CDEP)
-include $(CXXDEP)
library: $(BUILDDIR)/libowl.a

$(BUILDDIR)/libowl.a: $(COBJ) $(CXXOBJ)
	$(AR) -cqsv $@ $^

$(COBJ): $(GLSLVSPV) $(GLSLFSPV)

$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) -MMD $(OWLCFLAGS) -o $@ -c $<

$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp | $(BUILDDIR)
	$(CXX) -MMD $(OWLCXXFLAGS) -o $@ -c $<

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

