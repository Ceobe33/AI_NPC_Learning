#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

#include "imgui_internal.h"

#include <GLFW/glfw3.h>

#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "imgui.h"

#include "App.h"
#include "FileDialog.h"
#include "LayoutStore.h"
#include "PlatformGL.h"
#include "WebFileBridge.h"

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>

#ifdef EMSCRIPTEN_USE_PORT_CONTRIB_GLFW3
#include <GLFW/emscripten_glfw3.h>
#endif
#endif

namespace {

// The DOM elements this module attaches to. They must exist in whichever page
// boots the wasm; see site/source/spine/index.md for the harness.
const char* kCanvasSelector = "#spine-studio-canvas";
const char* kCanvasHostSelector = "#spine-studio";

App gApp;

bool gShowExportDialog = false;

// Set from the File menu; handled at the top of the next frame, where the
// dock builder can be re-run safely.
bool gResetLayout = false;

const float kMinSpeed = 0.0f;
const float kMaxSpeed = 5.0f;

const float kMinZoom = 0.05f;
const float kMaxZoom = 32.0f;

// Text shown in the speed box. Kept as a buffer so it can be typed into
// without the slider fighting the user for it.
char gSpeedBuffer[16] = "1.00";
bool gSpeedEditing = false;

// Index into the "Off / x2 / x3 / x4" supersampling combo.
int gSupersampleIndex = 1;

std::string Lowercase(const std::string& text) {
    std::string result = text;

    for (char& c : result) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }

    return result;
}

// Loads `path` when it names a skeleton file. Returns whether it was loaded.
// This is what makes the tool usable on platforms without a native open dialog.
bool TryOpenSkeleton(const std::filesystem::path& path) {
    std::error_code error;

    if (path.empty() || !std::filesystem::is_regular_file(path, error)) {
        return false;
    }

    const std::string extension = Lowercase(path.extension().string());

    if (extension != ".json" && extension != ".skel") {
        return false;
    }

    return gApp.OpenSkeleton(path);
}

void HandleFileDrop(GLFWwindow*, int count, const char** paths) {
    for (int i = 0; i < count; ++i) {
        if (paths[i] != nullptr && TryOpenSkeleton(paths[i])) {
            return;
        }
    }
}

void ClampExportSettings() {
    gApp.gifSettings.width = ImClamp(gApp.gifSettings.width, 16, 2048);
    gApp.gifSettings.height = ImClamp(gApp.gifSettings.height, 16, 2048);
    gApp.gifSettings.fps = ImClamp(gApp.gifSettings.fps, 1, 60);
    gApp.gifSettings.alphaThreshold =
        ImClamp(gApp.gifSettings.alphaThreshold, 1, 255);
    gApp.gifSettings.supersample = ImClamp(gApp.gifSettings.supersample, 1, 4);
    gSupersampleIndex = gApp.gifSettings.supersample - 1;
}

void WriteSpeedBuffer(float speed) {
    std::snprintf(gSpeedBuffer, sizeof(gSpeedBuffer), "%.2f",
                  static_cast<double>(speed));
}

void SetPreviewZoom(float zoom) {
    gApp.previewView.zoom = ImClamp(zoom, kMinZoom, kMaxZoom);
}

void ResetPreviewView() {
    gApp.previewView.zoom = 1.0f;
    gApp.previewView.offsetX = 0.0f;
    gApp.previewView.offsetY = 0.0f;
}

} // namespace

static void DrawMainMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Open Skeleton", "Cmd+O")) {
                gApp.OpenSkeletonDialog();
            }

            if (ImGui::MenuItem("Close Skeleton", nullptr, false,
                                gApp.asset.IsLoaded())) {
                gApp.CloseSkeleton();
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Reset Layout")) {
                gResetLayout = true;
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Exit")) {
                glfwSetWindowShouldClose(glfwGetCurrentContext(), GLFW_TRUE);
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Skeleton Panel");
            ImGui::MenuItem("Animation Panel");
            ImGui::MenuItem("Timeline");
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Export")) {
            if (ImGui::MenuItem("Export GIF", nullptr, false,
                                gApp.player.IsLoaded())) {
                gShowExportDialog = true;
            }

            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }
}

