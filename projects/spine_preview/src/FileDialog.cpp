#ifndef __APPLE__

#include "FileDialog.h"

// No native dialog implementation for this platform yet; the caller falls back
// to the last used path.
namespace FileDialog {

std::string OpenFile(const std::string&, const std::vector<std::string>&) {
    return {};
}

std::string SaveFile(const std::string&, const std::string&,
                     const std::vector<std::string>&) {
    return {};
}

} // namespace FileDialog

#endif
