// Spine runtime implementation.
//
// This file is compiled twice: once against the 4.2 runtime and once against
// the 3.8 runtime (which lives in the spine38 namespace). Everything below is
// written against the `sp` namespace alias, with #if blocks only where the two
// runtimes genuinely disagree.
#include "SpineRuntime.h"

#include "PlatformGL.h"

#if defined(SPINE_RUNTIME_38)
#  include <spine38/spine.h>
#else
#  include <spine/spine.h>
#endif

#include <stb_image.h>

#include <algorithm>
#include <fstream>
#include <utility>

#if defined(SPINE_RUNTIME_38)
namespace sp = spine38;
#else
namespace sp = spine;
#endif

// spine-cpp expects the host application to provide the default extension.
#if defined(SPINE_RUNTIME_38)
namespace spine38 {
#else
namespace spine {
#endif

// Upstream's DefaultSpineExtension refuses to read files on Emscripten
// (spine-cpp 4.2's Extension.cpp: _readFile is guarded by
// #ifndef __EMSCRIPTEN__ and returns nullptr), which silently breaks the 4.2
// atlas and skeleton loaders in the browser. stdio works perfectly well
// against the MEMFS the web file bridge writes into, so this extension reads
// the same way on every host. The 3.8 runtime has no such guard and always
// used fopen, so on the desktop this behaves exactly like upstream.
class StudioSpineExtension : public DefaultSpineExtension {
protected:
    char* _readFile(const String& path, int* length) override {
        if (length == nullptr) {
            return nullptr;
        }

        FILE* file = fopen(path.buffer(), "rb");

        if (file == nullptr) {
            *length = 0;
            return nullptr;
        }

        fseek(file, 0, SEEK_END);
        *length = static_cast<int>(ftell(file));
        fseek(file, 0, SEEK_SET);

        char* data = SpineExtension::alloc<char>(*length, __FILE__, __LINE__);
        const size_t read = fread(data, 1, static_cast<size_t>(*length), file);
        fclose(file);

        // Trust the bytes actually read, not the length ftell promised.
        *length = static_cast<int>(read);

        return data;
    }
};

SpineExtension* getDefaultExtension() {
    return new StudioSpineExtension();
}
}

namespace {

std::string ToLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::string Trim(const std::string& value) {
    const size_t first = value.find_first_not_of(" \t\r\n");

    if (first == std::string::npos) {
        return {};
    }

    const size_t last = value.find_last_not_of(" \t\r\n");

    return value.substr(first, last - first + 1);
}

void PremultiplyAlpha(unsigned char* pixels, int pixelCount) {
    for (int i = 0; i < pixelCount; ++i) {
        unsigned char* pixel = pixels + i * 4;
        const int alpha = pixel[3];
        pixel[0] = static_cast<unsigned char>(pixel[0] * alpha / 255);
        pixel[1] = static_cast<unsigned char>(pixel[1] * alpha / 255);
        pixel[2] = static_cast<unsigned char>(pixel[2] * alpha / 255);
    }
}

// The 3.8 runtime does not expose AtlasPage::pma and Spine 3.8 did not write a
// "pma" line into the atlas, so fall back to the "-pma" naming convention that
// the Spine exports use (e.g. spineboy-pma.atlas).
bool AtlasUsesPremultipliedAlpha(const std::filesystem::path& atlasPath) {
    const std::string stem = ToLower(atlasPath.stem().string());

    if (stem.find("pma") != std::string::npos) {
        return true;
    }

    std::ifstream file(atlasPath);
    std::string line;

    while (std::getline(file, line)) {
        const std::string trimmed = Trim(line);

        if (trimmed.rfind("pma:", 0) == 0) {
            return trimmed.find("true") != std::string::npos;
        }
    }

    return false;
}

unsigned int PackColor(const sp::Color& skeletonColor, const sp::Color& slotColor,
                       const sp::Color& attachmentColor) {
    const auto r = static_cast<unsigned char>(
        skeletonColor.r * slotColor.r * attachmentColor.r * 255);
    const auto g = static_cast<unsigned char>(
        skeletonColor.g * slotColor.g * attachmentColor.g * 255);
    const auto b = static_cast<unsigned char>(
        skeletonColor.b * slotColor.b * attachmentColor.b * 255);
    const auto a = static_cast<unsigned char>(
        skeletonColor.a * slotColor.a * attachmentColor.a * 255);

    return (static_cast<unsigned int>(a) << 24) |
           (static_cast<unsigned int>(r) << 16) |
           (static_cast<unsigned int>(g) << 8) | static_cast<unsigned int>(b);
}

} // namespace

// The implementation lives in a version specific namespace: this file is
// compiled twice and both copies end up in the same binary, so the symbols
// must not collide (otherwise the linker merges the vtables and one runtime
// would answer for both).
#if defined(SPINE_RUNTIME_38)
namespace spineimpl38 {
#else
namespace spineimpl42 {
#endif

// ---------------------------------------------------------------------------
// Texture loading
// ---------------------------------------------------------------------------

class SpineTextureLoaderImpl : public sp::TextureLoader {
public:
    ~SpineTextureLoaderImpl() override {
        for (SpineTexture* texture : textures) {
            if (texture->id != 0) {
                glDeleteTextures(1, &texture->id);
            }

            delete texture;
        }

        textures.clear();
    }