static void DrawSkeletonPanel() {
    ImGui::Begin("Skeleton");

    ImGui::Text("Skeleton Files");
    ImGui::Separator();

#if defined(__EMSCRIPTEN__)
    // Always shown in the browser: nothing here ever has a native dialog.
    ImGui::TextWrapped(
        "Open the files of your Spine export - the .json or .skel skeleton, "
        "its .atlas and its page images (multi-select them, or use Open "
        "Folder to take the whole directory). The files stay in the browser; "
        "nothing is uploaded. Exported GIFs are offered as a download when "
        "they finish.");
    ImGui::Spacing();
#else
    if (!FileDialog::Available()) {
        ImGui::TextWrapped(
            "This platform has no file dialog. Drop a .json or .skel file onto "
            "the window, or pass it on the command line. GIF exports are "
            "written to the working directory.");
        ImGui::Spacing();
    }
#endif

    if (ImGui::Button("Open File")) {
        gApp.OpenSkeletonDialog();
    }

#if defined(__EMSCRIPTEN__)
    // The directory picker greys out individual files, so a .skel can only be
    // picked through the file variant above. The folder one is still useful
    // for exports spread over sub-directories.
    ImGui::SameLine();

    if (ImGui::Button("Open Folder")) {
        gApp.OpenSkeletonFolderDialog();
    }
#endif

    ImGui::Spacing();
    ImGui::Separator();

    if (gApp.asset.IsLoaded()) {
        const SpineAssetPaths& paths = gApp.asset.GetPaths();

        ImGui::Text("Skeleton: %s", paths.skeleton.filename().string().c_str());
        ImGui::Text("Atlas:    %s", paths.atlas.filename().string().c_str());
        ImGui::Text("Animations: %d",
                    static_cast<int>(gApp.asset.GetAnimations().size()));
        ImGui::Text("Premultiplied alpha: %s",
                    gApp.asset.UsesPremultipliedAlpha() ? "yes" : "no");

        // Diagnostics for the framing: if these two differ a lot, the
        // skeleton data describes a canvas much larger than the animation.
        float minX = 0.0f;
        float minY = 0.0f;
        float maxX = 0.0f;
        float maxY = 0.0f;

        if (gApp.player.GetContentBounds(minX, minY, maxX, maxY)) {
            ImGui::Text("Animation area: %.0f x %.0f", maxX - minX, maxY - minY);
        }

        float boundsX = 0.0f;
        float boundsY = 0.0f;
        float boundsWidth = 0.0f;
        float boundsHeight = 0.0f;
        gApp.asset.GetBounds(boundsX, boundsY, boundsWidth, boundsHeight);

        ImGui::Text("Skeleton bounds: %.0f x %.0f", boundsWidth, boundsHeight);

        ImGui::Spacing();

        if (ImGui::Button("Close")) {
            gApp.CloseSkeleton();
        }
    } else {
        ImGui::Text("No skeleton loaded.");
        ImGui::TextWrapped(
            "Open a .json or .skel file. The matching .atlas file and its .png "
            "pages are loaded from the same directory.");
    }

    ImGui::Spacing();
    ImGui::Separator();

    ImGui::TextWrapped("%s", gApp.statusMessage.c_str());

    if (!gApp.errorMessage.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s",
                           gApp.errorMessage.c_str());
    }

    ImGui::End();
}

static void DrawAnimationPanel() {
    ImGui::Begin("Animations");

    ImGui::Text("Animations");
    ImGui::Separator();

    if (!gApp.asset.IsLoaded()) {
        ImGui::TextDisabled("No skeleton loaded.");
        ImGui::End();
        return;
    }

    const std::vector<std::string>& animations = gApp.asset.GetAnimations();

    for (int i = 0; i < static_cast<int>(animations.size()); ++i) {
        if (ImGui::Selectable(animations[static_cast<size_t>(i)].c_str(),
                              gApp.selectedAnimation == i)) {
            gApp.SelectAnimation(i);
        }
    }

    ImGui::End();
}

