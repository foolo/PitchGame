#include <jni.h>
#include <time.h>

#include <game-activity/native_app_glue/android_native_app_glue.h>
#include <game-activity/GameActivity.h>

#include "AndroidOut.h"
#include "Renderer.h"
#include "Game.h"

struct AppContext {
    Renderer* renderer;
    Game* game;

    AppContext(android_app* pApp) {
        renderer = new Renderer(pApp);
        game = new Game();
    }

    ~AppContext() {
        delete renderer;
        delete game;
    }
};

extern "C" {

/*!
 * Handles commands sent to this Android application
 * @param pApp the app the commands are coming from
 * @param cmd the command to handle
 */
void handle_cmd(android_app *pApp, int32_t cmd) {
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            pApp->userData = new AppContext(pApp);
            break;
        case APP_CMD_TERM_WINDOW:
            if (pApp->userData) {
                auto *pContext = reinterpret_cast<AppContext *>(pApp->userData);
                pApp->userData = nullptr;
                delete pContext;
            }
            break;
        default:
            break;
    }
}

/*!
 * Enable the motion events you want to handle; not handled events are
 * passed back to OS for further processing. For this example case,
 * only pointer and joystick devices are enabled.
 *
 * @param motionEvent the newly arrived GameActivityMotionEvent.
 * @return true if the event is from a pointer or joystick device,
 *         false for all other input devices.
 */
bool motion_event_filter_func(const GameActivityMotionEvent *motionEvent) {
    auto sourceClass = motionEvent->source & AINPUT_SOURCE_CLASS_MASK;
    return (sourceClass == AINPUT_SOURCE_CLASS_POINTER ||
            sourceClass == AINPUT_SOURCE_CLASS_JOYSTICK);
}

/*!
 * This the main entry point for a native activity
 */
void android_main(struct android_app *pApp) {
    aout << "Welcome to android_main" << std::endl;

    pApp->onAppCmd = handle_cmd;

    android_app_set_motion_event_filter(pApp, motion_event_filter_func);

    uint64_t lastTimeNs = 0;

    do {
        bool done = false;
        while (!done) {
            int timeout = 0;
            int events;
            android_poll_source *pSource;
            int result = ALooper_pollOnce(timeout, nullptr, &events,
                                          reinterpret_cast<void**>(&pSource));
            switch (result) {
                case ALOOPER_POLL_TIMEOUT:
                    [[clang::fallthrough]];
                case ALOOPER_POLL_WAKE:
                    done = true;
                    break;
                case ALOOPER_EVENT_ERROR:
                    aout << "ALooper_pollOnce returned an error" << std::endl;
                    break;
                case ALOOPER_POLL_CALLBACK:
                    break;
                default:
                    if (pSource) {
                        pSource->process(pApp, pSource);
                    }
            }
        }

        if (pApp->userData) {
            auto *pContext = reinterpret_cast<AppContext *>(pApp->userData);

            // Calculate delta time
            timespec now = {0, 0};
            clock_gettime(CLOCK_MONOTONIC, &now);
            uint64_t nowNs = (uint64_t)now.tv_sec * 1000000000ull + (uint64_t)now.tv_nsec;
            if (lastTimeNs == 0) lastTimeNs = nowNs;
            float dt = (float)(nowNs - lastTimeNs) * 1e-9f;
            lastTimeNs = nowNs;

            // Process game input
            pContext->renderer->handleInput();

            // Update game logic
            pContext->game->update(dt);

            // Render a frame
            pContext->renderer->render(*pContext->game);
        }
    } while (!pApp->destroyRequested);
}
}
