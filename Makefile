CXX ?= g++

TARGET := build/gnome-imgui-overlay
IMGUI := third_party/imgui
SOURCES := \
	src/extension_installer.cpp \
	src/main.cpp \
	src/wayland_passthrough.cpp \
	$(IMGUI)/imgui.cpp \
	$(IMGUI)/imgui_draw.cpp \
	$(IMGUI)/imgui_tables.cpp \
	$(IMGUI)/imgui_widgets.cpp \
	$(IMGUI)/imgui_demo.cpp \
	$(IMGUI)/backends/imgui_impl_sdl3.cpp \
	$(IMGUI)/backends/imgui_impl_opengl3.cpp

OBJECTS := $(SOURCES:%=build/%.o)
DEPS := $(OBJECTS:.o=.d)

CPPFLAGS := -Isrc -I$(IMGUI) -I$(IMGUI)/backends
CXXFLAGS := -std=c++20 -O2 -g -Wall -Wextra -Wpedantic -MMD -MP
LDLIBS := -ldl -lpthread

REQUIRED_PACKAGES := sdl3 wayland-client gl
BUILD_GOALS := $(if $(MAKECMDGOALS),$(filter-out clean install-extension,$(MAKECMDGOALS)),all)

ifneq ($(strip $(BUILD_GOALS)),)
ifeq ($(shell pkg-config --exists $(REQUIRED_PACKAGES) && echo yes),)
$(error Missing development packages: $(REQUIRED_PACKAGES). Install them with your distribution's package manager)
endif

CPPFLAGS += $(shell pkg-config --cflags $(REQUIRED_PACKAGES))
LDLIBS += $(shell pkg-config --libs $(REQUIRED_PACKAGES))
endif

.PHONY: all clean install-extension run

all: $(TARGET)

install-extension:
	./scripts/install-extension.sh

run: $(TARGET)
	./$(TARGET)

$(TARGET): $(OBJECTS)
	@mkdir -p $(@D)
	$(CXX) $(OBJECTS) $(LDFLAGS) $(LDLIBS) -o $@

build/%.o: %
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

clean:
	$(RM) -r build

-include $(DEPS)
