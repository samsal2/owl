.POSIX:
.SUFFIXES:

TYPE = Release
CMAKEOPTS = -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

CMAKE = cmake
NINJA = ninja
MKDIR = mkdir

BUILDDIR = build

all: ceceo

.PHONY: ceceo
ceceo: $(BUILDDIR)/build.ninja
	$(NINJA) -C$(BUILDDIR)

$(BUILDDIR)/build.ninja: $(BUILDDIR) Makefile
	$(CMAKE) -B$(BUILDDIR) -DCMAKE_BUILD_TYPE=$(TYPE) $(CMAKEOPTS) -GNinja .

$(BUILDDIR):
	$(MKDIR) $(BUILDDIR)

.PHONY: modules
modules:
	git submodule update --init --recursive

.PHONY: format
format:
	clang-format -i owl/owl_*

.PHONY: clean
clean:
	$(NINJA) -C$(BUILDDIR) clean
  
