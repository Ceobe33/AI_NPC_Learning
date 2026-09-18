#include "SpineRuntime.h"

#include <algorithm>
#include <fstream>
#include <vector>

namespace {

std::string ToLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

bool IsVersionChar(char c) {
    return (c >= '0' && c <= '9') || c == '.';
}

// Binary skeletons store the version as a length prefixed string near the
// start of the file (3.8) or right after a 32 bit hash (4.x), so scanning the
// first bytes for a dotted number is enough to tell them apart.
std::string ScanBinaryVersion(const std::string& bytes) {
    const size_t limit = bytes.size() < 96 ? bytes.size() : 96;

    for (size_t i = 0; i < limit; ++i) {
        if (bytes[i] < '0' || bytes[i] > '9') {
            continue;
        }

        size_t end = i;

        while (end < limit && IsVersionChar(bytes[end])) {
            ++end;
        }

        std::string candidate = bytes.substr(i, end - i);

        if (candidate.find('.') != std::string::npos && candidate.size() >= 3) {
            return candidate;
        }
    }

    return {};
}

std::string ScanJsonVersion(const std::string& text) {
    const std::string key = "spine";
    size_t position = 0;

    while ((position = text.find('"' + key + '"', position)) != std::string::npos) {
        size_t index = position + key.size() + 2;
        size_t end = text.size();

        while (index < end && text[index] != '}') {
            ++index;
        }

        const std::string fragment = text.substr(position, index - position);
        const size_t colon = fragment.find(':');

        if (colon == std::string::npos) {
            position += key.size();
            continue;
        }

        size_t start = fragment.find_first_of("\"", colon);

        if (start == std::string::npos) {
            position += key.size();
            continue;
        }

        const size_t stop = fragment.find('"', start + 1);

        if (stop == std::string::npos) {
            position += key.size();
            continue;
        }

        const std::string version = fragment.substr(start + 1, stop - start - 1);

        if (!version.empty() && version.find('.') != std::string::npos) {
            return version;
        }

        position += key.size();
    }

    return {};
}

} // namespace

std::string ReadSkeletonVersionString(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);

    if (!file) {
        return {};
    }

    const std::string extension = ToLower(path.extension().string());

    if (extension == ".json") {
        std::string text((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());

        return ScanJsonVersion(text);
    }

    std::string bytes(96, '\0');
    file.read(&bytes[0], static_cast<std::streamsize>(bytes.size()));
    bytes.resize(static_cast<size_t>(file.gcount()));

    return ScanBinaryVersion(bytes);
}

SpineAssetVersion DetectSkeletonVersion(const std::filesystem::path& path) {
    const std::string version = ReadSkeletonVersionString(path);

    if (version.empty()) {
        return SpineAssetVersion::Unknown;
    }

    if (version.rfind("3.", 0) == 0) {
        return SpineAssetVersion::V38;
    }

    if (version.rfind("4.", 0) == 0) {
        return SpineAssetVersion::V42;
    }

    return SpineAssetVersion::Unknown;
}
