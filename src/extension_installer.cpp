#include "extension_installer.h"

#include <fcntl.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace {

constexpr const char* extension_uuid = "gnome-imgui-overlay@ankitrawat";

int run_process(const std::vector<std::string>& arguments, bool quiet = false) {
    if (arguments.empty())
        return -1;

    const pid_t child = fork();
    if (child < 0)
        return -1;

    if (child == 0) {
        if (quiet) {
            const int null_fd = open("/dev/null", O_RDWR);
            if (null_fd >= 0) {
                dup2(null_fd, STDOUT_FILENO);
                dup2(null_fd, STDERR_FILENO);
                close(null_fd);
            }
        }

        std::vector<char*> argv;
        argv.reserve(arguments.size() + 1);
        for (const std::string& argument : arguments)
            argv.push_back(const_cast<char*>(argument.c_str()));
        argv.push_back(nullptr);
        execvp(argv.front(), argv.data());
        _exit(127);
    }

    int status = 0;
    if (waitpid(child, &status, 0) < 0)
        return -1;
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

std::filesystem::path user_extension_path() {
    const passwd* user = getpwuid(getuid());
    if (user == nullptr || user->pw_dir == nullptr)
        return {};
    return std::filesystem::path(user->pw_dir) /
        ".local/share/gnome-shell/extensions" / extension_uuid / "metadata.json";
}

void notify_login_required() {
    const std::string message =
        "Overlay controller installed. Log out and back in once, then launch "
        "the overlay again.";
    std::fprintf(stderr, "\n%s\n\n", message.c_str());
    run_process({"notify-send", "GNOME ImGui Overlay", message}, true);
}

}  // namespace

bool ensure_shell_extension(const std::filesystem::path& executable_path) {
    if (run_process({"gnome-extensions", "info", extension_uuid}, true) == 0) {
        if (run_process({"gnome-extensions", "enable", extension_uuid}, true) != 0) {
            std::fprintf(stderr, "Could not enable GNOME extension %s.\n",
                         extension_uuid);
            return false;
        }
        return true;
    }

    const std::filesystem::path installed_metadata = user_extension_path();
    if (!installed_metadata.empty() && std::filesystem::exists(installed_metadata)) {
        notify_login_required();
        return false;
    }

    std::error_code error;
    std::filesystem::path resolved_executable =
        std::filesystem::read_symlink("/proc/self/exe", error);
    if (error) {
        error.clear();
        resolved_executable = std::filesystem::canonical(executable_path, error);
    }
    if (error) {
        std::fprintf(stderr, "Could not resolve executable path: %s\n",
                     error.message().c_str());
        return false;
    }

    const std::filesystem::path installer =
        resolved_executable.parent_path().parent_path() /
        "scripts/install-extension.sh";
    if (!std::filesystem::exists(installer)) {
        std::fprintf(stderr, "Extension installer not found at %s\n",
                     installer.c_str());
        return false;
    }

    std::printf("First launch: installing the GNOME Shell controller...\n");
    if (run_process({installer.string()}) != 0) {
        std::fprintf(stderr, "Extension installation failed.\n");
        return false;
    }

    // A brand-new local extension cannot be discovered by the already-running
    // GNOME Wayland session. The installer pre-enables it for the next login.
    notify_login_required();
    return false;
}
