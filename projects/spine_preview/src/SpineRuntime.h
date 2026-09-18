#ifndef __SPINERUNTIME_H__
#define __SPINERUNTIME_H__

#include <filesystem>
#include <string>
#include <vector>

// A texture owned by an atlas page. `id` is an OpenGL texture name.
struct SpineTexture {
    unsigned int id = 0;
    int width = 0;
    int height = 0;
};

// Mirrors spine::BlendMode so that callers do not need the runtime headers.
enum SpineBlendMode {
    SpineBlend_Normal = 0,
    SpineBlend_Additive = 1,
    SpineBlend_Multiply = 2,
    SpineBlend_Screen = 3
};

// A group of triangles sharing one texture and blend mode, in skeleton space.
struct SpineDrawBatch {
    std::vector<float> positions;           // xy pairs
    std::vector<float> uvs;                 // uv pairs
    std::vector<unsigned int> colors;       // packed 0xAARRGGBB
    std::vector<unsigned short> indices;
    int blendMode = SpineBlend_Normal;
    void* texture = nullptr;                // SpineTexture*
};

// Version independent spine playback: loading, animation state and geometry.
class ISpineRuntime {
public:
    virtual ~ISpineRuntime() = default;

    // "4.2" or "3.8".
    virtual const char* GetVersionLabel() const = 0;

    virtual bool Load(const std::filesystem::path& skeletonPath,
                      const std::filesystem::path& atlasPath,
                      std::string& error) = 0;

    virtual void Unload() = 0;

    virtual const std::vector<std::string>& GetAnimations() const = 0;

    virtual void GetBounds(float& x, float& y, float& width,
                           float& height) const = 0;

    virtual bool UsesPremultipliedAlpha() const = 0;

    // Resets the track to time 0 and applies the pose.
    virtual void SetAnimation(const std::string& name, bool loop) = 0;

    virtual void Update(float deltaTime) = 0;

    virtual void Seek(float time) = 0;

    virtual float GetTime() const = 0;

    virtual float GetDuration() const = 0;

    // Rebuilds the geometry for the current pose. Valid until the next call.
    virtual const std::vector<SpineDrawBatch>& BuildDrawBatches() = 0;
};

ISpineRuntime* CreateSpineRuntime42();

ISpineRuntime* CreateSpineRuntime38();

// ---------------------------------------------------------------------------
// Asset version detection
// ---------------------------------------------------------------------------

enum class SpineAssetVersion {
    Unknown,
    V38,
    V42
};

// Reads the Spine version out of a .skel or .json file without parsing it.
std::string ReadSkeletonVersionString(const std::filesystem::path& path);

SpineAssetVersion DetectSkeletonVersion(const std::filesystem::path& path);

#endif /* ifndef __SPINERUNTIME_H__ */
