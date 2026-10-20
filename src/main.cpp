#include "app.hpp"
#include "raylib.h"
#include <memory>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
namespace { std::unique_ptr<stimulus::App> app; void update() { app->tick(); } }
#ifdef __EMSCRIPTEN__
extern "C" EMSCRIPTEN_KEEPALIVE int stimulus_import(const char* text) {
    return app && text && app->import_json(text) ? 1 : 0;
}
#endif
int main() {
    InitWindow(800, 800, "Stimulus");
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);
    app = std::make_unique<stimulus::App>();
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(update, 0, 1);
#else
    while (!WindowShouldClose()) update();
    app->close();
    app.reset();
    CloseWindow();
#endif
    return 0;
}
