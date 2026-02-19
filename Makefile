BUILD_DIR := build
VCPKG_DIR := vcpkg
TOOLCHAIN_FILE := $(VCPKG_DIR)/scripts/buildsystems/vcpkg.cmake
CMAKE := cmake
VENV_DIR := .venv
PY_REQ := python/requirements.txt

CMAKE_GEN ?= Ninja

ifeq ($(OS),Windows_NT)
  BUILD_DIR := build-win
  VCPKG_TRIPLET ?= x64-windows

  PY := $(VENV_DIR)/Scripts/python.exe
  PIP := "$(PY)" -m pip

  CORES := $(NUMBER_OF_PROCESSORS)

  VCPKG_BOOTSTRAP := cd $(VCPKG_DIR) && ./bootstrap-vcpkg.bat

  EXE := $(BUILD_DIR)/bmo_backend.exe

  CMAKE_GEN := "NMake Makefiles"
else
  UNAME_M := $(shell uname -m)
  ifeq ($(UNAME_M),aarch64)
    VCPKG_TRIPLET ?= arm64-linux-dynamic
  else
    VCPKG_TRIPLET ?= x64-linux-dynamic
  endif

  PY := $(VENV_DIR)/bin/python
  PIP := $(PY) -m pip

  CORES := $(shell nproc)

  VCPKG_BOOTSTRAP := cd $(VCPKG_DIR) && ./bootstrap-vcpkg.sh

  EXE := $(BUILD_DIR)/bmo_backend
endif

VCPKG_INSTALLED := $(BUILD_DIR)/vcpkg_installed/$(VCPKG_TRIPLET)
VCPKG_LIBDIR := $(VCPKG_INSTALLED)/lib
VCPKG_COMMIT ?= ce35b1a53aac26d7fcdb8ee1ef7a8e4eea02d27b

.PHONY: setup py-setup rebuild configure build clean clean-all run vcpkg builddir vcpkg-install vcpkg-upgrade \
		py-venv py-deps py-clean docker-build-pi docker-build-amd64 docker-build docker-run wake-run

setup:
ifeq ($(OS),Windows_NT)
	@cmake -S . -B build-win -G "Ninja" -DCMAKE_TOOLCHAIN_FILE=$(TOOLCHAIN_FILE) -DVCPKG_TARGET_TRIPLET=x64-windows
	@cmake --build build-win --config Release
else
	vcpkg configure build
endif

py-setup: setup py-deps

rebuild: clean configure build

builddir:
ifeq ($(OS),Windows_NT)
	@powershell -NoProfile -Command "New-Item -ItemType Directory -Force '$(BUILD_DIR)' | Out-Null"
else
	mkdir -p $(BUILD_DIR)
endif

vcpkg:
ifeq ($(OS),Windows_NT)
	@powershell -NoProfile -Command "if (!(Test-Path '$(VCPKG_DIR)')) { git clone https://github.com/microsoft/vcpkg.git '$(VCPKG_DIR)' }"
	@powershell -NoProfile -Command "if ('$(VCPKG_COMMIT)' -ne '') { cd '$(VCPKG_DIR)'; git fetch --all --tags; git checkout '$(VCPKG_COMMIT)' }"
	@powershell -NoProfile -Command "cd '$(VCPKG_DIR)'; .\bootstrap-vcpkg.bat"
else
	@if [ ! -d "$(VCPKG_DIR)" ]; then git clone https://github.com/microsoft/vcpkg.git "$(VCPKG_DIR)"; fi
	@if [ -n "$(VCPKG_COMMIT)" ]; then cd "$(VCPKG_DIR)" && git fetch --all --tags && git checkout "$(VCPKG_COMMIT)"; fi
	@cd $(VCPKG_DIR) && ./bootstrap-vcpkg.sh
endif

vcpkg-install: vcpkg
ifeq ($(OS),Windows_NT)
	@powershell -NoProfile -Command "& '$(VCPKG_DIR)\vcpkg.exe' install --triplet '$(VCPKG_TRIPLET)'"
else
	@$(VCPKG_DIR)/vcpkg install --triplet "$(VCPKG_TRIPLET)"
endif

vcpkg-upgrade: vcpkg
ifeq ($(OS),Windows_NT)
	@powershell -NoProfile -Command "& '$(VCPKG_DIR)\vcpkg.exe' upgrade --no-dry-run"
else
	@$(VCPKG_DIR)/vcpkg upgrade --no-dry-run
endif

configure: builddir
ifeq ($(OS),Windows_NT)
	@printf "\nProject must be built on Visual Studio on Windows"
else
	@printf "\nConfiguring project...\n\n"
	@cd $(BUILD_DIR) && $(CMAKE) .. \
		-G "$(CMAKE_GEN)" \
		-DCMAKE_TOOLCHAIN_FILE=../$(TOOLCHAIN_FILE) \
		-DVCPKG_TARGET_TRIPLET=$(VCPKG_TRIPLET) \
		-DCMAKE_BUILD_TYPE=Release
endif

build: builddir
ifeq ($(OS),Windows_NT)
	@printf "\nProject must be built on Visual Studio on Windows"
else
	@printf "\nBuilding project...\n\n"
	@$(CMAKE) --build $(BUILD_DIR) -- -j$(CORES)
endif

run:
ifeq ($(OS),Windows_NT)
	@powershell -NoProfile -Command "& '.\build-win\bmo_backend.exe'"
else
	@./$(BUILD_DIR)/bmo_backend
endif

py-venv:
ifeq ($(OS),Windows_NT)
	@if [ ! -d "$(VENV_DIR)" ]; then py -m venv $(VENV_DIR); fi
else
	@if [ ! -d "$(VENV_DIR)" ]; then python3 -m venv $(VENV_DIR); fi
endif
	@$(PIP) install -U pip wheel setuptools

py-deps: py-venv
	@$(PIP) install -r $(PY_REQ)

clean:
ifeq ($(OS),Windows_NT)
	@powershell -NoProfile -Command "if (Test-Path '$(BUILD_DIR)') { Remove-Item -Recurse -Force '$(BUILD_DIR)' }"
else
	@rm -rf $(BUILD_DIR)
endif

py-clean:
ifeq ($(OS),Windows_NT)
	@powershell -NoProfile -Command "if (Test-Path '$(VENV_DIR)') { Remove-Item -Recurse -Force '$(VENV_DIR)' }"
else
	@rm -rf $(VENV_DIR)
endif

clean-all: clean py-clean

docker-build-pi:
	docker buildx build --platform linux/arm64 -t bmo-backend:pi -f Dockerfile.build .

docker-build-amd64:
	docker buildx build --platform linux/amd64 -t bmo-backend:amd64 -f Dockerfile.build .

docker-build:
	docker build -t bmo-backend:dev -f Dockerfile.build .

docker-run:
	docker run --rm -it \
	  --device /dev/snd \
	  --group-add audio \
	  bmo-backend:dev

wake-run:
	@"$(PY)" python/wake/wake_word.py
