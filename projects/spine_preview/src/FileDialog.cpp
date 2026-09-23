#ifndef __APPLE__

#include "FileDialog.h"

// No native dialog implementation for this platform yet. Available() reports
// that, so the caller can fall back to another way of getting a path (a
// command line argument, a dropped file, or a default output location) instead
// of silently doing nothing.
namespace FileDialog {

bool Available() {
    return false;
}

std::string OpenFile(const std::string&, const std::vector<std::string>&) {
    return {};
}

std::string SaveFile(const std::string&, const std::string&,
                     const std::vector<std::string>&) {
    return {};
}

} // namespace FileDialog

#endif
