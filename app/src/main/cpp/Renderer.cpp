#include "Renderer.h"

#include <game-activity/native_app_glue/android_native_app_glue.h>
#include <GLES3/gl3.h>
#include <memory>
#include <vector>
#include <android/imagedecoder.h>
#include <time.h>

#include "AndroidOut.h"
#include "Shader.h"
#include "Utility.h"
#include "TextureAsset.h"

//! executes glGetString and outputs the result to logcat
#define PRINT_GL_STRING(s) {aout << #s": "<< glGetString(s) << std::endl;}

/*!
 * @brief if glGetString returns a space separated list of elements, prints each one on a new line
 */
#define PRINT_GL_STRING_AS_LIST(s) { \
std::istringstream extensionStream((const char *) glGetString(s));\
std::vector<std::string> extensionList(\
        std::istream_iterator<std::string>{extensionStream},\
        std::istream_iterator<std::string>());\
aout << #s":\n";\
for (auto& extension: extensionList) {\
    aout << extension << "\n";\
}\
aout << std::endl;\
}

//! Color for cornflower blue. Can be sent directly to glClearColor
#define CORNFLOWER_BLUE 100 / 255.f, 149 / 255.f, 237 / 255.f, 1

// Vertex shader
static const char *vertex = R"vertex(#version 300 es
in vec3 inPosition;
in vec2 inUV;

out vec2 fragUV;

uniform mat4 uProjection;
uniform vec2 uOffset;

void main() {
    fragUV = inUV;
    gl_Position = uProjection * vec4(inPosition.x + uOffset.x, inPosition.y + uOffset.y, inPosition.z, 1.0);
}
)vertex";

// Fragment shader
static const char *fragment = R"fragment(#version 300 es
precision mediump float;

in vec2 fragUV;

out vec4 outColor;

void main() {
    float dist = distance(fragUV, vec2(0.5, 0.5));
    if (dist > 0.5) {
        discard;
    }
    outColor = vec4(1.0, 1.0, 0.0, 1.0); // Yellow
}
)fragment";

/*!
 * Half the height of the projection matrix. This gives you a renderable area of height 4 ranging
 * from -2 to 2
 */
static constexpr float kProjectionHalfHeight = 2.f;
static constexpr float kProjectionNearPlane = -1.f;
static constexpr float kProjectionFarPlane = 1.f;

Renderer::~Renderer() {
    if (display_ != EGL_NO_DISPLAY) {
        eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (context_ != EGL_NO_CONTEXT) {
            eglDestroyContext(display_, context_);
            context_ = EGL_NO_CONTEXT;
        }
        if (surface_ != EGL_NO_SURFACE) {
            eglDestroySurface(display_, surface_);
            surface_ = EGL_NO_SURFACE;
        }
        eglTerminate(display_);
        display_ = EGL_NO_DISPLAY;
    }
}

void Renderer::render() {
    updateRenderArea();

    if (shaderNeedsNewProjectionMatrix_) {
        float projectionMatrix[16] = {0};
        Utility::buildOrthographicMatrix(
                projectionMatrix,
                kProjectionHalfHeight,
                float(width_) / height_,
                kProjectionNearPlane,
                kProjectionFarPlane);

        shader_->setProjectionMatrix(projectionMatrix);
        shaderNeedsNewProjectionMatrix_ = false;
    }

    // Calculate delta time
    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    uint64_t nowNs = (uint64_t)now.tv_sec * 1000000000ull + (uint64_t)now.tv_nsec;
    if (lastTimeNs_ == 0) lastTimeNs_ = nowNs;
    float dt = (float)(nowNs - lastTimeNs_) * 1e-9f;
    lastTimeNs_ = nowNs;

    // Update ball position
    ballPos_.x += ballVel_.x * dt;
    ballPos_.y += ballVel_.y * dt;

    // Bounce off walls
    float aspect = float(width_) / height_;
    float maxX = kProjectionHalfHeight * aspect;
    float maxY = kProjectionHalfHeight;

    if (ballPos_.x + ballRadius_ > maxX) {
        ballPos_.x = maxX - ballRadius_;
        ballVel_.x *= -1.0f;
    } else if (ballPos_.x - ballRadius_ < -maxX) {
        ballPos_.x = -maxX + ballRadius_;
        ballVel_.x *= -1.0f;
    }

    if (ballPos_.y + ballRadius_ > maxY) {
        ballPos_.y = maxY - ballRadius_;
        ballVel_.y *= -1.0f;
    } else if (ballPos_.y - ballRadius_ < -maxY) {
        ballPos_.y = -maxY + ballRadius_;
        ballVel_.y *= -1.0f;
    }

    // Update UI Debug Info via JNI
    if (updateDebugInfoMethodId_ != nullptr) {
        JNIEnv *env;
        app_->activity->vm->AttachCurrentThread(&env, nullptr);
        env->CallVoidMethod(app_->activity->javaGameActivity, updateDebugInfoMethodId_, ballPos_.x, ballPos_.y);
        // In a real app, you might want to detach if you're creating new threads,
        // but for the main loop it's often kept attached.
    }

    glClear(GL_COLOR_BUFFER_BIT);

    if (!models_.empty()) {
        shader_->activate();
        shader_->setOffset(ballPos_.x, ballPos_.y);

        for (const auto &model: models_) {
            shader_->drawModel(model);
        }
    }

    auto swapResult = eglSwapBuffers(display_, surface_);
    assert(swapResult == EGL_TRUE);
}

