#include "App.h"

#include "FileDialog.h"
#include "PlatformGL.h"

#include <cmath>

namespace {

const char* kSkeletonFilters[] = {"json", "skel"};

} // namespace

// ---------------------------------------------------------------------------
// Import
// ---------------------------------------------------------------------------

void App::OpenSkeletonDialog() {
    const std::string path = FileDialog::OpenFile(
        "Open Spine Skeleton",
        std::vector<std::string>(std::begin(kSkeletonFilters),
                                 std::end(kSkeletonFilters)));

    if (path.empty()) {
        return;
    }

    OpenSkeleton(path);
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

    previewTarget_.Bind();
    player.Draw(width, height, background, previewView);
    previewTarget_.Unbind();
}

unsigned int App::GetPreviewTexture() const {
    return previewTarget_.GetTexture();
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

    if (!exportTarget_.Resize(gifSettings.width, gifSettings.height)) {
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

    framePixels_.resize(static_cast<size_t>(gifSettings.width) *
                        static_cast<size_t>(gifSettings.height) * 4);

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

    const int width = exportTarget_.GetWidth();
    const int height = exportTarget_.GetHeight();

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
    exportView.padding = 0.12f;

    exportTarget_.Bind();
    player.Draw(width, height, exportBackground, exportView);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                 framePixels_.data());
    exportTarget_.Unbind();

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
