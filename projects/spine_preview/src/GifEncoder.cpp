#include "GifEncoder.h"

// glReadPixels returns bottom-up rows, GIF wants top-down rows.
#define GIF_FLIP_VERT
#include <gif.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

struct GifEncoder::Impl {
    GifWriter writer;
    bool open = false;
    int frameDelay = 4;
    bool premultipliedAlpha = false;
    std::vector<unsigned char> scratch;
};

namespace {

// The skeleton is drawn with premultiplied alpha, so glReadPixels hands back
// RGB that has already been multiplied by alpha. GIF needs straight (unassociated)
// colours, otherwise every soft edge turns into a dark fringe once the frame is
// composited over something else.
void Unpremultiply(unsigned char* rgba, size_t pixelCount) {
    for (size_t i = 0; i < pixelCount; ++i) {
        unsigned char* pixel = rgba + i * 4;
        const int alpha = pixel[3];

        if (alpha <= 0) {
            pixel[0] = pixel[1] = pixel[2] = 0;
            continue;
        }

        if (alpha >= 255) {
            continue;
        }

        for (int c = 0; c < 3; ++c) {
            const int value = (static_cast<int>(pixel[c]) * 255 + alpha / 2) / alpha;
            pixel[c] = static_cast<unsigned char>(value > 255 ? 255 : value);
        }
    }
}

} // namespace

GifEncoder::GifEncoder() = default;

GifEncoder::~GifEncoder() {
    End();
}

bool GifEncoder::Begin(const std::string& path, const GifExportSettings& settings) {
    End();

    impl_ = new Impl();

    impl_->frameDelay = settings.fps > 0 ? 100 / settings.fps : 10;

    if (impl_->frameDelay < 2) {
        impl_->frameDelay = 2;
    }

    impl_->premultipliedAlpha = settings.premultipliedAlpha;

    // Passing a threshold of 0 tells gif.h to ignore alpha completely, which
    // keeps the cheaper delta encoding for fully opaque exports.
    const int alphaThreshold =
        settings.transparent ? std::max(1, std::min(255, settings.alphaThreshold))
                             : 0;

    if (!GifBegin(&impl_->writer, path.c_str(),
                  static_cast<uint32_t>(settings.width),
                  static_cast<uint32_t>(settings.height),
                  static_cast<uint32_t>(impl_->frameDelay), 8, false,
                  alphaThreshold)) {
        error_ = "Could not create file: " + path;
        delete impl_;
        impl_ = nullptr;
        return false;
    }

    impl_->open = true;
    error_.clear();

    return true;
}

bool GifEncoder::WriteFrame(const unsigned char* rgba, int width, int height) {
    if (impl_ == nullptr || !impl_->open) {
        return false;
    }

    const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
    impl_->scratch.resize(pixelCount * 4);
    std::memcpy(impl_->scratch.data(), rgba, pixelCount * 4);

    if (impl_->premultipliedAlpha) {
        Unpremultiply(impl_->scratch.data(), pixelCount);
    }

    if (!GifWriteFrame(&impl_->writer, impl_->scratch.data(),
                       static_cast<uint32_t>(width), static_cast<uint32_t>(height),
                       static_cast<uint32_t>(impl_->frameDelay))) {
        error_ = "Failed to write GIF frame";
        return false;
    }

    return true;
}

bool GifEncoder::End() {
    if (impl_ == nullptr) {
        return true;
    }

    if (impl_->open) {
        GifEnd(&impl_->writer);
    }

    delete impl_;
    impl_ = nullptr;

    return true;
}

bool GifEncoder::IsOpen() const {
    return impl_ != nullptr && impl_->open;
}

const std::string& GifEncoder::GetError() const {
    return error_;
}