void Renderer::initRenderer() {
    constexpr EGLint attribs[] = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_BLUE_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_RED_SIZE, 8,
            EGL_DEPTH_SIZE, 24,
            EGL_NONE
    };

    auto display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(display, nullptr, nullptr);

    EGLint numConfigs;
    eglChooseConfig(display, attribs, nullptr, 0, &numConfigs);

    std::unique_ptr<EGLConfig[]> supportedConfigs(new EGLConfig[numConfigs]);
    eglChooseConfig(display, attribs, supportedConfigs.get(), numConfigs, &numConfigs);

    auto config = *std::find_if(
            supportedConfigs.get(),
            supportedConfigs.get() + numConfigs,
            [&display](const EGLConfig &config) {
                EGLint red, green, blue, depth;
                if (eglGetConfigAttrib(display, config, EGL_RED_SIZE, &red)
                    && eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &red)) {
                    // This check was a bit weird in the original, just making sure we have a config
                }
                EGLint r, g, b, d;
                if (eglGetConfigAttrib(display, config, EGL_RED_SIZE, &r)
                    && eglGetConfigAttrib(display, config, EGL_GREEN_SIZE, &g)
                    && eglGetConfigAttrib(display, config, EGL_BLUE_SIZE, &b)
                    && eglGetConfigAttrib(display, config, EGL_DEPTH_SIZE, &d)) {
                    return r == 8 && g == 8 && b == 8 && d == 24;
                }
                return false;
            });

    EGLint format;
    eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &format);
    EGLSurface surface = eglCreateWindowSurface(display, config, app_->window, nullptr);

    EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    EGLContext context = eglCreateContext(display, config, nullptr, contextAttribs);

    auto madeCurrent = eglMakeCurrent(display, surface, surface, context);
    assert(madeCurrent);

    display_ = display;
    surface_ = surface;
    context_ = context;

    width_ = -1;
    height_ = -1;

    shader_ = std::unique_ptr<Shader>(
            Shader::loadShader(vertex, fragment, "inPosition", "inUV", "uProjection", "uOffset"));
    assert(shader_);

    shader_->activate();

    glClearColor(CORNFLOWER_BLUE);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Initialize JNI method ID
    JNIEnv *env;
    app_->activity->vm->AttachCurrentThread(&env, nullptr);
    jclass clazz = env->GetObjectClass(app_->activity->javaGameActivity);
    updateDebugInfoMethodId_ = env->GetMethodID(clazz, "updateDebugInfo", "(FF)V");

    createModels();
}

void Renderer::updateRenderArea() {
    EGLint width;
    eglQuerySurface(display_, surface_, EGL_WIDTH, &width);

    EGLint height;
    eglQuerySurface(display_, surface_, EGL_HEIGHT, &height);

    if (width != width_ || height != height_) {
        width_ = width;
        height_ = height;
        glViewport(0, 0, width, height);
        shaderNeedsNewProjectionMatrix_ = true;
    }
}

void Renderer::createModels() {
    // Create a square that will be rendered as a circle by the fragment shader
    std::vector<Vertex> vertices = {
            Vertex(Vector3{ballRadius_, ballRadius_, 0}, Vector2{1, 1}),
            Vertex(Vector3{-ballRadius_, ballRadius_, 0}, Vector2{0, 1}),
            Vertex(Vector3{-ballRadius_, -ballRadius_, 0}, Vector2{0, 0}),
            Vertex(Vector3{ballRadius_, -ballRadius_, 0}, Vector2{1, 0})
    };
    std::vector<Index> indices = {
            0, 1, 2, 0, 2, 3
    };

    models_.emplace_back(vertices, indices, nullptr);
}

void Renderer::handleInput() {
    auto *inputBuffer = android_app_swap_input_buffers(app_);
    if (!inputBuffer) return;
    android_app_clear_motion_events(inputBuffer);
    android_app_clear_key_events(inputBuffer);
}
