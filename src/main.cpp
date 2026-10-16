#include "extension_installer.h"
#include "wayland_passthrough.h"

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl3.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <string_view>
#include <vector>

namespace {

std::atomic_bool running = true;

struct Options {
    bool show_help = false;
    double duration_seconds = 0.0;
};

void stop(int) {
    running.store(false);
}

void print_usage(const char* executable) {
    std::printf(
        "Usage: %s [options]\n\n"
        "  --duration SECONDS  Exit automatically; useful for smoke tests\n"
        "  --help              Show this help\n",
        executable);
}

bool parse_options(int argc, char** argv, Options& options) {
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg == "--duration" && i + 1 < argc) {
            options.duration_seconds = std::strtod(argv[++i], nullptr);
        } else if (arg == "--help") {
            options.show_help = true;
        } else {
            std::fprintf(stderr, "Unknown or incomplete option: %s\n", argv[i]);
            print_usage(argv[0]);
            return false;
        }
    }
    return true;
}

void configure_style(float scale) {
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    // style.WindowRounding = 10.0F;
    style.WindowBorderSize = 1.0F;
    // style.Colors[ImGuiCol_Border] = ImVec4(0.12F, 0.85F, 0.68F, 0.70F);
    style.ScaleAllSizes(scale);
    style.FontScaleDpi = scale;
}

void draw_imgui_examples() {
    static bool show_demo_window = true;
    static bool show_another_window = false;
    static float value = 0.0F;
    static int counter = 0;

    if (show_demo_window) {
        ImGui::ShowDemoWindow(&show_demo_window);
    }

    ImGui::Begin("Hello, world!");
    ImGui::TextUnformatted("This is the Dear ImGui SDL3/OpenGL example.");
    ImGui::Checkbox("Demo Window", &show_demo_window);
    ImGui::Checkbox("Another Window", &show_another_window);
    ImGui::SliderFloat("float", &value, 0.0F, 1.0F);
    if (ImGui::Button("Button")) {
        ++counter;
    }
    ImGui::SameLine();
    ImGui::Text("counter = %d", counter);
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
                1000.0F / ImGui::GetIO().Framerate,
                ImGui::GetIO().Framerate);
    ImGui::End();

    if (show_another_window) {
        ImGui::Begin("Another Window", &show_another_window);
        ImGui::TextUnformatted("Hello from another window!");
        if (ImGui::Button("Close Me")) {
            show_another_window = false;
        }
        ImGui::End();
    }
}

std::vector<InputRect> collect_imgui_input_rectangles() {
    const ImGuiContext& context = *ImGui::GetCurrentContext();
    const ImVec2 display_size = ImGui::GetIO().DisplaySize;
    std::vector<InputRect> rectangles;
    rectangles.reserve(static_cast<std::size_t>(context.Windows.Size));

    for (const ImGuiWindow* window : context.Windows) {
        if (!window->Active || window->Hidden || window->IsFallbackWindow ||
            (window->Flags & ImGuiWindowFlags_NoMouseInputs) != 0) {
            continue;
        }

        const int left = std::max(0, static_cast<int>(std::floor(window->Pos.x)));
        const int top = std::max(0, static_cast<int>(std::floor(window->Pos.y)));
        const int right = std::min(static_cast<int>(std::ceil(display_size.x)),
                                   static_cast<int>(std::ceil(window->Pos.x +
                                                              window->Size.x)));
        const int bottom = std::min(static_cast<int>(std::ceil(display_size.y)),
                                    static_cast<int>(std::ceil(window->Pos.y +
                                                               window->Size.y)));
        if (right > left && bottom > top) {
            rectangles.push_back({left, top, right - left, bottom - top});
        }
    }
    return rectangles;
}

}  // namespace

