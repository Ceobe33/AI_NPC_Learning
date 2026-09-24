#include "App.h"

#include "FileDialog.h"
#include "PlatformGL.h"
#include "WebFileBridge.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

const char* kSkeletonFilters[] = {"json", "skel"};

// Upper bound for the supersampled export framebuffer (per axis).
const int kMaxExportDimension = 4096;

// Averaging premultiplied RGBA is linear, so box-filtering the supersampled
// framebuffer gives the same result as rendering the frame with real coverage
// antialiasing - which is what keeps the skeleton edges from stair-stepping
// once the alpha is snapped to the GIF cut-off.
void BoxDownsample(const unsigned char* src, int srcWidth, int srcHeight,
                   unsigned char* dst, int dstWidth, int dstHeight,
                   int scale) {
    for (int y = 0; y < dstHeight; ++y) {
        for (int x = 0; x < dstWidth; ++x) {
            unsigned int sum[4] = {0, 0, 0, 0};

            for (int sy = 0; sy < scale; ++sy) {
                const unsigned char* row =
                    src + ((static_cast<size_t>(y) * scale + sy) * srcWidth +
                           static_cast<size_t>(x) * scale) *
                              4;

                for (int sx = 0; sx < scale; ++sx) {
                    sum[0] += row[sx * 4 + 0];
                    sum[1] += row[sx * 4 + 1];
                    sum[2] += row[sx * 4 + 2];
                    sum[3] += row[sx * 4 + 3];
                }
            }

            const unsigned int samples = static_cast<unsigned int>(scale * scale);
            unsigned char* out = dst + (static_cast<size_t>(y) * dstWidth + x) * 4;

            for (int c = 0; c < 4; ++c) {
                out[c] = static_cast<unsigned char>((sum[c] + samples / 2) / samples);
            }
        }
    }
}

} // namespace

// ---------------------------------------------------------------------------
// Import
// ---------------------------------------------------------------------------

void App::OpenSkeletonDialog() {
    // In the browser the pick happens in JavaScript and the chosen skeleton
    // arrives later through WebFileBridge's callback, so there is nothing to
    // wait on here. FileDialog::OpenFile is a stub there - calling it would
    // yield an empty path and make the button look dead.
    if (WebFileBridge::Available()) {
        WebFileBridge::OpenSkeletonFiles();
        return;
    }

    const std::string path = FileDialog::OpenFile(
        "Open Spine Skeleton",
        std::vector<std::string>(std::begin(kSkeletonFilters),
                                 std::end(kSkeletonFilters)));

    if (path.empty()) {
        return;
    }

    OpenSkeleton(path);
}

void App::OpenSkeletonFolderDialog() {
    // A folder only makes sense as a browser concept: on the desktop the file
    // dialog already lets the user reach whatever they need.
    if (WebFileBridge::Available()) {
        WebFileBridge::OpenSkeletonFolder();
        return;
    }

    OpenSkeletonDialog();
}

bool App::OpenSkeleton(const std::filesystem::path& skeletonPath) {
    errorMessage.clear();

    if (!asset.Load(skeletonPath)) {
        errorMessage = asset.GetError();
        statusMessage = "Failed to load skeleton.";
        return false;
    }

    player.Unload();

    if (!player.Load(asset)) {
        errorMessage = "Failed to create the skeleton instance.";
        statusMessage = "Failed to load skeleton.";
        return false;
    }

    player.SetSpeed(GetSpeed());

    // Some skeletons expose zero length pose animations; skip those so that
    // playback and export have something to work with.
    selectedAnimation = 0;

    const std::vector<std::string>& animations = asset.GetAnimations();

    for (size_t i = 0; i < animations.size(); ++i) {
        player.SetAnimation(animations[i], loop);
        selectedAnimation = static_cast<int>(i);

        if (player.GetDuration() > 0.0f) {
            break;
        }
    }

    gifSettings.width = 512;
    gifSettings.height = 512;

    // A new skeleton means new bounds, so the fitted camera has to start over.
    previewView = SpineView();

    statusMessage = "Loaded " + asset.GetPaths().skeleton.filename().string() +
                    " (" + std::to_string(asset.GetAnimations().size()) +
                    " animations)";

    return true;
}

