#include "SpineRenderer.h"

#include "PlatformGL.h"

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <vector>

namespace {

// GLSL ES - the dialect WebGL 2 speaks - differs from desktop GLSL in two
// ways that matter here: the version directive names ES explicitly, and a
// fragment shader has to declare a default precision for floats or it will not
// compile. Desktop GLSL accepts neither line, so they are picked per platform.
#if defined(__EMSCRIPTEN__)
const char* kVertexHeader = "#version 300 es\n";

const char* kFragmentHeader =
    "#version 300 es\n"
    "precision highp float;\n";
#else
const char* kVertexHeader = "#version 330\n";

const char* kFragmentHeader = "#version 330\n";
#endif

std::string BuildSource(const char* header, const char* body) {
    return std::string(header) + body;
}

const std::string kVertexShader = BuildSource(kVertexHeader,
    "layout(location = 0) in vec2 a_position;\n"
    "layout(location = 1) in vec2 a_uv;\n"
    "layout(location = 2) in vec4 a_color;\n"
    "uniform vec2 u_center;\n"
    "uniform vec2 u_halfSize;\n"
    "out vec2 v_uv;\n"
    "out vec4 v_color;\n"
    "void main() {\n"
    "    v_uv = a_uv;\n"
    "    v_color = a_color;\n"
    "    gl_Position = vec4((a_position - u_center) / u_halfSize, 0.0, 1.0);\n"
    "}\n");

const std::string kFragmentShader = BuildSource(kFragmentHeader,
    "in vec2 v_uv;\n"
    "in vec4 v_color;\n"
    "uniform sampler2D u_texture;\n"
    "out vec4 o_color;\n"
    "void main() {\n"
    "    o_color = texture(u_texture, v_uv) * v_color;\n"
    "}\n");

// Fullscreen triangle strip generated from gl_VertexID, no buffers needed.
const std::string kGridVertexShader = BuildSource(kVertexHeader,
    "void main() {\n"
    "    vec2 p = vec2(float((gl_VertexID & 1) << 2) - 1.0,\n"
    "                  float((gl_VertexID & 2) << 1) - 1.0);\n"
    "    gl_Position = vec4(p, 0.0, 1.0);\n"
    "}\n");

const std::string kGridFragmentShader = BuildSource(kFragmentHeader,
    "uniform vec2 u_origin;\n"  // canvas position of skeleton-space (0, 0)
    "uniform float u_cell;\n"
    "uniform vec3 u_light;\n"
    "uniform vec3 u_dark;\n"
    "out vec4 o_color;\n"
    "void main() {\n"
    "    vec2 cell = floor((gl_FragCoord.xy - u_origin) / u_cell);\n"
    "    float checker = mod(cell.x + cell.y, 2.0);\n"
    "    o_color = vec4(mix(u_dark, u_light, checker), 1.0);\n"
    "}\n");

unsigned int CompileShader(unsigned int type, const std::string& source) {
    unsigned int shader = glCreateShader(type);

    const char* sourcePointer = source.c_str();
    glShaderSource(shader, 1, &sourcePointer, nullptr);
    glCompileShader(shader);

    int status = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);

    if (status == 0) {
        char log[1024];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::fprintf(stderr, "Spine shader compilation failed: %s\n", log);
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

struct SpineVertex {
    float x;
    float y;
    float u;
    float v;
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
};

} // namespace

// ---------------------------------------------------------------------------
// RenderTarget
// ---------------------------------------------------------------------------

RenderTarget::~RenderTarget() {
    Release();
}

bool RenderTarget::Resize(int width, int height) {
    if (width <= 0 || height <= 0) {
        return false;
    }

    if (framebuffer_ != 0 && width_ == width && height_ == height) {
        return true;
    }

    Release();

    glGenFramebuffers(1, &framebuffer_);
    glGenTextures(1, &texture_);

    glBindTexture(GL_TEXTURE_2D, texture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           texture_, 0);

    const bool complete =
        glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    if (!complete) {
        Release();
        return false;
    }

    width_ = width;
    height_ = height;

    return true;
}

void RenderTarget::Release() {
    if (texture_ != 0) {
        glDeleteTextures(1, &texture_);
        texture_ = 0;
    }

    if (framebuffer_ != 0) {
        glDeleteFramebuffers(1, &framebuffer_);
        framebuffer_ = 0;
    }

    width_ = 0;
    height_ = 0;
}

void RenderTarget::Bind() {
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFramebuffer_);
    glGetIntegerv(GL_VIEWPORT, previousViewport_);

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
    glViewport(0, 0, width_, height_);
}

