#include "game.hpp"
#ifdef __ANDROID__
#include <SDL_main.h>   // Android вызывает SDL_main
#endif
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <sstream>
#include <functional>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

// Клавиатура (отладка на ПК) и геймпад R36S → логические кнопки
static Btn fromKey(SDL_Keycode k, bool& ok) {
    ok = true;
    switch (k) {
        case SDLK_UP: return UP; case SDLK_DOWN: return DOWN; case SDLK_LEFT: return LEFT; case SDLK_RIGHT: return RIGHT;
        case SDLK_z: return A; case SDLK_x: return B; case SDLK_a: return X; case SDLK_s: return Y;
        case SDLK_q: return L1; case SDLK_w: return R1; case SDLK_RETURN: return START; case SDLK_RSHIFT: return SELECT;
    }
    ok = false; return A;
}
static Btn fromPad(Uint8 b, bool& ok) {
    ok = true;
    switch (b) {
        case SDL_CONTROLLER_BUTTON_DPAD_UP: return UP; case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return DOWN;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return LEFT; case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return RIGHT;
        case SDL_CONTROLLER_BUTTON_B: return A; case SDL_CONTROLLER_BUTTON_A: return B;  // раскладка Nintendo: A справа
        case SDL_CONTROLLER_BUTTON_Y: return X; case SDL_CONTROLLER_BUTTON_X: return Y;
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return L1; case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return R1;
        case SDL_CONTROLLER_BUTTON_START: return START; case SDL_CONTROLLER_BUTTON_BACK: return SELECT;
    }
    ok = false; return A;
}

// Виртуальный геймпад для касаний: центры и радиусы в логических 640x480
struct PadBtn { Btn b; int x, y, r; const char* label; };
static const PadBtn PADS[] = {
    {UP, 72, 328, 26, "\xE2\x86\x91"}, {DOWN, 72, 416, 26, "\xE2\x86\x93"},
    {LEFT, 28, 372, 26, "\xE2\x86\x90"}, {RIGHT, 116, 372, 26, "\xE2\x86\x92"},
    {Y, 560, 330, 24, "Y"}, {X, 516, 372, 24, "X"}, {A, 604, 372, 24, "A"}, {B, 560, 414, 24, "B"},
    {SELECT, 236, 420, 18, "SEL"}, {START, 404, 420, 18, "START"},
};

// Нормализованные координаты касания → логические координаты игры
static std::pair<float, float> toLogical(Gfx& g, float nx, float ny) {
    int pw = 0, ph = 0; SDL_GetRendererOutputSize(g.renderer(), &pw, &ph);
    int wx = (int)(nx * pw), wy = (int)(ny * ph); float lx = 0, ly = 0;
    SDL_RenderWindowToLogical(g.renderer(), wx, wy, &lx, &ly);
    return {lx, ly};
}

// Состояние игры — глобальное, чтобы главный цикл работал и на ПК, и в браузере (Emscripten)
static Gfx gfx;
static Game game;
static Input in;
static bool hw[BTN_COUNT] = {};         // клавиатура и геймпад
static bool touchHeld[BTN_COUNT] = {};  // касания
static std::map<SDL_FingerID, std::pair<float, float>> fingers;
static bool touchSeen = false, headless = false, debugMode = false, g_done = false;
static int maxFrames = 0, frame = 0;
static Uint32 last = 0;
static std::multimap<int, Btn> keyScript;
static std::multimap<int, std::string> shotScript;