// Maps mouse input over the preview image onto the camera.
static void HandlePreviewInput(const ImVec2& size) {
    if (!ImGui::IsItemHovered()) {
        return;
    }

    ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);

    ImGuiIO& io = ImGui::GetIO();
    SpineView& view = gApp.previewView;

    const int width = static_cast<int>(size.x);
    const int height = static_cast<int>(size.y);

    float boundsCenterX = 0.0f;
    float boundsCenterY = 0.0f;
    float scale = 1.0f;

    if (!gApp.player.GetCamera(width, height, view, boundsCenterX,
                               boundsCenterY, scale)) {
        return;
    }

    if (io.MouseWheel != 0.0f) {
        const ImVec2 topLeft = ImGui::GetItemRectMin();
        const ImVec2 mouse = ImGui::GetMousePos();
        const float sx = mouse.x - topLeft.x;
        const float sy = mouse.y - topLeft.y;

        // Skeleton point under the cursor: screen y grows downwards while
        // skeleton space is y-up, hence the flipped sy term.
        const float worldX =
            boundsCenterX + view.offsetX + (sx - width * 0.5f) / scale;
        const float worldY =
            boundsCenterY + view.offsetY + (height * 0.5f - sy) / scale;

        SetPreviewZoom(view.zoom * std::exp(io.MouseWheel * 0.15f));

        float newScale = 1.0f;

        if (gApp.player.GetCamera(width, height, view, boundsCenterX,
                                  boundsCenterY, newScale)) {
            // Keep the point under the cursor pinned while zooming.
            view.offsetX =
                worldX - (sx - width * 0.5f) / newScale - boundsCenterX;
            view.offsetY =
                worldY - (height * 0.5f - sy) / newScale - boundsCenterY;
        }
    }

    if (ImGui::IsMouseDragging(ImGuiMouseButton_Left) ||
        ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
        float currentScale = 1.0f;
        gApp.player.GetCamera(width, height, view, boundsCenterX,
                              boundsCenterY, currentScale);

        view.offsetX -= io.MouseDelta.x / currentScale;
        view.offsetY += io.MouseDelta.y / currentScale;
    }

    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        ResetPreviewView();
    }
}

static void DrawPreviewToolbar() {
    const bool loaded = gApp.player.IsLoaded();

    ImGui::BeginDisabled(!loaded);

    if (ImGui::Button("-##ZoomOut", ImVec2(24.0f, 0.0f))) {
        SetPreviewZoom(gApp.previewView.zoom / 1.25f);
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(72.0f);

    float zoomPercent = gApp.previewView.zoom * 100.0f;

    if (ImGui::DragFloat("##Zoom", &zoomPercent, 1.0f, kMinZoom * 100.0f,
                         kMaxZoom * 100.0f, "%.0f%%")) {
        SetPreviewZoom(zoomPercent / 100.0f);
    }

    ImGui::SameLine();

    if (ImGui::Button("+##ZoomIn", ImVec2(24.0f, 0.0f))) {
        SetPreviewZoom(gApp.previewView.zoom * 1.25f);
    }

    ImGui::SameLine();

    if (ImGui::Button("Fit", ImVec2(46.0f, 0.0f))) {
        ResetPreviewView();
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(96.0f);

    int fitMode = static_cast<int>(gApp.previewView.fit);

    if (ImGui::Combo("##FitMode", &fitMode, "Contain\0Fill\0")) {
        gApp.previewView.fit = static_cast<SpineView::FitMode>(fitMode);
    }

    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "Contain - the whole animation fits, with empty canvas left on\n"
            "the longer axis.\n"
            "Fill - the panel is covered completely, overflowing parts are\n"
            "cropped.");
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(110.0f);
    ImGui::SliderFloat("Grid", &gApp.previewView.gridSize, 8.0f, 128.0f,
                       "%.0f px");

    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::TextDisabled("(?)");

    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Drag to pan\nWheel to zoom\nDouble-click to fit");
    }
}

static void DrawPreviewPanel() {
    ImGui::Begin("Preview", nullptr, ImGuiWindowFlags_NoScrollbar);

    DrawPreviewToolbar();
    ImGui::Separator();

    const ImVec2 availableSize = ImGui::GetContentRegionAvail();

    if (gApp.player.IsLoaded() && availableSize.x > 1.0f &&
        availableSize.y > 1.0f) {
        const int width = static_cast<int>(availableSize.x);
        const int height = static_cast<int>(availableSize.y);

        gApp.RenderPreview(width, height);

        // The render target is bottom-up, ImGui expects top-down UVs.
        ImGui::Image((ImTextureID)(intptr_t)gApp.GetPreviewTexture(),
                     availableSize, ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));

        HandlePreviewInput(availableSize);
    } else {
        const ImVec2 windowSize = ImGui::GetWindowSize();
        const ImVec2 textSize = ImGui::CalcTextSize("Spine Preview");

        ImGui::SetCursorPos(ImVec2((windowSize.x - textSize.x) * 0.5f,
                                   (windowSize.y - textSize.y) * 0.5f));

        ImGui::Text("Spine Preview");
    }

    ImGui::End();
}