void RenderTarget::Unbind() {
    glBindFramebuffer(GL_FRAMEBUFFER, previousFramebuffer_);
    glViewport(previousViewport_[0], previousViewport_[1], previousViewport_[2],
               previousViewport_[3]);
}

unsigned int RenderTarget::GetTexture() const {
    return texture_;
}

int RenderTarget::GetWidth() const {
    return width_;
}

int RenderTarget::GetHeight() const {
    return height_;
}

// ---------------------------------------------------------------------------
// SpinePlayer
// ---------------------------------------------------------------------------

SpinePlayer::~SpinePlayer() {
    Unload();
}

bool SpinePlayer::Load(SpineAsset& asset) {
    if (!asset.IsLoaded()) {
        return false;
    }

    Unload();

    asset_ = &asset;
    playing_ = true;

    CreateGLObjects();

    // Frame whatever the skeleton looks like before an animation is picked.
    RefreshContentBounds();

    return true;
}

void SpinePlayer::Unload() {
    DestroyGLObjects();
    ResetContentBounds();

    asset_ = nullptr;
    animation_.clear();
    playing_ = false;
}

bool SpinePlayer::IsLoaded() const {
    return asset_ != nullptr && asset_->GetRuntime() != nullptr;
}

void SpinePlayer::Play() {
    playing_ = true;
}

void SpinePlayer::Pause() {
    playing_ = false;
}

void SpinePlayer::Reset() {
    Seek(0.0f);
}

bool SpinePlayer::IsPlaying() const {
    return playing_;
}

void SpinePlayer::SetAnimation(const std::string& name, bool loop) {
    if (!IsLoaded() || name.empty()) {
        return;
    }

    animation_ = name;
    asset_->GetRuntime()->SetAnimation(name, loop);

    // Every animation covers a different area, so the framing has to be
    // measured again instead of reusing the previous one.
    RefreshContentBounds();
}

const std::string& SpinePlayer::GetAnimation() const {
    return animation_;
}

void SpinePlayer::ResetContentBounds() {
    hasContentBounds_ = false;
    contentMinX_ = 0.0f;
    contentMinY_ = 0.0f;
    contentMaxX_ = 0.0f;
    contentMaxY_ = 0.0f;
}

bool SpinePlayer::GetContentBounds(float& minX, float& minY, float& maxX,
                                   float& maxY) const {
    if (!hasContentBounds_) {
        return false;
    }

    minX = contentMinX_;
    minY = contentMinY_;
    maxX = contentMaxX_;
    maxY = contentMaxY_;

    return true;
}

void SpinePlayer::AccumulateContentBounds() {
    if (!IsLoaded()) {
        return;
    }

    for (const SpineDrawBatch& batch : asset_->GetRuntime()->BuildDrawBatches()) {
        const size_t vertexCount = batch.positions.size() / 2;

        for (size_t i = 0; i < vertexCount; ++i) {
            const float x = batch.positions[i * 2];
            const float y = batch.positions[i * 2 + 1];

            if (!hasContentBounds_) {
                contentMinX_ = x;
                contentMaxX_ = x;
                contentMinY_ = y;
                contentMaxY_ = y;
                hasContentBounds_ = true;
                continue;
            }

            if (x < contentMinX_) {
                contentMinX_ = x;
            }

            if (x > contentMaxX_) {
                contentMaxX_ = x;
            }

            if (y < contentMinY_) {
                contentMinY_ = y;
            }

            if (y > contentMaxY_) {
                contentMaxY_ = y;
            }
        }
    }
}

void SpinePlayer::RefreshContentBounds() {
    ResetContentBounds();

    if (!IsLoaded()) {
        return;
    }

    const float duration = GetDuration();

    if (duration <= 0.0f) {
        // No animation (or an empty one): frame the pose that is applied now.
        AccumulateContentBounds();
        return;
    }

    // Sample the whole clip so the framing is stable from the first frame
    // instead of slowly widening as playback reaches new extremes.
    const float savedTime = GetTime();
    const int samples = 24;

    for (int i = 0; i < samples; ++i) {
        Seek(duration * static_cast<float>(i) / static_cast<float>(samples));
        AccumulateContentBounds();
    }

    Seek(savedTime);
}

void SpinePlayer::SetSpeed(float speed) {
    speed_ = speed;
}

float SpinePlayer::GetSpeed() const {
    return speed_;
}

