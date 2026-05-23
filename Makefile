.PHONY: test clean all_shaders build_mac build_linux build_win

# Base build procedure #

BASECFLAGS = -std=c++17 -O0
BASELDFLAGS = -lglfw -lvulkan

BASE_OUTPUT ?= lepton2_main

GLSLC ?= glslc

SOURCES = $(wildcard src/*.cpp src/*/*.cpp src/*/*/*.cpp)
OBJECTS = $(subst src,build/o,$(SOURCES:.cpp=.o))

SHADER_SOURCES = $(wildcard shaders_src/*.comp shaders_src/*/*.comp shaders_src/*/*/*.comp shaders_src/*.frag shaders_src/*/*.frag shaders_src/*/*/*.frag shaders_src/*.vert shaders_src/*/*.vert shaders_src/*/*/*.vert)
SHADER_SPIRV = $(subst shaders_src,build/output/shaders,$(addsuffix .spv,$(SHADER_SOURCES)))

build_base: $(OBJECTS)
	mkdir -p $(shell dirname "$(OUTPUT)")
	clang++ $(CFLAGS) -o $(OUTPUT) $(OBJECTS) $(LDFLAGS)

$(OBJECTS): build/o/%.o: src/%.cpp
	@echo Compiling $< to $@...
	mkdir -p $(shell dirname "$@")
	clang++ $(CFLAGS) $< -c -o $@

$(SHADER_SPIRV): build/output/shaders/%.spv: shaders_src/%
	@echo Compiling $< to $@...
	mkdir -p $(shell dirname "$@")
	$(GLSLC) $< -o $@

all_shaders: $(SHADER_SPIRV)
	@echo All shaders built.

assets: $(SHADER_SPIRV)
	mkdir -p build/output/
	cp -r assets build/output/

clean:
	rm -rf build

clean_aux:
	mkdir -p build/o/lepton2/vulkancore
	mkdir -p rmtemp
	mv build/* rmtemp/
	mkdir -p build/o/lepton2
	mv rmtemp/o/lepton2/* build/o/lepton2/
	rm -rf rmtemp

clean_resources:
	rm -rf build/output/assets
	rm -rf build/output/shaders

# Test native build #

test: CFLAGS = $(BASECFLAGS) -D DEBUG_ENV -g
test: LDFLAGS = $(BASELDFLAGS)
test: OUTPUT = build/output/$(BASE_OUTPUT)
test: build_base assets
	@echo Launching $(OUTPUT)...
	@./$(OUTPUT) $(TESTARGS)

# Mac OSX build #
# FIXME: Only works on arm64-based cpu for now, need to update sysroot and unify binaries.

build_mac_extract_sysroot:
	mkdir -p build
	tar -xf build-resources/osx_sysroot.tar.gz -C build/

build_mac: CFLAGS = $(BASECFLAGS) -target arm64-apple-macos13 --sysroot build/osx_sysroot -stdlib=libc++ -mmacosx-version-min=13.0
build_mac: LDFLAGS = $(BASELDFLAGS) -fuse-ld=lld -Lbuild-resources -Wl,-rpath,.
build_mac: OUTPUT = build/output/$(BASE_OUTPUT)
build_mac: clean build_mac_extract_sysroot build_base assets
	mkdir -p build/output/vulkan/icd.d
	cp build-resources/MoltenVK_icd.json build/output/vulkan/icd.d/
	cp build-resources/libMoltenVK.dylib build/output/libMoltenVK.dylib
	cp build-resources/libvulkan.dylib build/output/libvulkan.1.dylib
	@echo "Build completed in build/output/."

# Linux build #

build_linux: CFLAGS = $(BASECFLAGS)
build_linux: LDFLAGS = $(BASELDFLAGS)
build_linux: OUTPUT = build/output/$(BASE_OUTPUT)
build_linux: clean build_base assets
	@echo "Build completed in build/output/."

# Windows build #

WIN_TARGET = x86_64-w64-mingw32
WIN_SYSROOT = /usr/$(WIN_TARGET)
WIN_GCCDIR_ROOT = /usr/lib/gcc/$(WIN_TARGET)
WIN_GCCDIR = $(WIN_GCCDIR_ROOT)/$(shell ls $(WIN_GCCDIR_ROOT) | grep win | head -n 1)

build_win_extract_sysroot:
	mkdir -p build
	tar -xf build-resources/win_vulkan_sdk.tar.gz -C build/

build_win: CFLAGS = $(BASECFLAGS) -target $(WIN_TARGET) -Ibuild/win_vulkan_sdk/Include -I$(WIN_GCCDIR)/include/c++ -I$(WIN_GCCDIR)/include/c++/$(WIN_TARGET) --sysroot=$(WIN_SYSROOT)
build_win: LDFLAGS = -Lbuild/win_vulkan_sdk/Lib -L$(WIN_GCCDIR) -lvulkan-1 -lglfw3 -lgdi32 -lkernel32 -luser32 -lshell32 -static -static-libgcc -static-libstdc++
build_win: OUTPUT = build/output/$(BASE_OUTPUT).exe
build_win: clean build_win_extract_sysroot build_base assets
	@echo "Build completed in build/output/."


clean_documentation:
	rm -rf docs/docs_root/doxygen
	rm -rf docs/doxygen_xml
	rm -rf docs/site
	rm -f docs/mkdocs.yml

documentation: clean_documentation
	doxygen docs/Doxyfile
	moxygen docs/doxygen_xml -c -H -o docs/docs_root/doxygen/%s.md
	python3 docs/process_doxygen.py
	mkdocs build -f docs/mkdocs.yml