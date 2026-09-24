#include "LayoutStore.h"

#if defined(__EMSCRIPTEN__)

#include <cstdlib>

#include <emscripten.h>

// localStorage is the only place a page can keep anything between visits, so
// it is what makes the browser layout survive a reload. Every access is
// guarded: private browsing and a full quota both throw rather than fail
// quietly.
EM_JS(char*, spine_studio_layout_load, (), {
    try {
        const data = localStorage.getItem("spine-animation-studio/layout");

        if (data === null) {
            return 0;
        }

        const size = lengthBytesUTF8(data) + 1;
        const pointer = _malloc(size);
        stringToUTF8(data, pointer, size);

        return pointer;
    } catch (error) {
        return 0;
    }
});

EM_JS(void, spine_studio_layout_save, (const char* settings), {
    try {
        localStorage.setItem("spine-animation-studio/layout",
                             UTF8ToString(settings));
    } catch (error) {
        // Not worth interrupting a session over: without this the layout
        // simply does not survive the reload.
    }
});

EM_JS(void, spine_studio_layout_clear, (), {
    try {
        localStorage.removeItem("spine-animation-studio/layout");
    } catch (error) {
    }
});

namespace LayoutStore {

std::string Load() {
    char* data = spine_studio_layout_load();

    if (data == nullptr) {
        return {};
    }

    std::string settings(data);

    std::free(data);

    return settings;
}

void Save(const std::string& settings) {
    spine_studio_layout_save(settings.c_str());
}

void Clear() {
    spine_studio_layout_clear();
}

}  // namespace LayoutStore

#else  // !defined(__EMSCRIPTEN__)

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace {

// Per user rather than next to the executable: a distributed app lives in a
// directory the user cannot write to, and this way the layout follows the
// account instead of the copy.
std::filesystem::path LayoutPath() {
    const char* home = std::getenv("HOME");

    if (home == nullptr || home[0] == '\0') {
        // Last resort for a system without a home directory: keep the historic
        // behaviour of writing next to wherever the app was started.
        return std::filesystem::path("imgui.ini");
    }

#if defined(__APPLE__)
    return std::filesystem::path(home) / "Library" / "Application Support" /
           "Spine Animation Studio" / "layout.ini";
#else
    return std::filesystem::path(home) / ".config" / "spine-animation-studio" /
           "layout.ini";
#endif
}

}  // namespace

namespace LayoutStore {

std::string Load() {
    std::ifstream file(LayoutPath(), std::ios::binary);

    if (!file) {
        return {};
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();

    return buffer.str();
}

void Save(const std::string& settings) {
    const std::filesystem::path path = LayoutPath();

    std::error_code error;

    // The directory does not exist on a first run.
    std::filesystem::create_directories(path.parent_path(), error);

    if (error) {
        return;
    }

    // Writing to a side file and renaming keeps a crash or a full disk from
    // leaving a half-written layout behind.
    const std::filesystem::path temporary = path.string() + ".tmp";

    {
        std::ofstream file(temporary, std::ios::binary | std::ios::trunc);

        if (!file) {
            return;
        }

        file.write(settings.data(), static_cast<std::streamsize>(settings.size()));
    }

    std::filesystem::rename(temporary, path, error);

    if (error) {
        std::filesystem::remove(temporary, error);
    }
}

void Clear() {
    std::error_code error;

    std::filesystem::remove(LayoutPath(), error);
}

}  // namespace LayoutStore

#endif
