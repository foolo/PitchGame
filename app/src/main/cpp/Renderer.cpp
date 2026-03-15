#include "Renderer.h"

#include <game-activity/native_app_glue/android_native_app_glue.h>
#include <GLES3/gl3.h>
#include <memory>
#include <vector>
#include <android/imagedecoder.h>
#include <ctime>
#include <cmath>
#include <numeric>
#include <algorithm>
#include <cstdlib>

#include "AndroidOut.h"
#include "Shader.h"
#include "Utility.h"
#include "TextureAsset.h"
#include "Audio.h"
#include "Constants.h"

//! Color for cornflower blue. Can be sent directly to glClearColor
#define CORNFLOWER_BLUE 100 / 255.f, 149 / 255.f, 237 / 255.f, 1

// Vertex shader
static const char *vertex = R"vertex(#version 300 es
in vec3 inPosition;
in vec2 inUV;

out vec2 fragUV;

uniform mat4 uProjection;
uniform vec2 uOffset;
uniform float uScale;

void main() {
    fragUV = inUV;
    gl_Position = uProjection * vec4(inPosition.x * uScale + uOffset.x, inPosition.y * uScale + uOffset.y, inPosition.z, 1.0);
}
)vertex";

// Fragment shader
static const char *fragment = R"fragment(#version 300 es
precision mediump float;

in vec2 fragUV;

out vec4 outColor;

uniform vec4 uColor;

void main() {
    float dist = distance(fragUV, vec2(0.5, 0.5));
    if (dist > 0.5) {
        discard;
    }
    outColor = uColor;
}
)fragment";

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

void Renderer::render(const Game& game) {
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

    Vector2 playerPos = game.getPlayerPos();
    Vector2 cameraPos = game.getCameraPos();

    // Update UI Debug Info via JNI
    if (updateDebugInfoMethodId_ != nullptr) {
        JNIEnv *env;
        app_->activity->vm->AttachCurrentThread(&env, nullptr);
        env->CallVoidMethod(app_->activity->javaGameActivity, updateDebugInfoMethodId_,
                            playerPos.x, playerPos.y, Audio_getLastRms(), Audio_getLastPitch());
    }

    glClear(GL_COLOR_BUFFER_BIT);

    if (!models_.empty()) {
        shader_->activate();
        GLint colorLoc = glGetUniformLocation(shader_->getProgram(), "uColor");
        GLint scaleLoc = glGetUniformLocation(shader_->getProgram(), "uScale");

        // 1. Farthest Layer: Clouds (Parallax factor 0.2)
        glUniform4f(colorLoc, 1.0f, 1.0f, 1.0f, 0.5f); // White, semi-transparent
        glUniform1f(scaleLoc, 3.0f); // Big circles
        for (const auto &objPos: game.getCloudObjects()) {
            shader_->setOffset(objPos.x - cameraPos.x * 0.2f, objPos.y);
            shader_->drawModel(models_[0]);
        }

        // 2. Mid Layer: Trees (Parallax factor 0.5)
        glUniform4f(colorLoc, 0.1f, 0.6f, 0.1f, 1.0f); // Green
        glUniform1f(scaleLoc, 1.5f);
        for (const auto &objPos: game.getTreeObjects()) {
            shader_->setOffset(objPos.x - cameraPos.x * 0.5f, objPos.y - 0.5f);
            shader_->drawModel(models_[0]);
        }

        // 3. Static objects (Parallax factor 1.0 - same as player)
        glUniform4f(colorLoc, 0.7f, 0.7f, 0.7f, 1.0f); // Grey
        glUniform1f(scaleLoc, 1.0f);
        for (const auto &objPos: game.getStaticObjects()) {
            shader_->setOffset(objPos.x - cameraPos.x, objPos.y - cameraPos.y);
            shader_->drawModel(models_[0]);
        }

        // 4. Player
        glUniform4f(colorLoc, 1.0f, 1.0f, 0.0f, 1.0f); // Yellow
        glUniform1f(scaleLoc, 1.0f);
        shader_->setOffset(playerPos.x - cameraPos.x, playerPos.y - cameraPos.y);
        shader_->drawModel(models_[0]);
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
    updateDebugInfoMethodId_ = env->GetMethodID(clazz, "updateDebugInfo", "(FFFF)V");

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
    float playerRadius = 0.2f; // Should probably match Game's playerRadius_
    // Create a square that will be rendered as a circle by the fragment shader
    std::vector<Vertex> vertices = {
            Vertex(Vector3{playerRadius, playerRadius, 0}, Vector2{1, 1}),
            Vertex(Vector3{-playerRadius, playerRadius, 0}, Vector2{0, 1}),
            Vertex(Vector3{-playerRadius, -playerRadius, 0}, Vector2{0, 0}),
            Vertex(Vector3{playerRadius, -playerRadius, 0}, Vector2{1, 0})
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
