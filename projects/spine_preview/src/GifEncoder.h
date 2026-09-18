#ifndef __GIFENCODER_H__
#define __GIFENCODER_H__

#include <string>

struct GifExportSettings {
    int width = 512;
    int height = 512;
    int fps = 30;

    bool transparent = false;

    // GIF has no partial transparency, so the alpha of every pixel is snapped
    // to 0 or 255 at this cut-off (0-255).
    int alphaThreshold = 128;

    // Set from the loaded skeleton right before the export starts.
    bool premultipliedAlpha = false;

    float background[4];
    GifExportSettings() {
        background[0] = 0.09f;
        background[1] = 0.09f;
        background[2] = 0.09f;
        background[3] = 1.0f;
    }
};

// Incremental animated GIF writer: Begin -> WriteFrame (n times) -> End.
class GifEncoder {
public:
    GifEncoder();

    ~GifEncoder();

    bool Begin(const std::string& path, const GifExportSettings& settings);

    // `rgba` is a tightly packed bottom-up RGBA buffer of settings.width x
    // settings.height pixels.
    bool WriteFrame(const unsigned char* rgba, int width, int height);

    bool End();

    bool IsOpen() const;

    const std::string& GetError() const;

private:
    struct Impl;

    Impl* impl_ = nullptr;
    std::string error_;
};

#endif /* ifndef __GIFENCODER_H__ */