void SpinePlayer::Update(float deltaTime) {
    if (!IsLoaded()) {
        return;
    }

    // Update(0) still applies the current pose, which keeps scrubbing working
    // while playback is paused.
    asset_->GetRuntime()->Update(playing_ ? deltaTime * speed_ : 0.0f);
}

void SpinePlayer::Seek(float time) {
    if (!IsLoaded()) {
        return;
    }

    asset_->GetRuntime()->Seek(time);
}

float SpinePlayer::GetTime() const {
    return IsLoaded() ? asset_->GetRuntime()->GetTime() : 0.0f;
}

float SpinePlayer::GetDuration() const {
    return IsLoaded() ? asset_->GetRuntime()->GetDuration() : 0.0f;
}

void SpinePlayer::CreateGLObjects() {
    if (program_ != 0) {
        return;
    }

    unsigned int vertexShader = CompileShader(GL_VERTEX_SHADER, kVertexShader);
    unsigned int fragmentShader = CompileShader(GL_FRAGMENT_SHADER, kFragmentShader);

    if (vertexShader == 0 || fragmentShader == 0) {
        return;
    }

    program_ = glCreateProgram();
    glAttachShader(program_, vertexShader);
    glAttachShader(program_, fragmentShader);
    glLinkProgram(program_);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    int status = 0;
    glGetProgramiv(program_, GL_LINK_STATUS, &status);

    if (status == 0) {
        char log[1024];
        glGetProgramInfoLog(program_, sizeof(log), nullptr, log);
        std::fprintf(stderr, "Spine program link failed: %s\n", log);

        glDeleteProgram(program_);
        program_ = 0;
        return;
    }

    // The checkerboard background needs no attributes, but core profile still
    // requires a bound VAO.
    const unsigned int gridVertexShader =
        CompileShader(GL_VERTEX_SHADER, kGridVertexShader);
    const unsigned int gridFragmentShader =
        CompileShader(GL_FRAGMENT_SHADER, kGridFragmentShader);

    if (gridVertexShader != 0 && gridFragmentShader != 0) {
        gridProgram_ = glCreateProgram();
        glAttachShader(gridProgram_, gridVertexShader);
        glAttachShader(gridProgram_, gridFragmentShader);
        glLinkProgram(gridProgram_);

        int gridStatus = 0;
        glGetProgramiv(gridProgram_, GL_LINK_STATUS, &gridStatus);

        if (gridStatus == 0) {
            char log[1024];
            glGetProgramInfoLog(gridProgram_, sizeof(log), nullptr, log);
            std::fprintf(stderr, "Grid program link failed: %s\n", log);

            glDeleteProgram(gridProgram_);
            gridProgram_ = 0;
        }
    }

    glDeleteShader(gridVertexShader);
    glDeleteShader(gridFragmentShader);

    glGenVertexArrays(1, &gridVertexArray_);

    glGenVertexArrays(1, &vertexArray_);
    glGenBuffers(1, &vertexBuffer_);
    glGenBuffers(1, &indexBuffer_);

    glBindVertexArray(vertexArray_);

    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(SpineVertex),
                          reinterpret_cast<void*>(offsetof(SpineVertex, x)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(SpineVertex),
                          reinterpret_cast<void*>(offsetof(SpineVertex, u)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(SpineVertex),
                          reinterpret_cast<void*>(offsetof(SpineVertex, r)));

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer_);

    glBindVertexArray(0);
}

void SpinePlayer::DestroyGLObjects() {
    if (vertexArray_ != 0) {
        glDeleteVertexArrays(1, &vertexArray_);
        vertexArray_ = 0;
    }

    if (vertexBuffer_ != 0) {
        glDeleteBuffers(1, &vertexBuffer_);
        vertexBuffer_ = 0;
    }

    if (indexBuffer_ != 0) {
        glDeleteBuffers(1, &indexBuffer_);
        indexBuffer_ = 0;
    }

    if (program_ != 0) {
        glDeleteProgram(program_);
        program_ = 0;
    }

    if (gridProgram_ != 0) {
        glDeleteProgram(gridProgram_);
        gridProgram_ = 0;
    }

    if (gridVertexArray_ != 0) {
        glDeleteVertexArrays(1, &gridVertexArray_);
        gridVertexArray_ = 0;
    }

    vertexCapacity_ = 0;
    indexCapacity_ = 0;
}