void App::CloseSkeleton() {
    player.Unload();
    asset.Unload();
    previewTarget_.Release();
    exportPreviewTarget_.Release();

    previewView = SpineView();
    selectedAnimation = 0;
    statusMessage = "No skeleton loaded.";
}

// ---------------------------------------------------------------------------
// Playback
// ---------------------------------------------------------------------------

void App::SelectAnimation(int index) {
    if (index < 0 || index >= static_cast<int>(asset.GetAnimations().size())) {
        return;
    }

    selectedAnimation = index;
    player.SetAnimation(asset.GetAnimations()[static_cast<size_t>(index)], loop);
}

void App::Update(float deltaTime) {
    player.Update(deltaTime);
}

void App::SetSpeed(float speed) {
    player.SetSpeed(speed);
}

float App::GetSpeed() const {
    return player.GetSpeed();
}

void App::SetLoop(bool loopEnabled) {
    loop = loopEnabled;

    if (selectedAnimation >= 0 &&
        selectedAnimation < static_cast<int>(asset.GetAnimations().size())) {
        player.SetAnimation(asset.GetAnimations()[static_cast<size_t>(selectedAnimation)],
                            loop);
    }
}

bool App::GetLoop() const {
    return loop;
}

void App::TogglePlay() {
    if (player.IsPlaying()) {
        player.Pause();
    } else {
        player.Play();
    }
}

void App::Restart() {
    player.Reset();
    player.Play();
}

void App::Seek(float time) {
    player.Seek(time);
}

float App::GetTime() const {
    return player.GetTime();
}

float App::GetDuration() const {
    return player.GetDuration();
}

// ---------------------------------------------------------------------------
// Preview
// ---------------------------------------------------------------------------

void App::RenderPreview(int width, int height) {
    if (!player.IsLoaded() || width <= 0 || height <= 0) {
        return;
    }

    if (!previewTarget_.Resize(width, height)) {
        return;
    }

    previewWidth_ = width;
    previewHeight_ = height;

    previewTarget_.Bind();
    player.Draw(width, height, background, previewView);
    previewTarget_.Unbind();
}

unsigned int App::GetPreviewTexture() const {
    return previewTarget_.GetTexture();
}

int App::GetPreviewWidth() const {
    return previewWidth_;
}

int App::GetPreviewHeight() const {
    return previewHeight_;
}

unsigned int App::RenderExportPreview() {
    if (!player.IsLoaded()) {
        return 0;
    }

    const int width = gifSettings.width;
    const int height = gifSettings.height;

    if (!exportPreviewTarget_.Resize(width, height)) {
        return 0;
    }

    float exportBackground[4] = {0.0f, 0.0f, 0.0f, 0.0f};

    if (!gifSettings.transparent) {
        exportBackground[0] = gifSettings.background[0];
        exportBackground[1] = gifSettings.background[1];
        exportBackground[2] = gifSettings.background[2];
        exportBackground[3] = gifSettings.background[3];
    }

    // The export always uses the fitted camera. The checkerboard stands in for
    // the transparent areas; with a solid background the real colour is shown
    // instead, so the preview matches what lands in the file.
    SpineView view;
    view.grid = gifSettings.transparent;

    exportPreviewTarget_.Bind();
    player.Draw(width, height, exportBackground, view);
    exportPreviewTarget_.Unbind();

    return exportPreviewTarget_.GetTexture();
}

int App::GetPlannedFrameCount() const {
    const float duration = player.GetDuration();

    if (duration <= 0.0f || gifSettings.fps <= 0) {
        return 0;
    }

    return std::max(1, static_cast<int>(std::ceil(duration * gifSettings.fps)));
}

// ---------------------------------------------------------------------------
// GIF export
// ---------------------------------------------------------------------------