int main(int argc, char** argv) {
    Options options;
    if (!parse_options(argc, argv, options)) {
        return EXIT_FAILURE;
    }
    if (options.show_help) {
        print_usage(argv[0]);
        return EXIT_SUCCESS;
    }

    const char* skip_extension =
        std::getenv("GNOME_IMGUI_OVERLAY_SKIP_EXTENSION");
    if ((skip_extension == nullptr || std::string_view(skip_extension) != "1") &&
        !ensure_shell_extension(argv[0]))
        return EXIT_SUCCESS;

    std::signal(SIGINT, stop);
    std::signal(SIGTERM, stop);

    SDL_SetHint(SDL_HINT_APP_ID, "io.github.ankitrawat.GnomeImGuiOverlay");
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "wayland");
    SDL_SetHint(SDL_HINT_VIDEO_WAYLAND_ALLOW_LIBDECOR, "0");
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return EXIT_FAILURE;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);

    SDL_Rect screen_area{0, 0, 1280, 800};
    const SDL_DisplayID display = SDL_GetPrimaryDisplay();
    if (!SDL_GetDisplayUsableBounds(display, &screen_area)) {
        SDL_GetDisplayBounds(display, &screen_area);
    }
    const int initial_width = std::clamp(screen_area.w - 64, 320, 960);
    const int initial_height = std::clamp(screen_area.h - 64, 240, 640);

    constexpr SDL_WindowFlags flags =
        SDL_WINDOW_OPENGL | SDL_WINDOW_TRANSPARENT | SDL_WINDOW_BORDERLESS |
        SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_RESIZABLE;
    SDL_Window* window = SDL_CreateWindow("GNOME ImGui Overlay", initial_width,
                                          initial_height, flags);
    if (window == nullptr) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return EXIT_FAILURE;
    }

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (gl_context == nullptr) {
        std::fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return EXIT_FAILURE;
    }

    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1);
    SDL_ShowWindow(window);

    WaylandInputRegion input_region;
    if (!input_region.initialize(window)) {
        std::fprintf(stderr, "Could not initialize selective input pass-through.\n");
        SDL_GL_DestroyContext(gl_context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return EXIT_FAILURE;
    }

    // Mutter auto-maximizes large windows when they are first mapped (controlled
    // by org.gnome.mutter auto-maximize). Map one transparent frame at a smaller
    // size first, then grow the normal toplevel to the work area. This preserves
    // GNOME's user-controlled "Always on Top" menu item.
    int initial_pixel_width = 0;
    int initial_pixel_height = 0;
    SDL_GetWindowSizeInPixels(window, &initial_pixel_width, &initial_pixel_height);
    glViewport(0, 0, initial_pixel_width, initial_pixel_height);
    glClearColor(0.0F, 0.0F, 0.0F, 0.0F);
    glClear(GL_COLOR_BUFFER_BIT);
    SDL_GL_SwapWindow(window);
    SDL_SyncWindow(window);

    if (!SDL_SetWindowSize(window, screen_area.w, screen_area.h)) {
        std::fprintf(stderr, "Could not resize overlay to the work area: %s\n",
                     SDL_GetError());
    }
    SDL_SyncWindow(window);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    const float scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
    configure_style(scale > 0.0F ? scale : 1.0F);
    ImGui_ImplSDL3_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init("#version 130");

    int window_width = 0;
    int window_height = 0;
    SDL_GetWindowSize(window, &window_width, &window_height);
    const SDL_WindowFlags actual_flags = SDL_GetWindowFlags(window);
    std::printf(
        "Backend: %s | selective ImGui input: yes | size: %dx%d | "
        "maximized: %s | fullscreen: %s\n",
        SDL_GetCurrentVideoDriver(), window_width, window_height,
        (actual_flags & SDL_WINDOW_MAXIMIZED) != 0 ? "yes" : "no",
        (actual_flags & SDL_WINDOW_FULLSCREEN) != 0 ? "yes" : "no");
    std::printf("Stop with Ctrl+C in this terminal.\n");

    const auto started_at = std::chrono::steady_clock::now();
    while (running.load()) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT ||
                (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                 event.window.windowID == SDL_GetWindowID(window))) {
                running.store(false);
            }
        }

        if (options.duration_seconds > 0.0) {
            const auto elapsed = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - started_at);
            if (elapsed.count() >= options.duration_seconds) {
                break;
            }
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        draw_imgui_examples();
        input_region.update(collect_imgui_input_rectangles());

        ImGui::Render();
        int width = 0;
        int height = 0;
        SDL_GetWindowSizeInPixels(window, &width, &height);
        glViewport(0, 0, width, height);
        glClearColor(0.0F, 0.0F, 0.0F, 0.0F);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    input_region.shutdown();
    SDL_GL_DestroyContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return EXIT_SUCCESS;
}