bool SpinePlayer::GetCamera(int width, int height, const SpineView& view,
                            float& boundsCenterX, float& boundsCenterY,
                            float& scale) const {
    if (!IsLoaded() || width <= 0 || height <= 0) {
        return false;
    }

    float boundsX = 0.0f;
    float boundsY = 0.0f;
    float boundsWidth = 1.0f;
    float boundsHeight = 1.0f;

    // Prefer the rectangle the animation actually covers. The bounds stored in
    // the skeleton data are the size of the editor canvas the asset was
    // authored on (SkeletonJson reads them straight out of the "skeleton"
    // object), which can be several times larger than the skeleton and is
    // centred on the canvas rather than on the skeleton.
    float minX = 0.0f;
    float minY = 0.0f;
    float maxX = 0.0f;
    float maxY = 0.0f;

    if (GetContentBounds(minX, minY, maxX, maxY)) {
        boundsX = minX;
        boundsY = minY;
        boundsWidth = maxX - minX;
        boundsHeight = maxY - minY;
    } else {
        asset_->GetBounds(boundsX, boundsY, boundsWidth, boundsHeight);
    }

    if (boundsWidth < 1.0f) {
        boundsWidth = 1.0f;
    }

    if (boundsHeight < 1.0f) {
        boundsHeight = 1.0f;
    }

    // Skeleton space and framebuffer space are both y-up.
    boundsCenterX = boundsX + boundsWidth * 0.5f;
    boundsCenterY = boundsY + boundsHeight * 0.5f;

    const float padding = view.padding < 0.0f ? 0.0f
                          : view.padding > 0.9f ? 0.9f
                                                : view.padding;

    // Contain leaves the limiting axis just inside the canvas; Fill covers
    // both axes and lets the excess spill out.
    const float fitScale = view.fit == SpineView::Fit_Fill
                               ? std::max(static_cast<float>(width) / boundsWidth,
                                          static_cast<float>(height) / boundsHeight)
                               : std::min(static_cast<float>(width) / boundsWidth,
                                          static_cast<float>(height) / boundsHeight);

    // In Contain the padding pulls the content away from the canvas edge; in
    // Fill it pushes past it, which is what guarantees no empty band.
    const float paddingScale = view.fit == SpineView::Fit_Fill ? (1.0f + padding)
                                                               : (1.0f - padding);

    scale = fitScale * paddingScale *
            (view.zoom > 0.01f ? view.zoom : 0.01f);

    return true;
}

