.POSIX:

PREFIX				= /usr/local/
PROJECT_NAME	= owl
MKDIRP_CMD		= mkdir -p
RMF_CMD				= rm -f
RMRF_CMD			= rm -rf
SOURCE_DIR		= $(PROJECT_NAME)
INCLUDE_DIR		= $(PROJECT_NAME)
OUTPUT_DIR		= bin
OBJECT_DIR		= object

LIBRARY = libowl.a

CFLAGS  = -std=c99 -O3 -Wall -Werror -Wextra -pedantic -pedantic-errors -DNDEBUG

LDFLAGS = -Llibraries/glfw/macos/lib-x86_64 -lglfw3                        \
          -Llibraries/vulkan/macos/lib -lvulkan                               \
          -framework Cocoa -framework IOKit

INCFLAGS =	-Ilibraries/glfw/macos/include -Ilibraries/vulkan/macos/include		\
						-I$(INCLUDE_DIR) -Iexternal/stb -Iexternal/cgltf

all: $(OUTPUT_DIR)/libowl.a $(OUTPUT_DIR)/model

$(OUTPUT_DIR)/model: examples/model.c
	$(CC) $(CFLAGS) $(INCFLAGS) -o $@ $< $(LDFLAGS) -L$(OUTPUT_DIR) -lowl -I.

$(OUTPUT_DIR)/$(LIBRARY): $(OBJECT_DIR)/camera.o $(OBJECT_DIR)/client.o				\
                          $(OBJECT_DIR)/draw_command.o $(OBJECT_DIR)/font.o		\
                          $(OBJECT_DIR)/internal.o $(OBJECT_DIR)/model.o			\
                          $(OBJECT_DIR)/renderer.o $(OBJECT_DIR)/vector_math.o\
                          $(OBJECT_DIR)/external/stb/stb_image.o              \
                          $(OBJECT_DIR)/external/cgltf/cgltf.o
	$(AR) -cqsv $@ $^

$(OBJECT_DIR)/camera.o: $(SOURCE_DIR)/camera.c $(INCLUDE_DIR)/camera.h
	$(CC) -c $(CFLAGS) $(INCFLAGS) -o $@ $<

$(OBJECT_DIR)/client.o: $(SOURCE_DIR)/client.c $(INCLUDE_DIR)/client.h
	$(CC) -c $(CFLAGS) $(INCFLAGS) -o $@ $<

$(OBJECT_DIR)/draw_command.o: $(SOURCE_DIR)/draw_command.c                    \
                              $(INCLUDE_DIR)/draw_command.h
	$(CC) -c $(CFLAGS) $(INCFLAGS) -o $@ $<

$(OBJECT_DIR)/font.o: $(SOURCE_DIR)/font.c $(INCLUDE_DIR)/font.h
	$(CC) -c $(CFLAGS) $(INCFLAGS) -o $@ $<

$(OBJECT_DIR)/internal.o: $(SOURCE_DIR)/internal.c $(INCLUDE_DIR)/internal.h
	$(CC) -c $(CFLAGS) $(INCFLAGS) -o $@ $<

$(OBJECT_DIR)/model.o: $(SOURCE_DIR)/model.c $(INCLUDE_DIR)/model.h
	$(CC) -c $(CFLAGS) $(INCFLAGS) -o $@ $<

$(OBJECT_DIR)/renderer.o: $(SOURCE_DIR)/renderer.c $(INCLUDE_DIR)/renderer.h
	$(CC) -c $(CFLAGS) $(INCFLAGS) -o $@ $<

$(OBJECT_DIR)/vector_math.o:  $(SOURCE_DIR)/vector_math.c                     \
                              $(INCLUDE_DIR)/vector_math.h
	$(CC) -c $(CFLAGS) $(INCFLAGS) -o $@ $<

$(OBJECT_DIR)/external/cgltf/cgltf.o: external/cgltf/cgltf/cgltf.c            \
                                      external/cgltf/cgltf/cgltf.h
	$(CC) -c $(CFLAGS) $(INCFLAGS) -o $@ $<

$(OBJECT_DIR)/external/stb/stb_image.o: external/stb/stb/stb_image.c          \
                                        external/stb/stb/stb_image.h
	$(CC) -c $(CFLAGS) $(INCFLAGS) -o $@ $<


directories:
	$(MKDIRP_CMD) $(OUTPUT_DIR)
	$(MKDIRP_CMD) $(OBJECT_DIR)
	$(MKDIRP_CMD) $(OBJECT_DIR)/examples
	$(MKDIRP_CMD) $(OBJECT_DIR)/external/cgltf
	$(MKDIRP_CMD) $(OBJECT_DIR)/external/stb

clean:
	$(RMRF_CMD) $(OUTPUT_DIR)
	$(RMRF_CMD) $(OBJECT_DIR)

