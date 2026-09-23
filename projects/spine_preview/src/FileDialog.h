#ifndef __FILEDIALOG_H__
#define __FILEDIALOG_H__

#include <string>
#include <vector>

namespace FileDialog {

// Extension lists contain raw extensions without a dot, e.g. { "json", "skel" }.
// An empty string is returned when the user cancels.

std::string OpenFile(const std::string& title,
                     const std::vector<std::string>& extensions);

std::string SaveFile(const std::string& title, const std::string& defaultName,
                     const std::vector<std::string>& extensions);

// True when this platform really has one. Without it SaveFile() always returns
// an empty string, which the caller cannot tell apart from the user cancelling
// - it has to fall back to another way of getting a path instead of silently
// doing nothing.
bool Available();

} // namespace FileDialog

#endif /* ifndef __FILEDIALOG_H__ */