static void DrawPlaybackPanel() {
    ImGui::Begin("Playback");

    const bool loaded = gApp.player.IsLoaded();

    if (ImGui::Button("Play", ImVec2(70.0f, 0.0f)) && loaded) {
        gApp.player.Play();
    }

    ImGui::SameLine();

    if (ImGui::Button("Pause", ImVec2(70.0f, 0.0f)) && loaded) {
        gApp.player.Pause();
    }

    ImGui::SameLine();

    if (ImGui::Button("Reset", ImVec2(70.0f, 0.0f)) && loaded) {
        gApp.Restart();
    }

    ImGui::SameLine();

    bool loop = gApp.GetLoop();

    if (ImGui::Checkbox("Loop", &loop)) {
        gApp.SetLoop(loop);
    }

    ImGui::SameLine();

    ImGui::Text("Speed");
    ImGui::SameLine();

    float speed = gApp.GetSpeed();

    if (!gSpeedEditing) {
        WriteSpeedBuffer(speed);
    }

    ImGui::SetNextItemWidth(130.0f);

    if (ImGui::SliderFloat("##Speed", &speed, kMinSpeed, kMaxSpeed, "%.2fx")) {
        gApp.SetSpeed(speed);
        WriteSpeedBuffer(speed);
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(64.0f);

    const bool committed =
        ImGui::InputText("##SpeedValue", gSpeedBuffer, sizeof(gSpeedBuffer),
                         ImGuiInputTextFlags_CharsDecimal |
                             ImGuiInputTextFlags_EnterReturnsTrue) ||
        ImGui::IsItemDeactivatedAfterEdit();

    if (committed) {
        const float typed =
            ImClamp(std::strtof(gSpeedBuffer, nullptr), kMinSpeed, kMaxSpeed);

        gApp.SetSpeed(typed);
        WriteSpeedBuffer(typed);
    }

    gSpeedEditing = ImGui::IsItemActive();

    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Type a speed and press Enter (%.0f - %.0f)",
                          static_cast<double>(kMinSpeed),
                          static_cast<double>(kMaxSpeed));
    }

    ImGui::SameLine();

    ImGui::Text("Time: %.2f / %.2f", gApp.GetTime(), gApp.GetDuration());

    ImGui::Spacing();

    if (loaded) {
        const float duration = gApp.GetDuration();
        float time = gApp.GetTime();

        if (duration > 0.0f && time > duration) {
            time = std::fmod(time, duration);
        }

        ImGui::SetNextItemWidth(-1.0f);

        if (ImGui::SliderFloat("##Timeline", &time, 0.0f,
                               duration > 0.0f ? duration : 1.0f, "%.2fs")) {
            gApp.player.Pause();
            gApp.Seek(time);
        }
    }

    ImGui::End();
}

