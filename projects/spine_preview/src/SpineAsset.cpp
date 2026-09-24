#include "SpineAsset.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace {

std::string ToLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::filesystem::path ReplaceExtension(const std::filesystem::path& path,
                                       const char* extension) {
    std::filesystem::path result = path;
    result.replace_extension(extension);
    return result;
}

// Drops the export-variant suffixes Spine appends to file names, so that
// "spineboy-pro.skel" and "spineboy-pma.atlas" both reduce to "spineboy".
std::string StripVariantSuffixes(std::string stem) {
    static const char* const variants[] = {"-pro", "-ess", "-pma"};
    bool stripped = true;

    while (stripped) {
        stripped = false;

        for (const char* variant : variants) {
            const std::size_t length = std::strlen(variant);

            if (stem.size() > length &&
                ToLower(stem.substr(stem.size() - length)) == variant) {
                stem.erase(stem.size() - length);
                stripped = true;
            }
        }
    }

    return stem;
}

// Looks for a .atlas file that belongs to the given skeleton file.
std::filesystem::path FindAtlasFor(const std::filesystem::path& skeletonPath) {
    const std::filesystem::path candidate =
        ReplaceExtension(skeletonPath, ".atlas");

    if (std::filesystem::exists(candidate)) {
        return candidate;
    }

    std::error_code error;
    const std::filesystem::path directory = skeletonPath.parent_path();
    const std::string wanted =
        ToLower(StripVariantSuffixes(skeletonPath.stem().string()));

    std::filesystem::path fallback;

    // A folder often holds several atlases (name.atlas, name-pma.atlas, ...),
    // so prefer one that belongs to the same project instead of taking
    // whichever one comes first.
    for (const auto& entry : std::filesystem::directory_iterator(directory, error)) {
        if (!entry.is_regular_file() ||
            ToLower(entry.path().extension().string()) != ".atlas") {
            continue;
        }

        if (fallback.empty()) {
            fallback = entry.path();
        }

        if (ToLower(StripVariantSuffixes(entry.path().stem().string())) == wanted) {
            return entry.path();
        }
    }

    return fallback;
}

} // namespace

SpineAsset::~SpineAsset() {
    Unload();
}

bool SpineAsset::Load(const std::filesystem::path& skeletonPath) {
    return Load(skeletonPath, FindAtlasFor(skeletonPath));
}

bool SpineAsset::Load(const std::filesystem::path& skeletonPath,
                      const std::filesystem::path& atlasPath) {
    error_.clear();

    const std::string extension = ToLower(skeletonPath.extension().string());

    if (extension != ".json" && extension != ".skel") {
        error_ = "Unsupported skeleton file: " + skeletonPath.string();
        return false;
    }

    if (atlasPath.empty() || !std::filesystem::exists(atlasPath)) {
        error_ = "No .atlas file found next to " + skeletonPath.string();
        return false;
    }

    // The vendored binary parsers read without bounds checks - SkeletonBinary
    // is literally *cursor++ - so a truncated or mislabelled .skel file would
    // run past the buffer and crash instead of failing cleanly. Every Spine
    // binary export carries its dotted version string within the first bytes,
    // so a file without one never reaches the parser.
    if (extension == ".skel" && ReadSkeletonVersionString(skeletonPath).empty()) {
        error_ = "Not a valid Spine binary skeleton: no version header found "
                 "near the start of " + skeletonPath.string();
        return false;
    }

    Unload();

    // Prefer the runtime matching the version the asset was exported with and
    // fall back to the other one only when detection is inconclusive: feeding
    // a 4.2 atlas to the 3.8 parser (or the other way round) trips internal
    // assertions inside the runtime.
    const SpineAssetVersion detected = DetectSkeletonVersion(skeletonPath);
    const bool prefer38 = detected == SpineAssetVersion::V38;

    ISpineRuntime* candidates[2] = {
        prefer38 ? CreateSpineRuntime38() : CreateSpineRuntime42(), nullptr};

    int candidateCount = 1;

    if (detected == SpineAssetVersion::Unknown) {
        candidates[1] = prefer38 ? CreateSpineRuntime42() : CreateSpineRuntime38();
        candidateCount = 2;
    }

    std::string firstError;

    for (int i = 0; i < candidateCount; ++i) {
        ISpineRuntime* candidate = candidates[i];
        std::string runtimeError;

        if (candidate->Load(skeletonPath, atlasPath, runtimeError)) {
            // The other candidate has only been freed if it was already tried
            // (i.e. it came before this one and failed).
            if (i == 0 && candidates[1] != nullptr) {
                delete candidates[1];
            }

            runtime_ = candidate;
            paths_.skeleton = skeletonPath;
            paths_.atlas = atlasPath;
            paths_.directory = atlasPath.parent_path();

            return true;
        }

        if (firstError.empty()) {
            firstError = runtimeError;
        }

        candidate->Unload();
        delete candidate;
    }

    error_ = firstError;

    const std::string fileVersion = ReadSkeletonVersionString(skeletonPath);

    if (!fileVersion.empty()) {
        error_ += " [asset exported with Spine " + fileVersion + "]";
    }

    return false;
}

void SpineAsset::Unload() {
    ReleaseRuntime();

    paths_ = SpineAssetPaths();
}

void SpineAsset::ReleaseRuntime() {
    if (runtime_ != nullptr) {
        delete runtime_;
        runtime_ = nullptr;
    }
}

bool SpineAsset::IsLoaded() const {
    return runtime_ != nullptr;
}

const std::vector<std::string>& SpineAsset::GetAnimations() const {
    static const std::vector<std::string> empty;

    return runtime_ != nullptr ? runtime_->GetAnimations() : empty;
}

const SpineAssetPaths& SpineAsset::GetPaths() const {
    return paths_;
}

const std::string& SpineAsset::GetError() const {
    return error_;
}

bool SpineAsset::UsesPremultipliedAlpha() const {
    return runtime_ != nullptr && runtime_->UsesPremultipliedAlpha();
}

void SpineAsset::GetBounds(float& x, float& y, float& width, float& height) const {
    if (runtime_ != nullptr) {
        runtime_->GetBounds(x, y, width, height);
        return;
    }

    x = 0.0f;
    y = 0.0f;
    width = 0.0f;
    height = 0.0f;
}

const char* SpineAsset::GetRuntimeVersion() const {
    return runtime_ != nullptr ? runtime_->GetVersionLabel() : "-";
}

ISpineRuntime* SpineAsset::GetRuntime() {
    return runtime_;
}
