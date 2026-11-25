NICE := nice -n 10
JOBS := -j4
TEST_JOBS := -j10
TEST_TIMEOUT := --timeout 240
CXX_HOST ?= /opt/homebrew/bin/g++-16
CC_HOST ?= /opt/homebrew/bin/gcc-16
CLANG_TIDY := /opt/homebrew/opt/llvm/bin/clang-tidy
CLANG_FORMAT := clang-format
PYTHON := python3
SWITCH_BASE_IMAGE := switch-ui-devkita64:r30-gcc16.1
SWITCH_IMAGE := switch-ui-devkita64:r30-gcc16.1-ccache
SWITCH_CCACHE := build/.ccache-switch
OUNCE_SDK_DIR ?= $(HOME)/Documents/projects/sdk-ounce
OUNCE_SDK_VERSION := 20261113
OUNCE_IMAGE := switch-ui-devkita64:r30-gcc16.1-ounce$(OUNCE_SDK_VERSION)
OUNCE_CCACHE := build/.ccache-ounce
DRAKO_SOURCES ?= $(HOME)/Documents/projects
DRAKO_IMAGE := $(OUNCE_IMAGE)-drako-d1f0507-b794767
CONSOLE_RUN = docker run --rm -v "$(CURDIR)":/src -w /src -e CCACHE_DIR=/src/$(1) \
	-e CCACHE_BASEDIR=/src -e CCACHE_MAXSIZE=5G \
	-e CCACHE_SLOPPINESS=pch_defines,time_macros,include_file_mtime,include_file_ctime $(2)
SWITCH_RUN = $(call CONSOLE_RUN,$(SWITCH_CCACHE),$(SWITCH_IMAGE))
OUNCE_RUN = $(call CONSOLE_RUN,$(OUNCE_CCACHE),$(OUNCE_IMAGE))
DRAKO_RUN = $(call CONSOLE_RUN,$(OUNCE_CCACHE),$(DRAKO_IMAGE))

