#ifndef __SPINEASSET_H__
#define __SPINEASSET_H__

#include <filesystem>
#include <string>
#include <vector>

#include "SpineRuntime.h"

// Paths that make up a loaded Spine asset.
struct SpineAssetPaths {
    std::filesystem::path atlas;
    std::filesystem::path skeleton;
    std::filesystem::path directory;
};

// Owns an atlas plus skeleton data, backed by whichever spine runtime matches
// the version the asset was exported with.
class SpineAsset {
public:
    ~SpineAsset();

    // Loads a .json / .skel file and looks for the matching .atlas next to it.
    bool Load(const std::filesystem::path& skeletonPath);

    // Loads a .json / .skel file with an explicitly chosen .atlas file.
    bool Load(const std::filesystem::path& skeletonPath,
              const std::filesystem::path& atlasPath);

    void Unload();

    bool IsLoaded() const;

    const std::vector<std::string>& GetAnimations() const;

    const SpineAssetPaths& GetPaths() const;

    const std::string& GetError() const;

    // True when the atlas was exported with premultiplied alpha.
    bool UsesPremultipliedAlpha() const;

    // Setup pose bounds in skeleton coordinates, used to frame the camera.
    void GetBounds(float& x, float& y, float& width, float& height) const;

    // "4.2" or "3.8" - the runtime that is actually driving this asset.
    const char* GetRuntimeVersion() const;

    ISpineRuntime* GetRuntime();

private:
    void ReleaseRuntime();

    ISpineRuntime* runtime_ = nullptr;

    SpineAssetPaths paths_;
    std::string error_;
};

#endif /* ifndef __SPINEASSET_H__ */