static void DrawExportDialog() {
    if (!gShowExportDialog) {
        return;
    }

    ImGui::Begin("Export GIF", &gShowExportDialog,
                 ImGuiWindowFlags_AlwaysAutoResize);

    ImGui::Text("Export the current animation as an animated GIF.");
    ImGui::Separator();

    ImGui::InputInt("Width", &gApp.gifSettings.width);
    ImGui::InputInt("Height", &gApp.gifSettings.height);

    if (gApp.GetPreviewWidth() > 0 && gApp.GetPreviewHeight() > 0) {
        ImGui::SameLine();

        if (ImGui::Button("Match preview")) {
            gApp.gifSettings.width = gApp.GetPreviewWidth();
            gApp.gifSettings.height = gApp.GetPreviewHeight();
        }

        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Use the current size of the Preview panel (%dx%d).",
                              gApp.GetPreviewWidth(), gApp.GetPreviewHeight());
        }
    }

    ImGui::SliderInt("FPS", &gApp.gifSettings.fps, 1, 60);
    ImGui::Checkbox("Transparent background", &gApp.gifSettings.transparent);

    ImGui::Checkbox("Dither", &gApp.gifSettings.dither);

    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "Spreads the 256-colour quantisation error over neighbouring "
            "pixels. Removes banding in gradients and soft shading.");
    }

    ImGui::Text("Supersampling");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f);
    ImGui::Combo("##Supersampling", &gSupersampleIndex, "Off\0x2\0x3\0x4\0");
    gApp.gifSettings.supersample = gSupersampleIndex + 1;

    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "Renders every frame larger and filters it down, so edges stay "
            "smooth instead of stair-stepping at the alpha cut-off. Slower, "
            "but much closer to what the preview shows.");
    }

    if (gApp.gifSettings.transparent) {
        // GIF has no partial transparency, so every pixel is snapped to fully
        // transparent or fully opaque at this cut-off. Lower it to keep faint
        // pixels (shadows, soft edges), raise it for cleaner outlines.
        ImGui::SliderInt("Alpha cut-off", &gApp.gifSettings.alphaThreshold, 1,
                         255);
    } else {
        ImGui::ColorEdit3("Background", gApp.gifSettings.background);
    }

    ClampExportSettings();

    ImGui::Separator();

    if (ImGui::Button("Export...", ImVec2(120.0f, 0.0f))) {
        std::string defaultName = "animation.gif";

        if (gApp.asset.IsLoaded()) {
            defaultName = gApp.asset.GetPaths().skeleton.stem().string() + "_" +
                          gApp.player.GetAnimation() + ".gif";
        }

        std::string path =
            FileDialog::SaveFile("Export GIF", defaultName, {"gif"});

        // Without a native dialog every export would be cancelled silently, so
        // write into the working directory instead.
        if (path.empty() && !FileDialog::Available()) {
            std::error_code error;
            path = (std::filesystem::current_path(error) / defaultName).string();
        }

        if (!path.empty()) {
            if (gApp.StartGifExport(path)) {
                gShowExportDialog = false;
            }
        }
    }

    ImGui::SameLine();

    if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f))) {
        gShowExportDialog = false;
    }

    ImGui::End();
}

static void DrawExportProgress() {
    if (!gApp.IsExporting()) {
        return;
    }

    ImGui::Begin("Exporting GIF", nullptr,
                 ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_AlwaysAutoResize);

    ImGui::TextWrapped("Writing %s", gApp.lastExportPath.c_str());
    ImGui::ProgressBar(gApp.GetExportProgress(),
                       ImVec2(320.0f, 0.0f),
                       gApp.GetExportProgressText().c_str());

    if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f))) {
        gApp.CancelGifExport();
    }

    ImGui::End();
}

// Builds the arrangement every fresh start starts from - identical on desktop
// and web, which is the point of having it in code instead of a checked-in
// imgui.ini. dockspaceID must come from inside DockSpaceWindow: ImGui hashes
// "MainDockSpace" against that window's id stack, and a node built under any
// other id is simply never displayed.
//
// force rebuilds the default arrangement even when a layout already exists,
// which is what "Reset Layout" needs.
static void SetupDefaultDockLayout(ImGuiID dockspaceID, bool force) {
    static bool initialized = false;

    if (initialized && !force) {
        return;
    }

    initialized = true;

    // Respect a layout restored from the layout store.
    if (!force) {
        if (ImGuiDockNode* existing = ImGui::DockBuilderGetNode(dockspaceID)) {
            if (existing->Windows.Size > 0 ||
                existing->ChildNodes[0] != nullptr ||
                existing->ChildNodes[1] != nullptr) {
                return;
            }
        }
    }

    ImGuiViewport* viewport = ImGui::GetMainViewport();

    ImGui::DockBuilderRemoveNode(dockspaceID);
    ImGui::DockBuilderAddNode(dockspaceID, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspaceID, viewport->WorkSize);

    ImGuiID dockMain = dockspaceID;
    ImGuiID dockLeft;
    ImGuiID dockRight;
    ImGuiID dockBottom;

    // Left: 22%
    ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Left, 0.22f, &dockLeft,
                                &dockMain);

    // Right: 22%
    ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Right, 0.22f, &dockRight,
                                &dockMain);

    // Bottom: 20%
    ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Down, 0.20f, &dockBottom,
                                &dockMain);

    ImGui::DockBuilderDockWindow("Skeleton", dockLeft);
    ImGui::DockBuilderDockWindow("Animations", dockRight);
    ImGui::DockBuilderDockWindow("Playback", dockBottom);
    ImGui::DockBuilderDockWindow("Preview", dockMain);

    ImGui::DockBuilderFinish(dockspaceID);
}

