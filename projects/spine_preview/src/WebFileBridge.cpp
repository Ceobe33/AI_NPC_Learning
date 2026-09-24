#include "WebFileBridge.h"

#if defined(__EMSCRIPTEN__)

#include <emscripten.h>

namespace {

WebFileBridge::OpenPathCallback gOpenPath = nullptr;

}  // namespace

// Called from JavaScript once the picked files have been written to MEMFS.
// Keeping it in this module means App does not have to be visible to the JS
// side, and it survives the linker stripping everything else it cannot reach.
extern "C" EMSCRIPTEN_KEEPALIVE int spine_studio_open_path(const char* path) {
    if (gOpenPath == nullptr) {
        return 0;
    }

    return gOpenPath(path) ? 1 : 0;
}

// One picker, two modes. Directory mode keeps relative paths intact, which a
// nested export needs because the skeleton resolves its atlas and page images
// by relative path; file mode is the one that mirrors the native dialog and is
// the only mode where a lone .skel is actually clickable - a directory picker
// greys out every file in the sheet.
EM_JS(void, spine_studio_pick_assets, (int directoryMode), {
    const useDirectory = directoryMode ? true : false;
    const inputId = useDirectory ? 'spine-studio-folder-input'
                                 : 'spine-studio-file-input';
    let input = document.getElementById(inputId);

    if (!input) {
        input = document.createElement('input');
        input.id = inputId;
        input.type = 'file';
        input.multiple = true;

        if (useDirectory) {
            input.webkitdirectory = true;
        } else {
            // Listing the extensions an export is made of keeps the sheet from
            // filtering out the .skel the user is after.
            input.accept = ".json,.skel,.atlas,.png,.jpg,.jpeg,.webp";
        }

        input.style.display = 'none';

        input.addEventListener('change', async () => {
            const files = Array.from(input.files || []);
            if (files.length === 0) {
                return;
            }

            const base = '/spine-assets';
            const written = [];

            for (const file of files) {
                // Chromium hands over "folder/sub/file", Firefox drops the
                // path and gives just the file name. Either way the layout has
                // to be rebuilt inside MEMFS.
                const relative = file.webkitRelativePath || file.name;
                const parts = relative.split('/').filter(p => p.length > 0);
                if (parts.length === 0) {
                    continue;
                }

                const target = base + '/' + parts.join('/');
                const slash = target.lastIndexOf('/');
                if (slash > 0) {
                    FS.mkdirTree(target.substring(0, slash));
                }

                const file_data = new Uint8Array(await file.arrayBuffer());
                FS.writeFile(target, file_data);

                const name = parts[parts.length - 1];
                const dot = name.lastIndexOf('.');
                // Double quotes, not single: EM_ASM bodies are still run
                // through the C preprocessor, where '' is an empty char.
                const extension =
                    dot < 0 ? "" : name.substring(dot + 1).toLowerCase();
                const stem =
                    dot < 0 ? name : name.substring(0, dot).toLowerCase();

                written.push({
                    path: target,
                    extension: extension,
                    stem: stem
                });
            }

            // A folder can hold several skeletons (a .skel plus the .json it
            // was exported from, or test leftovers), so prefer the one whose
            // name matches the atlas - that is the pair Spine actually writes.
            let skeleton = null;
            let atlasStem = null;

            for (const entry of written) {
                if (entry.extension === 'atlas' && atlasStem === null) {
                    atlasStem = entry.stem;
                }
            }

            if (atlasStem !== null) {
                const match = written.find(
                    entry => entry.stem === atlasStem &&
                             (entry.extension === 'skel' ||
                              entry.extension === 'json'));
                if (match) {
                    skeleton = match.path;
                }
            }

            if (skeleton === null) {
                const binary = written.find(
                    entry => entry.extension === 'skel');
                if (binary) {
                    skeleton = binary.path;
                }
            }

            if (skeleton === null) {
                const text = written.find(entry => entry.extension === 'json');
                if (text) {
                    skeleton = text.path;
                }
            }

            if (skeleton === null) {
                console.warn(
                    'Spine Animation Studio: no .json or .skel file among the ' +
                    'selected assets.');
                return;
            }

            Module.ccall('spine_studio_open_path', 'number', ['string'],
                         [skeleton]);
        });

        document.body.appendChild(input);
    }

    // Clearing the value first lets the same folder be picked twice.
    input.value = "";
    input.click();
});

// Exported GIFs live in MEMFS, which disappears with the page. Handing the
// bytes to an <a download> is the only way for the user to actually get them.
EM_JS(void, spine_studio_download, (const char* path), {
    const target = UTF8ToString(path);

    try {
        const file_data = FS.readFile(target);
        const download = target.substring(target.lastIndexOf('/') + 1);
        const url = URL.createObjectURL(
            new Blob([file_data], {type: 'application/octet-stream'}));

        const anchor = document.createElement('a');
        anchor.href = url;
        anchor.download = download;
        document.body.appendChild(anchor);
        anchor.click();
        document.body.removeChild(anchor);

        setTimeout(() => URL.revokeObjectURL(url), 0);
    } catch (error) {
        console.error('Spine Animation Studio: could not offer ' + target +
                      ' for download.', error);
    }
});

namespace WebFileBridge {

bool Available() {
    return true;
}

void SetOpenPathCallback(OpenPathCallback callback) {
    gOpenPath = callback;
}

void OpenSkeletonFiles() {
    spine_studio_pick_assets(0);
}

void OpenSkeletonFolder() {
    spine_studio_pick_assets(1);
}

void DownloadFile(const std::string& path) {
    spine_studio_download(path.c_str());
}

}  // namespace WebFileBridge

#else  // !defined(__EMSCRIPTEN__)

// Native builds never compile this file entry point into anything meaningful;
// the stubs let the rest of the code stay free of build guards.
namespace WebFileBridge {

bool Available() {
    return false;
}

void SetOpenPathCallback(OpenPathCallback) {}

void OpenSkeletonFiles() {}

void OpenSkeletonFolder() {}

void DownloadFile(const std::string&) {}

}  // namespace WebFileBridge

#endif
