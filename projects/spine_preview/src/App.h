#ifndef __APP_H__
#define __APP_H__

#include <filesystem>
#include <string>
#include <vector>

#include "GifEncoder.h"
#include "SpineAsset.h"
#include "SpineRenderer.h"

// Holds the whole import -> play -> export pipeline and the UI state around it.
class App {
public:
    void OpenSkeletonDialog();

    // Browser-only variant: picks a whole export folder instead of files.
    void OpenSkeletonFolderDialog();

    bool OpenSkeleton(const std::filesystem::path& skeletonPath);

    void CloseSkeleton();

    void SelectAnimation(int index);

    void Update(float deltaTime);

    // Renders the current pose into the preview render target.
    void RenderPreview(int width, int height);

    unsigned int GetPreviewTexture() const;

    // Size of the last rendered preview panel, used to offer an export that
    // matches what is on screen.
    int GetPreviewWidth() const;

    int GetPreviewHeight() const;

    // Renders the pose at the export resolution, using the export settings
    // (size, background, transparency). Returns the texture to display, or 0.
    unsigned int RenderExportPreview();

    int GetPlannedFrameCount() const;

    void SetSpeed(float speed);

    float GetSpeed() const;

    void SetLoop(bool loop);

    bool GetLoop() const;

    void TogglePlay();

    void Restart();

    void Seek(float time);

    float GetTime() const;

    float GetDuration() const;

    // --- GIF export -------------------------------------------------------

    bool StartGifExport(const std::string& path);

    // Processes a single frame of the running export. Returns true while busy.
    bool StepGifExport();

    void CancelGifExport();

    bool IsExporting() const;

    float GetExportProgress() const;

    std::string GetExportProgressText() const;

    // --- state ------------------------------------------------------------

    SpineAsset asset;
    SpinePlayer player;

    int selectedAnimation = 0;
    bool loop = true;

    float background[4] = {0.09f, 0.09f, 0.09f, 1.0f};

    // Pan/zoom of the Preview panel. Reset whenever a skeleton is loaded.
    SpineView previewView;

    GifExportSettings gifSettings;

    std::string statusMessage = "No skeleton loaded.";
    std::string errorMessage;

    std::string lastExportPath;

private:
    RenderTarget previewTarget_;
    RenderTarget exportPreviewTarget_;

    int previewWidth_ = 0;
    int previewHeight_ = 0;

    GifEncoder gifEncoder_;
    RenderTarget exportTarget_;
    std::vector<unsigned char> framePixels_;
    std::vector<unsigned char> sourcePixels_;

    bool exporting_ = false;
    int exportFrame_ = 0;
    int exportFrameCount_ = 0;
    int exportScale_ = 1;
    float exportTimeBeforeStart_ = 0.0f;
    bool exportResumePlayback_ = false;
};

#endif /* ifndef __APP_H__ */