    void load(sp::AtlasPage& page, const sp::String& path) override {
        int width = 0;
        int height = 0;
        int channels = 0;

        stbi_set_flip_vertically_on_load(0);

        unsigned char* pixels = stbi_load(path.buffer(), &width, &height, &channels, 4);

        if (pixels == nullptr) {
            fprintf(stderr,
                    "spine-studio: stbi_load failed for %s: %s\n", path.buffer(),
                    stbi_failure_reason());
            return;
        }

#if defined(SPINE_RUNTIME_38)
        if (AtlasUsesPremultipliedAlpha(path.buffer())) {
            PremultiplyAlpha(pixels, width * height);
        }
#else
        if (page.pma) {
            PremultiplyAlpha(pixels, width * height);
        }
#endif

        auto* texture = new SpineTexture();
        texture->width = width;
        texture->height = height;

        glGenTextures(1, &texture->id);
        glBindTexture(GL_TEXTURE_2D, texture->id);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, pixels);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);

        stbi_image_free(pixels);

        textures.push_back(texture);

#if defined(SPINE_RUNTIME_38)
        page.setRendererObject(texture);
#else
        page.texture = texture;
#endif
    }

    void unload(void* texture) override {
        auto* spineTexture = static_cast<SpineTexture*>(texture);

        if (spineTexture == nullptr) {
            return;
        }

        if (spineTexture->id != 0) {
            glDeleteTextures(1, &spineTexture->id);
            spineTexture->id = 0;
        }
    }

    std::vector<SpineTexture*> textures;
};

// ---------------------------------------------------------------------------
// Runtime implementation
// ---------------------------------------------------------------------------

class SpineRuntimeImpl : public ISpineRuntime {
public:
    SpineRuntimeImpl() {
        const unsigned short quad[6] = {0, 1, 2, 2, 3, 0};

        for (int i = 0; i < 6; ++i) {
            quadIndices_.add(quad[i]);
        }
    }

    ~SpineRuntimeImpl() override {
        Unload();
    }

    const char* GetVersionLabel() const override {
#if defined(SPINE_RUNTIME_38)
        return "3.8";
#else
        return "4.2";
#endif
    }

