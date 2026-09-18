#ifndef __FILEDIALOG_H__
#define __FILEDIALOG_H__

#include <string>
#include <vector>

namespace FileDialog {

// Extension lists contain raw extensions without a dot, e.g. { "json", "skel" }.
// An empty string is returned when the user cancels.

std::string OpenFile(const std::string& title,
                     const std::vector<std::string>& extensions);

std::string SaveFile(const std::string& title,
                     const std::string& defaultName,
                     const std::vector<std::string>& extensions);

} // namespace FileDialog

#endif /* ifndef __FILEDIALOG_H__ */