static void DrawDockspace() {
    ImGuiViewport* viewport = ImGui::GetMainViewport();

    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags windowFlags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
        ImGuiWindowFlags_NoBackground;

    ImGui::Begin("DockSpaceWindow", nullptr, windowFlags);

    ImGuiID dockspaceID = ImGui::GetID("MainDockSpace");

    // Inside the window on purpose - see the comment on the function. The
    // builder has to run before DockSpace() so the node exists when the
    // dockspace is drawn for the first time.
    if (gResetLayout) {
        gResetLayout = false;
        LayoutStore::Clear();
        SetupDefaultDockLayout(dockspaceID, true);
    } else {
        SetupDefaultDockLayout(dockspaceID, false);
    }

    ImGui::DockSpace(dockspaceID, ImVec2(0.0f, 0.0f),
                     ImGuiDockNodeFlags_PassthruCentralNode);

    ImGui::End();
}

// ImGui asks for a save a moment after the last change, so this only has to
// notice the request and hand the settings to the platform's store. Returns
// true after a write, so the caller can report progress if it wants to.
static bool UpdateLayoutPersistence() {
    ImGuiIO& io = ImGui::GetIO();

    if (!io.WantSaveIniSettings) {
        return false;
    }

    // ImGui keeps this set until the application clears it.
    io.WantSaveIniSettings = false;

    size_t size = 0;
    const char* data = ImGui::SaveIniSettingsToMemory(&size);

    if (data == nullptr || size == 0) {
        return false;
    }

    LayoutStore::Save(std::string(data, size));

    return true;
}

static void HandleShortcuts() {
    if (ImGui::IsKeyDown(ImGuiMod_Ctrl) && ImGui::IsKeyPressed(ImGuiKey_O)) {
        gApp.OpenSkeletonDialog();
    }
}

// The frame loop has to be runnable one iteration at a time. A browser never
// lets main() own the loop - it calls back into the module instead - so the
// body lives here and both hosts only decide how often to call it.
GLFWwindow* gWindow = nullptr;
float gLastFrameTime = 0.0f;

bool RunFrame() {
    if (gWindow == nullptr || glfwWindowShouldClose(gWindow)) {
        return false;
    }

    glfwPollEvents();

    const float now = static_cast<float>(glfwGetTime());
    const float deltaTime = now - gLastFrameTime;
    gLastFrameTime = now;

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    HandleShortcuts();

    gApp.Update(deltaTime);

    // Draw application UI.

    DrawMainMenuBar();

    DrawDockspace();

    DrawSkeletonPanel();
    DrawAnimationPanel();
    DrawPreviewPanel();
    DrawPlaybackPanel();

    DrawExportDialog();
    DrawExportProgress();

    // Advances the GIF export one frame at a time so the progress bar
    // stays responsive.
    if (gApp.IsExporting()) {
        gApp.StepGifExport();
    }

    // Render.

    ImGui::Render();

    int displayWidth = 0;
    int displayHeight = 0;

    glfwGetFramebufferSize(gWindow, &displayWidth, &displayHeight);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, displayWidth, displayHeight);

    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(gWindow);

    UpdateLayoutPersistence();

    return true;
}

void ShutdownStudio() {
    gApp.CloseSkeleton();

    // One last write: ImGui only asks for a save a moment after the last
    // change, so a resize made in the final second would otherwise be lost.
    size_t settingsSize = 0;
    const char* settings = ImGui::SaveIniSettingsToMemory(&settingsSize);

    if (settings != nullptr && settingsSize > 0) {
        LayoutStore::Save(std::string(settings, settingsSize));
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();

    ImGui::DestroyContext();

    glfwDestroyWindow(gWindow);
    glfwTerminate();

    gWindow = nullptr;
}

#if defined(__EMSCRIPTEN__)
void EmscriptenFrame() {
    if (!RunFrame()) {
        ShutdownStudio();
        emscripten_cancel_main_loop();
    }
}
#endif

int main(int argc, char** argv) {
    // --------------------------------------------------------
    // Initialize GLFW
    // --------------------------------------------------------

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }

#if defined(__EMSCRIPTEN__)
    // WebGL 2 == GLES 3.0, asked for explicitly. The browser has no desktop
    // profile and no forward compatibility switch.
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

#ifdef EMSCRIPTEN_USE_PORT_CONTRIB_GLFW3
    // The canvas belongs to the page, not to GLFW, so it has to be named
    // before the window that draws into it is created.
    emscripten_glfw_set_next_window_canvas_selector(kCanvasSelector);
#endif
#else
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
#endif

    GLFWwindow* window =
        glfwCreateWindow(1280, 720, "Spine Animation Studio", nullptr, nullptr);

    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }

    // Dropping a skeleton onto the window works everywhere, so it doubles as
    // the way to open files where there is no native dialog.
    glfwSetDropCallback(window, HandleFileDrop);

    glfwMakeContextCurrent(window);