    bool Load(const std::filesystem::path& skeletonPath,
              const std::filesystem::path& atlasPath,
              std::string& error) override {
        Unload();

        const std::string path = skeletonPath.string();
        const std::string extension = ToLower(skeletonPath.extension().string());

        if (extension != ".json" && extension != ".skel") {
            error = "Unsupported skeleton file: " + path;
            return false;
        }

        atlas_ = new (__FILE__, __LINE__) sp::Atlas(atlasPath.string().c_str(),
                                                    &textureLoader_);

#if defined(SPINE_RUNTIME_38)
        const bool pageLoaded =
            atlas_->getPages()[0]->getRendererObject() != nullptr;
#else
        const bool pageLoaded = atlas_->getPages()[0]->texture != nullptr;
#endif

        if (atlas_->getPages().size() == 0 || !pageLoaded) {
            error = "Failed to read atlas (or its PNG): " + atlasPath.string();

            // Without this the failure is indistinguishable from a silent
            // no-op, especially in the browser where stderr is the console.
            fprintf(stderr,
                    "spine-studio: atlas load failed: %s\n"
                    "  pages parsed: %zu, pageLoaded: %s\n",
                    atlasPath.string().c_str(), atlas_->getPages().size(),
                    pageLoaded ? "yes" : "no");

            Unload();
            return false;
        }

#if defined(SPINE_RUNTIME_38)
        premultipliedAlpha_ = AtlasUsesPremultipliedAlpha(atlasPath);
#else
        premultipliedAlpha_ = atlas_->getPages()[0]->pma;
#endif

        sp::SkeletonData* skeletonData = nullptr;
        std::string runtimeError;

        if (extension == ".skel") {
            sp::SkeletonBinary binary(atlas_);
            skeletonData = binary.readSkeletonDataFile(path.c_str());
            runtimeError = binary.getError().isEmpty() ? std::string()
                                                       : binary.getError().buffer();
        } else {
            sp::SkeletonJson json(atlas_);
            skeletonData = json.readSkeletonDataFile(path.c_str());
            runtimeError = json.getError().isEmpty() ? std::string()
                                                     : json.getError().buffer();
        }

        if (skeletonData == nullptr) {
            error = runtimeError.empty()
                        ? "Failed to read skeleton: " + path
                        : "Failed to read skeleton: " + runtimeError;
            Unload();
            return false;
        }

        skeletonData_ = skeletonData;
        skeleton_ = new (__FILE__, __LINE__) sp::Skeleton(skeletonData_);
        animationStateData_ =
            new (__FILE__, __LINE__) sp::AnimationStateData(skeletonData_);
        animationStateData_->setDefaultMix(0.2f);
        animationState_ =
            new (__FILE__, __LINE__) sp::AnimationState(animationStateData_);

        animations_.clear();

        auto& animations = skeletonData_->getAnimations();

        for (size_t i = 0; i < animations.size(); ++i) {
            animations_.push_back(animations[i]->getName().buffer());
        }

        return true;
    }

    void Unload() override {
        animations_.clear();
        batches_.clear();
        premultipliedAlpha_ = false;

        if (animationState_ != nullptr) {
            delete animationState_;
            animationState_ = nullptr;
        }

        if (animationStateData_ != nullptr) {
            delete animationStateData_;
            animationStateData_ = nullptr;
        }

        if (skeleton_ != nullptr) {
            delete skeleton_;
            skeleton_ = nullptr;
        }

        if (skeletonData_ != nullptr) {
            delete skeletonData_;
            skeletonData_ = nullptr;
        }

        if (atlas_ != nullptr) {
            delete atlas_;
            atlas_ = nullptr;
        }
    }

    const std::vector<std::string>& GetAnimations() const override {
        return animations_;
    }

    void GetBounds(float& x, float& y, float& width, float& height) const override {
        if (skeletonData_ == nullptr) {
            x = 0.0f;
            y = 0.0f;
            width = 0.0f;
            height = 0.0f;
            return;
        }

        x = skeletonData_->getX();
        y = skeletonData_->getY();
        width = skeletonData_->getWidth();
        height = skeletonData_->getHeight();
    }

    bool UsesPremultipliedAlpha() const override {
        return premultipliedAlpha_;
    }

    void SetAnimation(const std::string& name, bool loop) override {
        if (animationState_ == nullptr || name.empty()) {
            return;
        }

        animationState_->setAnimation(0, name.c_str(), loop);
        Seek(0.0f);
    }

