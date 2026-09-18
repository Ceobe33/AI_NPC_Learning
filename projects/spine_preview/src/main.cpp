#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>

#include "imgui_internal.h"

#include <GLFW/glfw3.h>

#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "imgui.h"

#include "App.h"
#include "FileDialog.h"
#include "PlatformGL.h"

namespace {

App gApp;

bool gShowExportDialog = false;

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

    if (ImGui::Button("Open File")) {
        gApp.OpenSkeletonDialog();
    }

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

        const std::string path =
            FileDialog::SaveFile("Export GIF", defaultName, {"gif"});

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

static void SetupDefaultDockLayout() {
    static bool initialized = false;

    if (initialized) {
        return;
    }

    initialized = true;

    const ImGuiID dockspaceID = ImGui::GetID("MainDockSpace");

    // Respect a layout the user saved in imgui.ini.
    if (ImGuiDockNode* existing = ImGui::DockBuilderGetNode(dockspaceID)) {
        if (existing->Windows.Size > 0 || existing->ChildNodes[0] != nullptr ||
            existing->ChildNodes[1] != nullptr) {
            return;
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

    ImGui::DockSpace(dockspaceID, ImVec2(0.0f, 0.0f),
                     ImGuiDockNodeFlags_PassthruCentralNode);

    ImGui::End();
}

static void HandleShortcuts() {
    if (ImGui::IsKeyDown(ImGuiMod_Ctrl) && ImGui::IsKeyPressed(ImGuiKey_O)) {
        gApp.OpenSkeletonDialog();
    }
}

int main() {
    // --------------------------------------------------------
    // Initialize GLFW
    // --------------------------------------------------------

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window =
        glfwCreateWindow(1280, 720, "Spine Animation Studio", nullptr, nullptr);

    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

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

    // Windows may only be dragged by their title bar. Without this, dragging
    // any empty space moves the window, which fights with the drag-to-pan
    // gesture on the Preview image.
    io.ConfigWindowsMoveFromTitleBarOnly = true;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // --------------------------------------------------------
    // Main Loop
    // --------------------------------------------------------

    float lastTime = static_cast<float>(glfwGetTime());

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        const float now = static_cast<float>(glfwGetTime());
        const float deltaTime = now - lastTime;
        lastTime = now;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        HandleShortcuts();

        gApp.Update(deltaTime);

        // Draw application UI.

        DrawMainMenuBar();

        DrawDockspace();
        SetupDefaultDockLayout();

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

        glfwGetFramebufferSize(window, &displayWidth, &displayHeight);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, displayWidth, displayHeight);

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // --------------------------------------------------------
    // Cleanup
    // --------------------------------------------------------

    gApp.CloseSkeleton();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();

    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
