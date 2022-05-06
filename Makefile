VERSION = 0.0.1

# config

SRCDIR = owl
BUILDDIR = build

GENDIR = $(BUILDDIR)/gen
LIBDIR = $(BUILDDIR)/lib
BINDIR = $(BUILDDIR)/bin
OBJDIR = $(BUILDDIR)/obj

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

INC = -I$(VULKANINC) -I$(GLFWINC) -I$(GENDIR)

LIB = -rpath $(GLFWLIB) -L$(GLFWLIB) -lglfw3      \
      -framework Cocoa -framework IOKit -lm       \
      -rpath $(VULKANLIB) -L$(VULKANLIB) -lvulkan

DEF = 

WARN = -Wall -Wextra -Wshadow -Werror -pedantic -pedantic-errors

OWLCFLAGS = $(WARN) $(INC) $(DEF) $(CFLAGS)
OWLLDFLAGS = $(LIB) $(LDFLAGS)
 
OWLCXXFLAGS = -std=c++11 $(INC) $(DEF) $(CXXFLAGS)

# build

EXSRC = $(wildcard examples/*.c)
EXOUT = $(EXSRC:examples/%.c=$(BINDIR)/%) 

CSRC = $(wildcard $(SRCDIR)/*.c)
COBJ = $(CSRC:$(SRCDIR)/%.c=$(OBJDIR)/%.o) 
CDEP = $(CSRC:$(SRCDIR)/%.c=$(OBJDIR)/%.d)

CXXSRC = $(wildcard $(SRCDIR)/*.cpp)
CXXOBJ = $(CXXSRC:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o) 
CXXDEP = $(CXXSRC:$(SRCDIR)/%.cpp=$(OBJDIR)/%.d)

GLSLVSRC = $(wildcard $(SRCDIR)/*.vert)
GLSLVSPV = $(GLSLVSRC:$(SRCDIR)/%.vert=$(GENDIR)/%.vert.spv.u32)

GLSLFSRC = $(wildcard $(SRCDIR)/*.frag)
GLSLFSPV = $(GLSLFSRC:$(SRCDIR)/%.frag=$(GENDIR)/%.frag.spv.u32)

all: examples library shaders

.PHONY: examples
examples: $(EXOUT)

$(EXOUT): $(LIBDIR)/libowl.a

$(BINDIR)/%: examples/%.c | $(BINDIR)
	$(CC) -I. $(OWLCFLAGS) -o $@ $< $(OWLLDFLAGS) -L$(LIBDIR) -lowl -lstdc++

.PHONY: library
-include $(CDEP)
-include $(CXXDEP)
library: $(LIBDIR)/libowl.a

$(LIBDIR)/libowl.a: $(COBJ) $(CXXOBJ) | $(LIBDIR)
	$(AR) -cqsv $@ $^

$(COBJ): $(GLSLVSPV) $(GLSLFSPV)

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) -MMD $(OWLCFLAGS) -o $@ -c $<

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp | $(OBJDIR)
	$(CXX) -MMD $(OWLCXXFLAGS) -o $@ -c $<

.PHONY: shaders
shaders: $(GLSLVSPV) $(GLSLFSPV)

$(GENDIR)/%.vert.spv.u32: $(SRCDIR)/%.vert | $(GENDIR)
	$(GLSLANG) -V -x -o $@ $<

$(GENDIR)/%.frag.spv.u32: $(SRCDIR)/%.frag | $(GENDIR)
	$(GLSLANG) -V -x -o $@ $<

$(GENDIR): | $(BUILDDIR)
	$(MKDIR) $(GENDIR)

$(LIBDIR): | $(BUILDDIR)
	$(MKDIR) $(LIBDIR)

$(BINDIR): | $(BUILDDIR)
	$(MKDIR) $(BINDIR)

$(OBJDIR): | $(BUILDDIR)
	$(MKDIR) $(OBJDIR)

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