HOST_PLATFORM := $(if $(filter Darwin,$(shell uname -s)),macos,linux)
TARGET ?= $(HOST_PLATFORM)
ARISA_VERSION := $(shell sed -n 's/^project.arisa VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt)
PACKAGE_ROOT := build/package
PACKAGE_ARCH = $(if $(filter switch switch2,$(TARGET)),aarch64,$(shell uname -m))
PACKAGE_NAME = arisa-$(ARISA_VERSION)-$(TARGET)-$(PACKAGE_ARCH)
PACKAGE_STAGE = $(PACKAGE_ROOT)/stage/$(TARGET)
PACKAGE_CHECK = $(PACKAGE_ROOT)/check/$(TARGET)
PACKAGE_FRAMES := 5
HOST_STRIP = $(if $(filter macos,$(HOST_PLATFORM)),strip;-S,strip;--strip-debug)
HOST_INSPECT = $(if $(filter macos,$(HOST_PLATFORM)),otool -l,objdump -h)
SWITCH_BIN := /opt/devkitpro/devkitA64/bin
HOST_LAUNCHER := $(shell command -v ccache 2>/dev/null)
HOST_PACKAGE_FLAGS = -DCMAKE_C_COMPILER=$(CC_HOST) -DCMAKE_CXX_COMPILER=$(CXX_HOST) \
	$(if $(HOST_LAUNCHER),-DCMAKE_C_COMPILER_LAUNCHER=$(HOST_LAUNCHER) -DCMAKE_CXX_COMPILER_LAUNCHER=$(HOST_LAUNCHER))
VULKAN_LOADER := $(firstword $(wildcard /opt/homebrew/lib/libvulkan.1.dylib))
PACKAGE_OFFSCREEN ?= $(if $(VULKAN_LOADER),1,0)

FORMAT_FILES = $(shell find include src tests switch examples -path tests/lib -prune -o -type f \( -name '*.hpp' -o -name '*.cpp' \) -print)
TIDY_SOURCES = $(shell find src examples -path src/provider/horizon -prune -o \( -name 'deko3d_*' -o -name 'ounce_*' -o -name 'switch_main.cpp' \) -prune -o -type f -name '*.cpp' -print)
GCC_INCLUDE_DIRS = $(shell $(CXX_HOST) -E -x c++ -v /dev/null -o /dev/null 2>&1 | sed -n '/<\.\.\.> search starts here:/,/^End of search list./p' | sed -e '1d' -e '$$d' -e 's/^ *//' -e 's/ (framework directory)$$//' | grep '/include/c++/')
TIDY_ARGS = --quiet -p build/host-debug --extra-arg=-nostdinc++ --extra-arg=-Wno-unknown-warning-option --extra-arg=-Wno-ignored-gch $(foreach dir,$(GCC_INCLUDE_DIRS),--extra-arg=-isystem$(dir))

CHANGED_FILES = $(shell { git diff --name-only HEAD; git ls-files --others --exclude-standard; } 2>/dev/null | sort -u)
TIDY_CHANGED = $(if $(filter .clang-tidy include/%,$(CHANGED_FILES)),$(TIDY_SOURCES),$(filter $(TIDY_SOURCES),$(CHANGED_FILES)))
ASAN_ENV = ASAN_OPTIONS=detect_leaks=0:abort_on_error=1 UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1
TSAN_ENV = TSAN_OPTIONS=halt_on_error=1:second_deadlock_stack=1 ARISA_TEST_BUDGET_MS=0
TSAN_TARGETS = arisa_async_tests arisa_provider_tests arisa_provider_standard_tests arisa_provider_headless_tests \
	arisa_conformance_tests arisa_composition_tests arisa_runtime_tests arisa_rendering_tests arisa_interaction_tests \
	arisa_provider_sdl3_tests arisa_gallery_tests $(if $(VULKAN_LOADER),arisa_provider_vulkan_tests)
TSAN_LABELS = '^(async|provider.*|conformance|composition|runtime|rendering|interaction)$$'
TSAN_GALLERY = 'GalleryHandOffTest.*:GalleryMemoryTest.*:Pages/GalleryAllocationTest.*/materials:Pages/GalleryAllocationTest.*/glass:Pages/GalleryAllocationTest.*/lists'
FUZZ_SECONDS := 60
SOAK_SECONDS := 300

.PHONY: package package-macos package-linux package-switch package-switch2 package-windows \
	package-check package-check-macos package-check-linux package-check-switch package-check-switch2 package-check-windows
.PHONY: configure build test fast check-build check-test asan tsan fuzz soak leaks nx switch switch-image ounce ounce-image ounce-drako ounce-drako-image lint lint-banned lint-tidy lint-tidy-changed format format-check check update-goldens stack-census

configure:
	@cmake --preset host-debug > /dev/null

build: configure
	$(NICE) cmake --build build/host-debug $(JOBS)

test: build
	$(NICE) ctest --test-dir build/host-debug --output-on-failure $(TEST_JOBS) $(TEST_TIMEOUT)

fast: build
	@$(PYTHON) tools/affected_tests.py build/host-debug 10

check-build: build

check-test: test

asan:
	cmake --preset host-asan > /dev/null
	$(NICE) cmake --build build/host-asan $(JOBS)
	$(ASAN_ENV) $(NICE) ctest --test-dir build/host-asan --output-on-failure $(TEST_JOBS) $(TEST_TIMEOUT)

tsan:
	cmake --preset host-tsan > /dev/null
	$(NICE) cmake --build build/host-tsan $(JOBS) --target $(TSAN_TARGETS)
	$(TSAN_ENV) $(NICE) ctest --test-dir build/host-tsan -L $(TSAN_LABELS) --output-on-failure $(TEST_JOBS) $(TEST_TIMEOUT)
	$(TSAN_ENV) $(NICE) build/host-tsan/arisa_gallery_tests --gtest_brief=1 --gtest_filter=$(TSAN_GALLERY)

soak: configure
	$(NICE) cmake --build build/host-debug $(JOBS) --target arisa_soak
	$(NICE) build/host-debug/arisa_soak --duration $(SOAK_SECONDS) --interval 30 --csv build/host-debug/soak.csv

fuzz:
	$(PYTHON) tools/fuzz_gate.py build/host-fuzz $(FUZZ_SECONDS)

leaks: build
	$(PYTHON) tools/leak_gate.py build/host-debug

switch-image:
	@docker image inspect $(SWITCH_IMAGE) > /dev/null 2>&1 || \
		printf 'FROM %s\nRUN apt-get update && apt-get install -y --no-install-recommends ccache && rm -rf /var/lib/apt/lists/*\n' \
			$(SWITCH_BASE_IMAGE) | docker build -t $(SWITCH_IMAGE) -

nx: switch-image
	@mkdir -p $(SWITCH_CCACHE)
	$(SWITCH_RUN) sh -c 'cmake --preset nx > /dev/null && nice -n 10 cmake --build build/nx -j4'

switch: nx

ounce-image:
	@docker image inspect $(OUNCE_IMAGE) > /dev/null 2>&1 || { \
		test -f "$(OUNCE_SDK_DIR)/ounce-sdk.tar.xz" || { echo "no ounce-sdk.tar.xz in $(OUNCE_SDK_DIR): set OUNCE_SDK_DIR"; exit 1; }; \
		$(NICE) docker build -t $(OUNCE_IMAGE) --build-arg BASE=$(SWITCH_BASE_IMAGE) \
			--build-context ounce-sdk="$(OUNCE_SDK_DIR)" -f toolchain/ounce.Dockerfile toolchain; }

ounce: ounce-image
	@mkdir -p $(OUNCE_CCACHE)
	$(OUNCE_RUN) sh -c 'cmake --preset ounce > /dev/null && nice -n 10 cmake --build build/ounce -j4'

ounce-drako-image: ounce-image
	@docker image inspect $(DRAKO_IMAGE) > /dev/null 2>&1 || \
		$(NICE) docker build -t $(DRAKO_IMAGE) --build-arg BASE=$(OUNCE_IMAGE) \
			--build-context drako3d="$(DRAKO_SOURCES)/drako3d" \
			--build-context uam2="$(DRAKO_SOURCES)/uam2" \
			--build-context drakotools="$(DRAKO_SOURCES)/drakotools" \
			-f toolchain/ounce-drako.Dockerfile toolchain

ounce-drako: ounce-drako-image
	@mkdir -p $(OUNCE_CCACHE)
	$(DRAKO_RUN) sh -c 'cmake --preset ounce-drako > /dev/null && nice -n 10 cmake --build build/ounce-drako -j4'

stack-census:
	cmake --preset host-stack > /dev/null
	$(NICE) cmake --build build/host-stack $(JOBS)
	@$(PYTHON) tools/stack_census.py build/host-stack 20

update-goldens: build
	ARISA_UPDATE_GOLDENS=1 $(NICE) build/host-debug/arisa_text_tests --gtest_filter='TextGolden*'
	ARISA_UPDATE_GOLDENS=1 $(NICE) ctest --test-dir build/host-debug -R 'examples/gallery_' $(TEST_JOBS)
	ARISA_UPDATE_GOLDENS=1 $(NICE) build/host-debug/arisa_gallery_tests --gtest_filter='GalleryLayoutScrollTest.*Golden*'
	ARISA_UPDATE_GOLDENS=1 $(NICE) build/host-debug/arisa_gallery_tests --gtest_filter='GalleryInteractionTest.*Golden*'
	ARISA_UPDATE_GOLDENS=1 $(NICE) build/host-debug/arisa_gallery_tests --gtest_filter='GalleryPanelsTest.*Golden*'

lint: lint-banned lint-tidy

lint-banned:
	@$(PYTHON) tools/banned_symbols.py .

lint-tidy: configure
	@printf '%s\n' $(TIDY_SOURCES) | $(NICE) xargs -P 4 -n 6 $(CLANG_TIDY) $(TIDY_ARGS)

lint-tidy-changed: configure
	@$(if $(strip $(TIDY_CHANGED)),printf '%s\n' $(TIDY_CHANGED) | $(NICE) xargs -P 4 -n 6 $(CLANG_TIDY) $(TIDY_ARGS),echo "lint-tidy-changed: no changed sources")

format:
	@$(CLANG_FORMAT) -i $(FORMAT_FILES)

format-check:
	@$(CLANG_FORMAT) --dry-run --Werror $(FORMAT_FILES)

check:
	@$(MAKE) --no-print-directory format-check
	@$(MAKE) --no-print-directory lint-banned
	@$(MAKE) --no-print-directory lint-tidy-changed
	@$(MAKE) --no-print-directory test

package: package-$(TARGET)

package-macos: TARGET = macos
package-linux: TARGET = linux
package-macos package-linux:
	@test "$(TARGET)" = "$(HOST_PLATFORM)" || { echo "the $(TARGET) package has to be built on $(TARGET)"; exit 1; }
	cmake --preset package-host $(HOST_PACKAGE_FLAGS) > /dev/null
	$(NICE) cmake --build build/package-host $(JOBS)
	rm -rf $(PACKAGE_STAGE)
	cmake --install build/package-host --prefix $(PACKAGE_STAGE)/$(PACKAGE_NAME) > /dev/null
	cmake -DSTAGE=$(PACKAGE_STAGE) -DNAME=$(PACKAGE_NAME) -DOUTPUT_DIR=$(PACKAGE_ROOT) "-DSTRIP=$(HOST_STRIP)" -P cmake/scripts/package_archive.cmake

define package_console
	@mkdir -p $(2)
	$(call CONSOLE_RUN,$(2),$(3)) sh -c 'cmake --preset package-$(1) > /dev/null && \
		nice -n 10 cmake --build build/package-$(1) -j4 && \
		rm -rf $(PACKAGE_STAGE) && \
		cmake --install build/package-$(1) --prefix $(PACKAGE_STAGE)/$(PACKAGE_NAME) > /dev/null && \
		cmake -DSTAGE=$(PACKAGE_STAGE) -DNAME=$(PACKAGE_NAME) -DOUTPUT_DIR=$(PACKAGE_ROOT) "-DSTRIP=$(SWITCH_BIN)/aarch64-none-elf-strip;--strip-debug" -P cmake/scripts/package_archive.cmake'
endef

package-switch: TARGET = switch
package-switch: switch-image
	$(call package_console,switch,$(SWITCH_CCACHE),$(SWITCH_IMAGE))

package-switch2: TARGET = switch2
package-switch2: ounce-drako-image
	$(call package_console,switch2,$(OUNCE_CCACHE),$(DRAKO_IMAGE))

package-windows package-check-windows:
	@echo "arisa does not build for Windows: the toolchain is GCC 16 with SDL3 on macOS and Linux and devkitA64 for the Switch"
	@exit 1

package-check: package-check-$(TARGET)

define package_debug_info_check
test -s $(2)/lib/libarisa.a && \
test "$$($(1) $(2)/lib/libarisa.a | grep -c debug_info)" $(3) 0 && \
echo "$(2)/lib/libarisa.a: $$($(1) $(2)/lib/libarisa.a | grep -c debug_info) debug_info sections"
endef

define package_consumer_host
	cmake -S tests/package/consumer -B $(PACKAGE_CHECK)/$(1) -G Ninja -DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_C_COMPILER=$(CC_HOST) -DCMAKE_CXX_COMPILER=$(CXX_HOST) \
		-DCMAKE_PREFIX_PATH=$(CURDIR)/$(PACKAGE_CHECK)/prefix/$(2) $(3) > /dev/null
	$(NICE) cmake --build $(PACKAGE_CHECK)/$(1) $(JOBS)
	$(PACKAGE_CHECK)/$(1)/arisa_consumer --headless --frames $(PACKAGE_FRAMES)
	$(if $(filter 1,$(PACKAGE_OFFSCREEN)),SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=dummy $(if $(VULKAN_LOADER),SDL_VULKAN_LIBRARY=$(VULKAN_LOADER)) $(PACKAGE_CHECK)/$(1)/arisa_consumer --offscreen --frames $(PACKAGE_FRAMES))
endef

CONSOLE_TOOLCHAIN = $(if $(filter switch2,$(TARGET)),/opt/devkitpro/libounce/cmake/Switch2.cmake,/opt/devkitpro/cmake/Switch.cmake)

define package_consumer_switch
cmake -S tests/package/consumer -B $(PACKAGE_CHECK)/$(1) -G Ninja -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_TOOLCHAIN_FILE=$(CONSOLE_TOOLCHAIN) \
	-DCMAKE_PREFIX_PATH=/src/$(PACKAGE_CHECK)/prefix/$(2) $(3) > /dev/null && \
nice -n 10 cmake --build $(PACKAGE_CHECK)/$(1) -j4 && \
test -s $(PACKAGE_CHECK)/$(1)/arisa_consumer.nro && \
ls -l $(PACKAGE_CHECK)/$(1)/arisa_consumer.nro
endef

package-check-macos: TARGET = macos
package-check-linux: TARGET = linux
package-check-macos package-check-linux: package-check-%: package-%
	rm -rf $(PACKAGE_CHECK)
	mkdir -p $(PACKAGE_CHECK)/prefix
	cmake -E chdir $(PACKAGE_CHECK)/prefix cmake -E tar xzf $(CURDIR)/$(PACKAGE_ROOT)/$(PACKAGE_NAME).tar.gz
	cmake -E chdir $(PACKAGE_CHECK)/prefix cmake -E tar xzf $(CURDIR)/$(PACKAGE_ROOT)/$(PACKAGE_NAME)-debug.tar.gz
	$(call package_debug_info_check,$(HOST_INSPECT),$(PACKAGE_CHECK)/prefix/$(PACKAGE_NAME),-eq)
	$(call package_debug_info_check,$(HOST_INSPECT),$(PACKAGE_CHECK)/prefix/$(PACKAGE_NAME)-debug,-gt)
	$(call package_consumer_host,headers,$(PACKAGE_NAME),)
	$(call package_consumer_host,module,$(PACKAGE_NAME),-DARISA_CONSUMER_MODULE=ON)
	$(call package_consumer_host,debug-headers,$(PACKAGE_NAME)-debug,)
	$(call package_consumer_host,debug-module,$(PACKAGE_NAME)-debug,-DARISA_CONSUMER_MODULE=ON)

package-check-switch: TARGET = switch
package-check-switch: CHECK_RUN = $(SWITCH_RUN)
package-check-switch2: TARGET = switch2
package-check-switch2: CHECK_RUN = $(DRAKO_RUN)
package-check-switch package-check-switch2: package-check-%: package-%
	$(CHECK_RUN) sh -c 'rm -rf $(PACKAGE_CHECK) && mkdir -p $(PACKAGE_CHECK)/prefix && \
		cmake -E chdir $(PACKAGE_CHECK)/prefix cmake -E tar xzf /src/$(PACKAGE_ROOT)/$(PACKAGE_NAME).tar.gz && \
		cmake -E chdir $(PACKAGE_CHECK)/prefix cmake -E tar xzf /src/$(PACKAGE_ROOT)/$(PACKAGE_NAME)-debug.tar.gz && \
		$(call package_debug_info_check,$(SWITCH_BIN)/aarch64-none-elf-objdump -h,$(PACKAGE_CHECK)/prefix/$(PACKAGE_NAME),-eq) && \
		$(call package_debug_info_check,$(SWITCH_BIN)/aarch64-none-elf-objdump -h,$(PACKAGE_CHECK)/prefix/$(PACKAGE_NAME)-debug,-gt) && \
		$(call package_consumer_switch,headers,$(PACKAGE_NAME),) && \
		$(call package_consumer_switch,module,$(PACKAGE_NAME),-DARISA_CONSUMER_MODULE=ON) && \
		$(call package_consumer_switch,debug-headers,$(PACKAGE_NAME)-debug,) && \
		$(call package_consumer_switch,debug-module,$(PACKAGE_NAME)-debug,-DARISA_CONSUMER_MODULE=ON)'
