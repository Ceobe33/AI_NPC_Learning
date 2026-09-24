#ifndef __WEBFILEBRIDGE_H__
#define __WEBFILEBRIDGE_H__

#include <string>

// Browser replacement for everything the native builds get from the OS:
// choosing a local Spine export and getting the finished GIF back out.
//
// A page cannot see the user's disk, so this talks to the DOM instead: a
// hidden <input type="file"> for the import and an <a download> for the
// export, both against Emscripten's in-memory file system.
//
// Every function is a no-op on native builds, which keeps the #ifdef noise out
// of the places that call them. Check Available() first if a caller needs to
// explain itself in the UI.
namespace WebFileBridge {

using OpenPathCallback = bool (*)(const char* path);

// True when running as WebAssembly with the DOM bridge compiled in.
bool Available();

// Lets the application hand over its own loader, so this module does not have
// to know about App. Ignored outside Emscripten builds.
void SetOpenPathCallback(OpenPathCallback callback);

// Opens the browser's file picker so individual files can be chosen. The
// selection is copied into MEMFS and the skeleton among it is handed to the
// application. This is the mode that matches the native file dialog - and the
// only one in which a single .skel can actually be clicked, because a
// directory picker greys out every file.
void OpenSkeletonFiles();

// Same, but picks a whole folder (keeping relative paths) for exports that are
// spread over sub-directories.
void OpenSkeletonFolder();

// Offers a file the application just wrote to the user as a download.
void DownloadFile(const std::string& path);

}  // namespace WebFileBridge

#endif /* ifndef __WEBFILEBRIDGE_H__ */