bool App::StartGifExport(const std::string& path) {
    if (!player.IsLoaded()) {
        errorMessage = "Load a skeleton before exporting.";
        return false;
    }

    const float duration = player.GetDuration();

    if (duration <= 0.0f) {
        errorMessage = "The selected animation has no duration.";
        return false;
    }

    exportScale_ = std::max(1, std::min(4, gifSettings.supersample));

    // Keep the supersampled framebuffer within what the GL driver will
    // actually allocate.
    while (exportScale_ > 1 &&
           (gifSettings.width * exportScale_ > kMaxExportDimension ||
            gifSettings.height * exportScale_ > kMaxExportDimension)) {
        --exportScale_;
    }

    if (!exportTarget_.Resize(gifSettings.width * exportScale_,
                              gifSettings.height * exportScale_)) {
        errorMessage = "Could not allocate the export render target.";
        return false;
    }

    // The renderer blends with premultiplied alpha, so the pixels read back
    // from the framebuffer have to be divided by alpha again before they go
    // into the GIF.
    gifSettings.premultipliedAlpha = asset.UsesPremultipliedAlpha();

    if (!gifEncoder_.Begin(path, gifSettings)) {
        errorMessage = gifEncoder_.GetError();
        return false;
    }

    const size_t frameBytes = static_cast<size_t>(gifSettings.width) *
                              static_cast<size_t>(gifSettings.height) * 4;

    framePixels_.resize(frameBytes);
    sourcePixels_.resize(frameBytes * static_cast<size_t>(exportScale_) *
                         static_cast<size_t>(exportScale_));

    exportTimeBeforeStart_ = player.GetTime();
    exportResumePlayback_ = player.IsPlaying();

    player.Pause();

    exportFrameCount_ =
        std::max(1, static_cast<int>(std::ceil(duration * gifSettings.fps)));
    exportFrame_ = 0;
    exporting_ = true;
    lastExportPath = path;

    errorMessage.clear();

    return true;
}

bool App::StepGifExport() {
    if (!exporting_) {
        return false;
    }

    const int width = gifSettings.width;
    const int height = gifSettings.height;
    const int sourceWidth = width * exportScale_;
    const int sourceHeight = height * exportScale_;

    player.Seek(static_cast<float>(exportFrame_) /
                static_cast<float>(gifSettings.fps));

    // A transparent export has to clear to transparent black, not to the
    // background colour: with premultiplied alpha any non-zero clear colour
    // stays mixed into the semi-transparent edges and survives as a coloured
    // fringe around the skeleton.
    float exportBackground[4] = {0.0f, 0.0f, 0.0f, 0.0f};

    if (!gifSettings.transparent) {
        exportBackground[0] = gifSettings.background[0];
        exportBackground[1] = gifSettings.background[1];
        exportBackground[2] = gifSettings.background[2];
        exportBackground[3] = gifSettings.background[3];
    }

    // The exported frames never carry the checkerboard; it is only a preview
    // aid.
    SpineView exportView;
    exportView.grid = false;
    exportView.padding = 0.04f;

    exportTarget_.Bind();
    player.Draw(sourceWidth, sourceHeight, exportBackground, exportView);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, sourceWidth, sourceHeight, GL_RGBA, GL_UNSIGNED_BYTE,
                 sourcePixels_.data());
    exportTarget_.Unbind();

    if (exportScale_ > 1) {
        BoxDownsample(sourcePixels_.data(), sourceWidth, sourceHeight,
                      framePixels_.data(), width, height, exportScale_);
    } else {
        std::memcpy(framePixels_.data(), sourcePixels_.data(),
                    framePixels_.size());
    }

    gifEncoder_.WriteFrame(framePixels_.data(), width, height);

    ++exportFrame_;

    if (exportFrame_ >= exportFrameCount_) {
        gifEncoder_.End();

        exporting_ = false;

        player.Seek(exportTimeBeforeStart_);

        if (exportResumePlayback_) {
            player.Play();
        }

        statusMessage = "Exported " + std::to_string(exportFrameCount_) +
                        " frames to " + lastExportPath;

        // MEMFS is wiped when the tab closes, so the file has to leave the
        // browser while the user still expects it.
        WebFileBridge::DownloadFile(lastExportPath);

        return false;
    }

    return true;
}

void App::CancelGifExport() {
    if (!exporting_) {
        return;
    }

    gifEncoder_.End();
    exporting_ = false;

    player.Seek(exportTimeBeforeStart_);

    if (exportResumePlayback_) {
        player.Play();
    }

    statusMessage = "GIF export cancelled.";
}

bool App::IsExporting() const {
    return exporting_;
}

float App::GetExportProgress() const {
    if (exportFrameCount_ <= 0) {
        return 0.0f;
    }

    return static_cast<float>(exportFrame_) /
           static_cast<float>(exportFrameCount_);
}

std::string App::GetExportProgressText() const {
    return std::to_string(exportFrame_) + " / " +
           std::to_string(exportFrameCount_) + " frames";
}
