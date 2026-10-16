#pragma once

#include <filesystem>

// Ensures that the companion GNOME Shell extension is installed and enabled.
// Returns false when this is the first install and GNOME needs one login refresh.
bool ensure_shell_extension(const std::filesystem::path& executable_path);
