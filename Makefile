.PHONY: test clean all_shaders build_mac build_linux

# Base build procedure #

BASECFLAGS = -std=c++17 -g
BASELDFLAGS = -lglfw -lvulkan

SOURCES = $(wildcard */*.cpp) $(wildcard */*/*.cpp) $(wildcard */*/*/*.cpp)
OBJECTS = $(subst src,build/o,$(SOURCES:.cpp=.o))

SHADER_SOURCES = $(wildcard shaders_src/*.frag) $(wildcard shaders_src/*.vert) $(wildcard shaders_src/*/*.frag) $(wildcard shaders_src/*/*.vert)
SHADER_SPIRV = $(subst shaders_src,build/output/shaders,$(addsuffix .spv,$(SHADER_SOURCES)))

build-base: $(OBJECTS)
	mkdir -p $(shell dirname "$(OUTPUT)")
	clang++ $(CFLAGS) -o $(OUTPUT) $(OBJECTS) $(LDFLAGS)

$(OBJECTS): build/o/%.o: src/%.cpp
	@echo Compiling $< to $@...
	mkdir -p $(shell dirname "$@")
	clang++ $(CFLAGS) $< -c -o $@

$(SHADER_SPIRV): build/output/shaders/%.spv: shaders_src/%
	@echo Compiling $< to $@...
	mkdir -p $(shell dirname "$@")
	glslc $< -o $@

all_shaders: $(SHADER_SPIRV)
	@echo All shaders built.

clean:
	rm -rf build

# Test native build #

test: CFLAGS = $(BASECFLAGS) -D DEBUG_ENV
test: LDFLAGS = $(BASELDFLAGS)
test: OUTPUT = build/output/lepton2_main
test: clean build-base $(SHADER_SPIRV)
	@echo Launching $(OUTPUT)...
	@./$(OUTPUT)

# Mac OSX build #
# FIXME: Only works on arm64-based cpu for now, need to update sysroot and unify binaries.

build_mac_extract_sysroot:
	mkdir -p build
	tar -xf build-resources/osx_sysroot.tar.gz -C build/

build_mac: CFLAGS = $(BASECFLAGS) -target arm64-apple-macos13 --sysroot build/osx_sysroot -stdlib=libc++ -mmacosx-version-min=13.0
build_mac: LDFLAGS = $(BASELDFLAGS) -fuse-ld=lld -Lbuild-resources -Wl,-rpath,.
build_mac: OUTPUT = build/output/lepton2_main
build_mac: clean build_mac_extract_sysroot build-base $(SHADER_SPIRV)
	mkdir -p build/output/vulkan/icd.d
	cp build-resources/MoltenVK_icd.json build/output/vulkan/icd.d/
	cp build-resources/libMoltenVK.dylib build/output/libMoltenVK.dylib
	cp build-resources/libvulkan.dylib build/output/libvulkan.1.dylib
	@echo "Build completed in build/output/."

# Linux build #

build_linux: CFLAGS = $(BASECFLAGS)
build_linux: LDFLAGS = $(BASELDFLAGS)
build_linux: OUTPUT = build/output/lepton2_main
build_linux: clean build-base $(SHADER_SPIRV)
	@echo "Build completed in build/output/."