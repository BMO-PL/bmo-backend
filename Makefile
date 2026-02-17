BUILD_DIR := build
VCPKG_DIR := vcpkg
TOOLCHAIN_FILE := $(VCPKG_DIR)/scripts/buildsystems/vcpkg.cmake
CMAKE := cmake

.PHONY: setup rebuild configure build clean run vcpkg builddir

setup: vcpkg builddir configure build

rebuild: clean setup

builddir:
	mkdir -p $(BUILD_DIR)

vcpkg:
	@if [ ! -d "$(VCPKG_DIR)" ]; then \
		git clone https://github.com/microsoft/vcpkg.git $(VCPKG_DIR); \
	fi
	@cd $(VCPKG_DIR) && ./bootstrap-vcpkg.sh

configure: builddir
	@printf "\nConfiguring project...\n\n"
	@cd $(BUILD_DIR) && $(CMAKE) .. \
		-DCMAKE_TOOLCHAIN_FILE=../$(TOOLCHAIN_FILE) \
		-DCMAKE_BUILD_TYPE=Release

build: builddir
	@printf "\nBuilding project...\n\n"
	@$(CMAKE) --build $(BUILD_DIR) -- -j$$(nproc)

run: build
	@./$(BUILD_DIR)/bmo_backend $(ARGS)

clean:
	@rm -rf $(BUILD_DIR)