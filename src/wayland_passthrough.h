#pragma once

#include <vector>

struct SDL_Window;
struct wl_compositor;
struct wl_surface;

struct InputRect {
    int x;
    int y;
    int width;
    int height;

    bool operator==(const InputRect&) const = default;
};

// Maintains a Wayland input region containing only the visible ImGui windows.
// Pointer/touch events inside those rectangles reach ImGui; events everywhere
// else pass through to the surface underneath.
class WaylandInputRegion {
public:
    WaylandInputRegion() = default;
    ~WaylandInputRegion();

    WaylandInputRegion(const WaylandInputRegion&) = delete;
    WaylandInputRegion& operator=(const WaylandInputRegion&) = delete;

    bool initialize(SDL_Window* window);
    bool update(const std::vector<InputRect>& rectangles);
    void shutdown();

private:
    wl_surface* surface_ = nullptr;
    wl_compositor* compositor_ = nullptr;
    std::vector<InputRect> rectangles_;
    bool region_was_set_ = false;
};