void SpinePlayer::DrawGrid(int width, int height, float centerX, float centerY,
                           float scale, const SpineView& view) {
    if (gridProgram_ == 0) {
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        return;
    }

    // Keep the cell size in skeleton units constant so the grid behaves like
    // Spine's: it grows when you zoom in, and is doubled when zooming out
    // would make it smaller than a few pixels (which would just look like
    // noise).
    float cell = view.gridSize * view.zoom;

    while (cell < 8.0f) {
        cell *= 2.0f;
    }

    // Canvas position of skeleton-space origin: worldToPixel(x) is
    // x * scale + (width * 0.5 - centerX * scale).
    const float originX = static_cast<float>(width) * 0.5f - centerX * scale;
    const float originY = static_cast<float>(height) * 0.5f - centerY * scale;

    glUseProgram(gridProgram_);
    glBindVertexArray(gridVertexArray_);
    glUniform2f(glGetUniformLocation(gridProgram_, "u_origin"), originX, originY);
    glUniform1f(glGetUniformLocation(gridProgram_, "u_cell"), cell);
    glUniform3f(glGetUniformLocation(gridProgram_, "u_light"), 0.76f, 0.76f,
                0.76f);
    glUniform3f(glGetUniformLocation(gridProgram_, "u_dark"), 0.66f, 0.66f,
                0.66f);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

void SpinePlayer::Draw(int width, int height, float background[4],
                       const SpineView& view) {
    if (!IsLoaded() || program_ == 0 || width <= 0 || height <= 0) {
        return;
    }

    const bool premultipliedAlpha = asset_->UsesPremultipliedAlpha();

    glViewport(0, 0, width, height);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);

    float boundsCenterX = 0.0f;
    float boundsCenterY = 0.0f;
    float scale = 1.0f;

    GetCamera(width, height, view, boundsCenterX, boundsCenterY, scale);

    // Skeleton space and framebuffer space are both y-up.
    const float centerX = boundsCenterX + view.offsetX;
    const float centerY = boundsCenterY + view.offsetY;

    if (view.grid) {
        DrawGrid(width, height, centerX, centerY, scale, view);
    } else {
        glClearColor(background[0], background[1], background[2],
                     background[3]);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    glEnable(GL_BLEND);

    const float halfWidth = static_cast<float>(width) * 0.5f / scale;
    const float halfHeight = static_cast<float>(height) * 0.5f / scale;

    glUseProgram(program_);
    glUniform2f(glGetUniformLocation(program_, "u_center"), centerX, centerY);
    glUniform2f(glGetUniformLocation(program_, "u_halfSize"), halfWidth, halfHeight);
    glUniform1i(glGetUniformLocation(program_, "u_texture"), 0);

    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(vertexArray_);

    std::vector<SpineVertex> vertices;

    for (const SpineDrawBatch& batch : asset_->GetRuntime()->BuildDrawBatches()) {
        if (batch.positions.empty() || batch.indices.empty() ||
            batch.texture == nullptr) {
            continue;
        }

        const size_t vertexCount = batch.positions.size() / 2;

        switch (batch.blendMode) {
            case SpineBlend_Additive:
                if (premultipliedAlpha) {
                    glBlendFuncSeparate(GL_ONE, GL_ONE, GL_ONE, GL_ONE);
                } else {
                    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_ONE, GL_ONE);
                }
                break;
            case SpineBlend_Multiply:
                glBlendFuncSeparate(GL_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA, GL_ONE,
                                    GL_ONE_MINUS_SRC_ALPHA);
                break;
            case SpineBlend_Screen:
                glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_COLOR, GL_ONE,
                                    GL_ONE_MINUS_SRC_COLOR);
                break;
            case SpineBlend_Normal:
            default:
                if (premultipliedAlpha) {
                    glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE,
                                        GL_ONE_MINUS_SRC_ALPHA);
                } else {
                    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
                                        GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
                }
                break;
        }

        vertices.clear();
        vertices.reserve(vertexCount);

        for (size_t i = 0; i < vertexCount; ++i) {
            SpineVertex vertex;
            vertex.x = batch.positions[i * 2];
            vertex.y = batch.positions[i * 2 + 1];
            vertex.u = batch.uvs[i * 2];
            vertex.v = batch.uvs[i * 2 + 1];

            // Keep the fitted rectangle in step with poses that were never
            // sampled, e.g. reached by dragging the timeline.
            if (!hasContentBounds_) {
                contentMinX_ = vertex.x;
                contentMaxX_ = vertex.x;
                contentMinY_ = vertex.y;
                contentMaxY_ = vertex.y;
                hasContentBounds_ = true;
            } else {
                if (vertex.x < contentMinX_) {
                    contentMinX_ = vertex.x;
                }

                if (vertex.x > contentMaxX_) {
                    contentMaxX_ = vertex.x;
                }

                if (vertex.y < contentMinY_) {
                    contentMinY_ = vertex.y;
                }

                if (vertex.y > contentMaxY_) {
                    contentMaxY_ = vertex.y;
                }
            }

            const unsigned int color = batch.colors[i];
            vertex.r = static_cast<unsigned char>((color >> 16) & 0xff);
            vertex.g = static_cast<unsigned char>((color >> 8) & 0xff);
            vertex.b = static_cast<unsigned char>(color & 0xff);
            vertex.a = static_cast<unsigned char>((color >> 24) & 0xff);

            vertices.push_back(vertex);
        }

        const GLsizeiptr vertexBytes =
            static_cast<GLsizeiptr>(vertices.size() * sizeof(SpineVertex));
        const GLsizeiptr indexBytes =
            static_cast<GLsizeiptr>(batch.indices.size() * sizeof(unsigned short));

        glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer_);

        if (static_cast<int>(vertices.size()) > vertexCapacity_) {
            vertexCapacity_ = static_cast<int>(vertices.size()) * 2;
            glBufferData(GL_ARRAY_BUFFER,
                         static_cast<GLsizeiptr>(vertexCapacity_) * sizeof(SpineVertex),
                         nullptr, GL_DYNAMIC_DRAW);
        }

        glBufferSubData(GL_ARRAY_BUFFER, 0, vertexBytes, vertices.data());

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer_);

        if (static_cast<int>(batch.indices.size()) > indexCapacity_) {
            indexCapacity_ = static_cast<int>(batch.indices.size()) * 2;
            glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                         static_cast<GLsizeiptr>(indexCapacity_) * sizeof(unsigned short),
                         nullptr, GL_DYNAMIC_DRAW);
        }

        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, indexBytes,
                        batch.indices.data());

        glBindTexture(GL_TEXTURE_2D,
                      static_cast<SpineTexture*>(batch.texture)->id);

        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(batch.indices.size()),
                       GL_UNSIGNED_SHORT, nullptr);
    }

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
    glDisable(GL_BLEND);
}
