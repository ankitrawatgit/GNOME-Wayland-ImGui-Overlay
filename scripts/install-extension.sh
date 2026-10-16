#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
extension_dir="$project_dir/extension"
extension_uuid="gnome-imgui-overlay@ankitrawat"
package_dir="$(mktemp -d)"
trap 'rm -rf -- "$package_dir"' EXIT

gnome-extensions pack --force --out-dir "$package_dir" "$extension_dir"
gnome-extensions install --force \
    "$package_dir/$extension_uuid.shell-extension.zip"

# GNOME Wayland cannot reload the Shell in-place to discover a brand-new local
# extension. Pre-enable it in GSettings so the next login loads it immediately.
update_extension_list() {
    local key="$1"
    local operation="$2"
    local current updated
    current="$(gsettings get org.gnome.shell "$key")"
    updated="$(python3 - "$extension_uuid" "$operation" "$current" <<'PY'
import ast
import sys

uuid, operation, raw = sys.argv[1:]
if raw.startswith('@as '):
    raw = raw[4:]
values = ast.literal_eval(raw)
values = [value for value in values if value != uuid]
if operation == 'add':
    values.append(uuid)
print(repr(values))
PY
)"
    gsettings set org.gnome.shell "$key" "$updated"
}

update_extension_list disabled-extensions remove
update_extension_list enabled-extensions add

if gnome-extensions enable "$extension_uuid"; then
    echo "Enabled $extension_uuid"
    echo "Future overlay launches will be sized and kept above automatically."
else
    echo "Installed and pre-enabled $extension_uuid."
    echo "Log out and back in once so this Wayland session can load it."
    echo "After that, launching the overlay is the only step."
fi
