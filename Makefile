BUILD_DIR := build
VCPKG_DIR := vcpkg
TOOLCHAIN_FILE := $(VCPKG_DIR)/scripts/buildsystems/vcpkg.cmake
CMAKE := cmake

.PHONY: configure build clean run vcpkg

setup: vcpkg configure build

rebuild: clean configure build

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

vcpkg:
	@if [ ! -d "$(VCPKG_DIR)" ]; then \
		git clone https://github.com/microsoft/vcpkg.git $(VCPKG_DIR); \
	fi
	@cd $(VCPKG_DIR) && ./bootstrap-vcpkg.sh

configure: $(BUILD_DIR)
	@cd $(BUILD_DIR) && $(CMAKE) .. \
		-DCMAKE_TOOLCHAIN_FILE=../$(TOOLCHAIN_FILE) \
		-DCMAKE_BUILD_TYPE=Release

build:
	@$(CMAKE) --build $(BUILD_DIR) -- -j$$(nproc)

run: build
	@./$(BUILD_DIR)/bmo_backend $(ARGS)

clean:
	rm -rf $(BUILD_DIR)