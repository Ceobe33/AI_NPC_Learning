#ifndef __LAYOUTSTORE_H__
#define __LAYOUTSTORE_H__

#include <string>

// Where the editor keeps its window layout.
//
// Dear ImGui normally reads and writes "imgui.ini" in the working directory.
// That does not work here: a browser has no disk to write to (its in-memory
// file system disappears with the page) and a bundled desktop app must not
// scribble next to itself, both because that directory is read-only once the
// app is signed or installed and because a checked-in imgui.ini would drag a
// stale layout onto a fresh machine.
//
// Both hosts get the same deal instead: the layout is stored per user -
// localStorage in the browser, a file under the user's application data
// directory on the desktop - and anything with nothing stored yet falls back
// to the layout the code builds, which is therefore identical on both.
namespace LayoutStore {

// The settings saved by a previous session, or an empty string on a first run.
std::string Load();

void Save(const std::string& settings);

// Throws the stored layout away, so the next start builds the default again.
void Clear();

}  // namespace LayoutStore

#endif /* ifndef __LAYOUTSTORE_H__ */