#ifdef SPINE_STUDIO_NEEDS_GL_LOADER
    // Where GL only reaches 1.1 every core profile entry point has to be
    // resolved at run time. Nothing may touch GL before this succeeds.
    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        std::cerr << "Failed to initialize the OpenGL loader\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }
#endif

    // Enable VSync.
    glfwSwapInterval(1);

    // --------------------------------------------------------
    // Initialize Dear ImGui
    // --------------------------------------------------------

    IMGUI_CHECKVERSION();

    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();

    // Enable docking.
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // The layout is persisted by LayoutStore instead: ImGui's own file would
    // land in the working directory, which a browser cannot offer and a
    // signed app bundle is not allowed to write to. Leaving it unset is what
    // switches ImGui into manual mode (io.WantSaveIniSettings).
    io.IniFilename = nullptr;

    // Restoring first means every platform that has run before shows its own
    // saved layout, while a fresh one - web or desktop - falls through to the
    // identical default the dock builder produces.
    const std::string savedLayout = LayoutStore::Load();

    if (!savedLayout.empty()) {
        ImGui::LoadIniSettingsFromMemory(savedLayout.c_str(), savedLayout.size());
    }

    // Windows may only be dragged by their title bar. Without this, dragging
    // any empty space moves the window, which fights with the drag-to-pan
    // gesture on the Preview image.
    io.ConfigWindowsMoveFromTitleBarOnly = true;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);

#if defined(__EMSCRIPTEN__)
    // Without this the canvas keeps the pixel size it was created with and
    // never follows the window.
    ImGui_ImplGlfw_InstallEmscriptenCallbacks(window, kCanvasSelector);

#ifdef EMSCRIPTEN_USE_PORT_CONTRIB_GLFW3
    // Letting the surrounding <div> dictate the size keeps the editor usable in
    // any layout, including the narrow one this site uses on phones.
    emscripten_glfw_make_canvas_resizable(window, kCanvasHostSelector, nullptr);
#endif
#endif

#if defined(__EMSCRIPTEN__)
    // The GL version prefix tells ImGui which GLSL dialect to emit. WebGL 2
    // only understands the ES flavour, even though it runs the same core
    // features as desktop GL 3.3.
    ImGui_ImplOpenGL3_Init("#version 300 es");
#else
    ImGui_ImplOpenGL3_Init("#version 330");
#endif

    // Lets the browser's directory picker hand whatever skeleton it found over
    // to App, the same way a drop or a command line argument would.
    WebFileBridge::SetOpenPathCallback([](const char* path) {
        return TryOpenSkeleton(path);
    });

    // --------------------------------------------------------
    // Load whatever was passed on the command line
    // --------------------------------------------------------

    // Only relevant on platforms without a native open dialog, but harmless
    // elsewhere. macOS Finder hands the app a -psn_* argument, hence the
    // leading dash check.
    for (int i = 1; i < argc; ++i) {
        if (argv[i] != nullptr && argv[i][0] != '-' && TryOpenSkeleton(argv[i])) {
            break;
        }
    }

    // --------------------------------------------------------
    // Main Loop
    // --------------------------------------------------------

    gWindow = window;
    gLastFrameTime = static_cast<float>(glfwGetTime());

#if defined(__EMSCRIPTEN__)
    // Returning to the browser is what keeps the page responsive; asking for
    // 0 frames per second means "match requestAnimationFrame".
    emscripten_set_main_loop(EmscriptenFrame, 0, true);
#else
    while (RunFrame()) {
    }

    ShutdownStudio();
#endif

    return 0;
}