    void Update(float deltaTime) override {
        if (skeleton_ == nullptr) {
            return;
        }

        skeleton_->update(deltaTime);
        animationState_->update(deltaTime);

        ApplyPose();
    }

    void Seek(float time) override {
        if (animationState_ == nullptr) {
            return;
        }

        sp::TrackEntry* entry = animationState_->getCurrent(0);

        if (entry == nullptr) {
            return;
        }

        entry->setTrackTime(time);

        // A track that is still crossfading mixes with alpha
        // mixTime / mixDuration, which is 0 until update() has advanced it.
        // Seeking has to land on the target animation's pose exactly, so
        // finish (or skip) the crossfade before applying.
        for (sp::TrackEntry* track = entry; track != nullptr;
             track = track->getMixingFrom()) {
            if (track->getMixDuration() > 0.0f) {
                track->setMixTime(track->getMixDuration());
            } else {
                track->setMixDuration(0.0f);
            }
        }

#if defined(SPINE_RUNTIME_38)
        // The 3.8 runtime only refreshes a track entry's bookkeeping (and
        // retires a completed crossfade) inside update(), so a bare
        // setTrackTime + apply would keep re-applying the pose from before
        // the seek.
        animationState_->update(0.0f);
#endif

        ApplyPose();
    }

    float GetTime() const override {
        if (animationState_ == nullptr) {
            return 0.0f;
        }

        sp::TrackEntry* entry = animationState_->getCurrent(0);

        return entry != nullptr ? entry->getTrackTime() : 0.0f;
    }

    float GetDuration() const override {
        if (animationState_ == nullptr) {
            return 0.0f;
        }

        sp::TrackEntry* entry = animationState_->getCurrent(0);

        if (entry == nullptr || entry->getAnimation() == nullptr) {
            return 0.0f;
        }

        return entry->getAnimation()->getDuration();
    }

