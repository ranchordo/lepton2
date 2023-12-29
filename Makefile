CFLAGS = -std=c++17 -O3
LDFLAGS = -lglfw -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi

SOURCES = $(wildcard */*.cpp) $(wildcard */*/*.cpp) $(wildcard */*/*/*.cpp)
OBJECTS = $(subst src,build,$(SOURCES:.cpp=.o))

.PHONY: test clean

VulkanMain: $(OBJECTS)
	g++ $(CFLAGS) -o VulkanMain $(OBJECTS) $(LDFLAGS)

$(OBJECTS): build/%.o: src/%.cpp
	@echo Compiling $< to $@...
	mkdir -p $(shell dirname "$@")
	g++ $(CFLAGS) $< -c -o $@

test: clean VulkanMain shaders_cmp
	./VulkanMain

shaders_cmp: shaders/shader.vert shaders/shader.frag
	mkdir -p shaders_build
	glslc shaders/shader.vert -o shaders_build/shader.vert.spv
	glslc shaders/shader.frag -o shaders_build/shader.frag.spv

clean:
	rm -f VulkanMain
	rm -rf shaders_build
	rm -rf build