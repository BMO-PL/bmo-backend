BUILD_DIR := build
VCPKG_DIR := vcpkg
TOOLCHAIN_FILE := $(VCPKG_DIR)/scripts/buildsystems/vcpkg.cmake
CMAKE := cmake

UNAME_M := $(shell uname -m)
ifeq ($(UNAME_M),aarch64)
  VCPKG_TRIPLET ?= arm64-linux-dynamic
else
  VCPKG_TRIPLET ?= x64-linux-dynamic
endif
VCPKG_INSTALLED := $(BUILD_DIR)/vcpkg_installed/$(VCPKG_TRIPLET)
VCPKG_LIBDIR := $(VCPKG_INSTALLED)/lib

VENV_DIR := .venv
PY := $(VENV_DIR)/bin/python
PIP := $(PY) -m pip

PY_REQ := python/requirements.txt

.PHONY: setup rebuild configure build clean clean-all run vcpkg builddir \
		py-venv py-deps py-clean 

setup: vcpkg builddir configure build

py-setup: setup py-deps

rebuild: clean setup

builddir:
	mkdir -p $(BUILD_DIR)

vcpkg:
	@if [ ! -d "$(VCPKG_DIR)/.git" ] && [ ! -f "$(VCPKG_DIR)/.git" ]; then \
		echo "ERROR: vcpkg submodule not initialized."; \
		echo "Run: git submodule update --init --recursive"; \
		exit 1; \
	fi
	@cd $(VCPKG_DIR) && ./bootstrap-vcpkg.sh

configure: builddir
	@printf "\nConfiguring project...\n\n"
	@cd $(BUILD_DIR) && $(CMAKE) .. \
		-DCMAKE_TOOLCHAIN_FILE=../$(TOOLCHAIN_FILE) \
		-DVCPKG_TARGET_TRIPLET=$(VCPKG_TRIPLET) \
		-DCMAKE_BUILD_TYPE=Release

build: builddir
	@printf "\nBuilding project...\n\n"
	@$(CMAKE) --build $(BUILD_DIR) -- -j$$(nproc)

run: build
	@./$(BUILD_DIR)/bmo_backend $(ARGS)

py-venv:
	@if [ ! -d "$(VENV_DIR)" ]; then \
		python3 -m venv $(VENV_DIR); \
	fi
	@$(PIP) install -U pip wheel setuptools

py-deps: py-venv 
	@$(PIP) install -r $(PY_REQ)

clean:
	@rm -rf $(BUILD_DIR)

py-clean:
	@rm -rf $(VENV_DIR)

clean-all: clean py-clean

docker-build:
	docker build -t bmo-backend:dev -f Dockerfile.build .

docker-run:
	docker run --rm -it \
	  --device /dev/snd \
	  --group-add audio \
	  bmo-backend:dev

wake-run:
	@$(PY) python/wake/wake_word.py