static void frameStep() {
    in.beginFrame();
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        bool ok;
        if (e.type == SDL_QUIT) g_done = true;
        else if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) { if (e.key.repeat) continue; Btn b = fromKey(e.key.keysym.sym, ok); if (ok) hw[b] = e.type == SDL_KEYDOWN; }
        else if (e.type == SDL_CONTROLLERBUTTONDOWN || e.type == SDL_CONTROLLERBUTTONUP) { Btn b = fromPad(e.cbutton.button, ok); if (ok) hw[b] = e.type == SDL_CONTROLLERBUTTONDOWN; }
        else if (e.type == SDL_FINGERDOWN || e.type == SDL_FINGERMOTION) { touchSeen = true; fingers[e.tfinger.fingerId] = toLogical(gfx, e.tfinger.x, e.tfinger.y); }
        else if (e.type == SDL_FINGERUP) fingers.erase(e.tfinger.fingerId);
    }
    // пальцы на кнопках виртуального геймпада → те же логические кнопки
    for (bool& t : touchHeld) t = false;
    for (auto& f : fingers) for (auto& p : PADS) {
        float dx = f.second.first - p.x, dy = f.second.second - p.y;
        if (dx * dx + dy * dy <= (float)(p.r * p.r)) touchHeld[p.b] = true;
    }
    for (int b = 0; b < BTN_COUNT; b++) in.set((Btn)b, hw[b] || touchHeld[b]);
    for (auto r = keyScript.equal_range(frame); r.first != r.second; ++r.first) in.pressed[r.first->second] = true;

    Uint32 now = SDL_GetTicks();
    float dt = headless ? 1.0f / 30 : (now - last) / 1000.0f; last = now; if (dt > 0.1f) dt = 0.1f;
    game.update(in, dt); game.draw(gfx);
    if (touchSeen) for (auto& p : PADS) {
        gfx.disc(p.x, p.y, p.r, {255, 255, 255, 60});
        gfx.text(p.label, p.x, p.y - 9, 14, {255, 255, 255, 220}, 1);
    }
    for (auto r = shotScript.equal_range(frame); r.first != r.second; ++r.first) gfx.screenshot(r.first->second);
    gfx.present(); frame++;
    if (debugMode && frame % 300 == 0) fprintf(stderr, "%s\n", game.stats().c_str());
    if (game.quit || (maxFrames && frame >= maxFrames)) g_done = true;
    if (!headless) SDL_Delay(1);
}

#ifdef __EMSCRIPTEN__
static void emLoop() { frameStep(); if (g_done) emscripten_cancel_main_loop(); }
#endif

int main(int argc, char** argv) {
    std::string dataDir = "data", fontPath = "assets/fonts/ui.ttf", script, shots;
    bool forceTouch = false;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--headless") headless = true;
        else if (a == "--debug") debugMode = true;
        else if (a == "--touch") forceTouch = true;
        else if (a == "--data" && i + 1 < argc) dataDir = argv[++i];
        else if (a == "--script" && i + 1 < argc) script = argv[++i];   // "30:X 40:DOWN"
        else if (a == "--shots" && i + 1 < argc) shots = argv[++i];     // "60:out.png"
        else if (a == "--frames" && i + 1 < argc) maxFrames = atoi(argv[++i]);
    }
    touchSeen = forceTouch;
    if (!gfx.init(headless)) return 1;
    if (!gfx.loadFont(fontPath)) { fprintf(stderr, "нет шрифта %s\n", fontPath.c_str()); return 1; }
    if (!game.init(dataDir)) return 1;
    if (debugMode) game.debugUnlock();
    SDL_GameController* pad = nullptr;
    for (int i = 0; i < SDL_NumJoysticks(); i++) if (SDL_IsGameController(i)) { pad = SDL_GameControllerOpen(i); break; }
    (void)pad;

    { std::stringstream ss(script); std::string t; while (ss >> t) { auto p = t.find(':'); Btn b; if (p != std::string::npos && btnFromName(t.substr(p + 1), b)) keyScript.insert({atoi(t.c_str()), b}); } }
    { std::stringstream ss(shots); std::string t; while (ss >> t) { auto p = t.find(':'); if (p != std::string::npos) shotScript.insert({atoi(t.c_str()), t.substr(p + 1)}); } }

    last = SDL_GetTicks();
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(emLoop, 0, 1);   // браузер сам вызывает кадры
#else
    while (!g_done) frameStep();
    gfx.shutdown();
#endif
    return 0;
}
