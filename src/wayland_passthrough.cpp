#include "wayland_passthrough.h"

#include <SDL3/SDL.h>
#include <wayland-client.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {

struct RegistryState {
    wl_compositor* compositor = nullptr;
};

void on_global(void* data, wl_registry* registry, std::uint32_t name,
               const char* interface, std::uint32_t version) {
    auto& state = *static_cast<RegistryState*>(data);
    if (std::strcmp(interface, wl_compositor_interface.name) == 0) {
        const auto supported_version = std::min(version, 4U);
        state.compositor = static_cast<wl_compositor*>(
            wl_registry_bind(registry, name, &wl_compositor_interface,
                             supported_version));
    }
}

void on_global_remove(void*, wl_registry*, std::uint32_t) {}

constexpr wl_registry_listener registry_listener = {
    .global = on_global,
    .global_remove = on_global_remove,
};

}  // namespace

WaylandInputRegion::~WaylandInputRegion() {
    shutdown();
}

bool WaylandInputRegion::initialize(SDL_Window* window) {
    const char* driver = SDL_GetCurrentVideoDriver();
    if (driver == nullptr || std::strcmp(driver, "wayland") != 0) {
        std::fprintf(stderr,
                     "Selective input needs SDL's Wayland backend; current "
                     "driver: %s\n",
                     driver != nullptr ? driver : "unknown");
        return false;
    }

    const SDL_PropertiesID properties = SDL_GetWindowProperties(window);
    auto* display = static_cast<wl_display*>(SDL_GetPointerProperty(
        properties, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr));
    surface_ = static_cast<wl_surface*>(SDL_GetPointerProperty(
        properties, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr));

    if (display == nullptr || surface_ == nullptr) {
        std::fprintf(stderr, "SDL did not expose its Wayland display/surface.\n");
        surface_ = nullptr;
        return false;
    }

    wl_registry* registry = wl_display_get_registry(display);
    if (registry == nullptr) {
        std::fprintf(stderr, "Could not obtain the Wayland registry.\n");
        surface_ = nullptr;
        return false;
    }

    RegistryState state;
    wl_registry_add_listener(registry, &registry_listener, &state);
    const bool roundtrip_ok = wl_display_roundtrip(display) >= 0;
    wl_registry_destroy(registry);

    if (!roundtrip_ok || state.compositor == nullptr) {
        std::fprintf(stderr, "Could not bind the Wayland compositor.\n");
        surface_ = nullptr;
        return false;
    }

    compositor_ = state.compositor;
    return update({});
}

bool WaylandInputRegion::update(const std::vector<InputRect>& rectangles) {
    if (surface_ == nullptr || compositor_ == nullptr) {
        return false;
    }
    if (region_was_set_ && rectangles == rectangles_) {
        return true;
    }

    wl_region* region = wl_compositor_create_region(compositor_);
    if (region == nullptr) {
        std::fprintf(stderr, "Could not create a Wayland input region.\n");
        return false;
    }

    for (const InputRect& rectangle : rectangles) {
        if (rectangle.width > 0 && rectangle.height > 0) {
            wl_region_add(region, rectangle.x, rectangle.y, rectangle.width,
                          rectangle.height);
        }
    }

    // Wayland copies this region on the next surface commit. SDL's upcoming
    // buffer swap performs that commit, so the temporary region can be freed.
    wl_surface_set_input_region(surface_, region);
    wl_region_destroy(region);

    rectangles_ = rectangles;
    region_was_set_ = true;
    return true;
}

void WaylandInputRegion::shutdown() {
    if (compositor_ != nullptr) {
        wl_compositor_destroy(compositor_);
    }
    compositor_ = nullptr;
    surface_ = nullptr;
    rectangles_.clear();
    region_was_set_ = false;
}
