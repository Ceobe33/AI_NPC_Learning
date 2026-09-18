#ifndef __SPINERENDERER_H__
#define __SPINERENDERER_H__

#include <string>

#include "SpineAsset.h"
#include "SpineRuntime.h"

// An offscreen colour buffer that Spine can be rendered into.
// The result is available as an OpenGL texture (preview) or via glReadPixels
// (GIF export).
class RenderTarget {
public:
    ~RenderTarget();

    bool Resize(int width, int height);

    void Release();

    void Bind();

    void Unbind();

    unsigned int GetTexture() const;

    int GetWidth() const;

    int GetHeight() const;

private:
    unsigned int framebuffer_ = 0;
    unsigned int texture_ = 0;
    int width_ = 0;
    int height_ = 0;

    int previousFramebuffer_ = 0;
    int previousViewport_[4] = {0, 0, 0, 0};
};

// Camera and canvas settings for a single Draw() call.
struct SpineView {
    // Pan, in skeleton units, applied on top of the fitted camera.
    float offsetX = 0.0f;
    float offsetY = 0.0f;

    float zoom = 1.0f;

    // Fraction of the canvas kept free around the skeleton when fitting.
    float padding = 0.12f;

    // Draw the grey/white checkerboard instead of a flat clear colour.
    bool grid = true;

    // Checkerboard cell size in pixels at zoom 1. The cell therefore keeps a
    // constant size in skeleton units and scales with the zoom, like Spine's
    // viewport grid.
    float gridSize = 32.0f;
};

// Plays back a loaded Spine asset and renders it with OpenGL.
class SpinePlayer {
public:
    ~SpinePlayer();

    bool Load(SpineAsset& asset);

    void Unload();

    bool IsLoaded() const;

    void Play();

    void Pause();

    void Reset();

    bool IsPlaying() const;

    void SetAnimation(const std::string& name, bool loop = true);

    const std::string& GetAnimation() const;

    void SetSpeed(float speed);

    float GetSpeed() const;

    // Advances the animation. Call once per frame.
    void Update(float deltaTime);

    // Jumps to an absolute time in seconds.
    void Seek(float time);

    float GetTime() const;

    float GetDuration() const;

    // Renders into the framebuffer that is currently bound, using the full
    // (0, 0, width, height) viewport and a camera fitted to the skeleton.
    void Draw(int width, int height, float background[4],
              const SpineView& view = SpineView());

    // Reports the camera Draw() would use: the centre of the skeleton bounds
    // (before the view offset) and the pixels-per-skeleton-unit scale. Lets
    // the UI convert between screen and skeleton space.
    bool GetCamera(int width, int height, const SpineView& view,
                   float& boundsCenterX, float& boundsCenterY,
                   float& scale) const;

private:
    void CreateGLObjects();

    void DestroyGLObjects();

    void DrawGrid(int width, int height, float centerX, float centerY,
                  float scale, const SpineView& view);

    SpineAsset* asset_ = nullptr;
    std::string animation_;

    bool playing_ = false;
    float speed_ = 1.0f;

    unsigned int program_ = 0;
    unsigned int gridProgram_ = 0;
    unsigned int gridVertexArray_ = 0;
    unsigned int vertexArray_ = 0;
    unsigned int vertexBuffer_ = 0;
    unsigned int indexBuffer_ = 0;
    int vertexCapacity_ = 0;
    int indexCapacity_ = 0;
};

#endif /* ifndef __SPINERENDERER_H__ */