    const std::vector<SpineDrawBatch>& BuildDrawBatches() override {
        batches_.clear();

        if (skeleton_ == nullptr) {
            return batches_;
        }

        auto& drawOrder = skeleton_->getDrawOrder();

        for (size_t i = 0; i < drawOrder.size(); ++i) {
            sp::Slot* slot = drawOrder[i];

            if (slot == nullptr) {
                continue;
            }

            sp::Attachment* attachment = slot->getAttachment();

            if (attachment == nullptr) {
                clipper_.clipEnd(*slot);
                continue;
            }

            sp::Color* attachmentColor = nullptr;
            sp::Vector<float>* vertices = &worldVertices_;
            sp::Vector<float>* uvs = nullptr;
            sp::Vector<unsigned short>* indices = nullptr;
            void* texture = nullptr;
            int vertexCount = 0;

            if (attachment->getRTTI().isExactly(sp::RegionAttachment::rtti)) {
                auto* region = static_cast<sp::RegionAttachment*>(attachment);
                attachmentColor = &region->getColor();

                if (attachmentColor->a == 0) {
                    clipper_.clipEnd(*slot);
                    continue;
                }

                worldVertices_.setSize(8, 0);

#if defined(SPINE_RUNTIME_38)
                region->computeWorldVertices(slot->getBone(), worldVertices_, 0, 2);
                auto* atlasRegion =
                    static_cast<sp::AtlasRegion*>(region->getRendererObject());
                texture = atlasRegion != nullptr && atlasRegion->page != nullptr
                              ? atlasRegion->page->getRendererObject()
                              : nullptr;
#else
                region->computeWorldVertices(*slot, worldVertices_, 0, 2);
                texture = region->getRegion() != nullptr
                              ? region->getRegion()->rendererObject
                              : nullptr;
#endif

                vertexCount = 4;
                uvs = &region->getUVs();
                indices = &quadIndices_;
            } else if (attachment->getRTTI().isExactly(sp::MeshAttachment::rtti)) {
                auto* mesh = static_cast<sp::MeshAttachment*>(attachment);
                attachmentColor = &mesh->getColor();

                if (attachmentColor->a == 0) {
                    clipper_.clipEnd(*slot);
                    continue;
                }

                const size_t length = mesh->getWorldVerticesLength();
                worldVertices_.setSize(length, 0);
                mesh->computeWorldVertices(*slot, 0, length,
                                           worldVertices_.buffer(), 0, 2);

#if defined(SPINE_RUNTIME_38)
                auto* atlasRegion =
                    static_cast<sp::AtlasRegion*>(mesh->getRendererObject());
                texture = atlasRegion != nullptr && atlasRegion->page != nullptr
                              ? atlasRegion->page->getRendererObject()
                              : nullptr;
#else
                texture = mesh->getRegion() != nullptr
                              ? mesh->getRegion()->rendererObject
                              : nullptr;
#endif

                vertexCount = static_cast<int>(length >> 1);
                uvs = &mesh->getUVs();
                indices = &mesh->getTriangles();
            } else if (attachment->getRTTI().isExactly(
                           sp::ClippingAttachment::rtti)) {
                clipper_.clipStart(*slot,
                                   static_cast<sp::ClippingAttachment*>(attachment));
                continue;
            } else {
                clipper_.clipEnd(*slot);
                continue;
            }

            if (texture == nullptr || indices == nullptr || uvs == nullptr) {
                clipper_.clipEnd(*slot);
                continue;
            }

            if (clipper_.isClipping()) {
                clipper_.clipTriangles(*vertices, *indices, *uvs, 2);
                vertices = &clipper_.getClippedVertices();
                uvs = &clipper_.getClippedUVs();
                indices = &clipper_.getClippedTriangles();
                vertexCount = static_cast<int>(
                    clipper_.getClippedVertices().size() >> 1);
            }

            if (vertexCount <= 0 || indices->size() == 0) {
                clipper_.clipEnd(*slot);
                continue;
            }

            const unsigned int color = PackColor(skeleton_->getColor(),
                                                 slot->getColor(),
                                                 *attachmentColor);

            SpineDrawBatch batch;
            batch.blendMode = static_cast<int>(slot->getData().getBlendMode());
            batch.texture = texture;
            batch.positions.assign(vertices->buffer(),
                                   vertices->buffer() + vertexCount * 2);
            batch.uvs.assign(uvs->buffer(), uvs->buffer() + vertexCount * 2);
            batch.colors.assign(static_cast<size_t>(vertexCount), color);
            batch.indices.assign(indices->buffer(),
                                 indices->buffer() + indices->size());

            batches_.push_back(std::move(batch));

            clipper_.clipEnd(*slot);
        }

        clipper_.clipEnd();

        return batches_;
    }

private:
    void ApplyPose() {
        animationState_->apply(*skeleton_);

#if defined(SPINE_RUNTIME_38)
        skeleton_->updateWorldTransform();
#else
        skeleton_->updateWorldTransform(sp::Physics_Update);
#endif
    }

    sp::Atlas* atlas_ = nullptr;
    sp::SkeletonData* skeletonData_ = nullptr;
    sp::Skeleton* skeleton_ = nullptr;
    sp::AnimationStateData* animationStateData_ = nullptr;
    sp::AnimationState* animationState_ = nullptr;

    SpineTextureLoaderImpl textureLoader_;
    sp::SkeletonClipping clipper_;
    sp::Vector<float> worldVertices_;
    sp::Vector<unsigned short> quadIndices_;

    std::vector<std::string> animations_;
    std::vector<SpineDrawBatch> batches_;
    bool premultipliedAlpha_ = false;
};

} // namespace spineimpl

#if defined(SPINE_RUNTIME_38)
ISpineRuntime* CreateSpineRuntime38() {
    return new spineimpl38::SpineRuntimeImpl();
}
#else
ISpineRuntime* CreateSpineRuntime42() {
    return new spineimpl42::SpineRuntimeImpl();
}
#endif
