#pragma once
#include <string>
// Логические кнопки R36S. Подписи в UI берутся отсюда, чтобы везде были названия кнопок консоли.
enum Btn { UP, DOWN, LEFT, RIGHT, A, B, X, Y, L1, R1, L2, R2, START, SELECT, BTN_COUNT };
struct Input {
    bool down[BTN_COUNT] = {};
    bool pressed[BTN_COUNT] = {};
    void beginFrame() { for (bool& p : pressed) p = false; }
    void set(Btn b, bool v) { if (v && !down[b]) pressed[b] = true; down[b] = v; }
};
inline const char* btnName(Btn b) {
    static const char* n[] = {"UP","DOWN","LEFT","RIGHT","A","B","X","Y","L1","R1","L2","R2","START","SELECT"};
    return n[b];
}
inline bool btnFromName(const std::string& s, Btn& out) {
    for (int i = 0; i < BTN_COUNT; i++) if (s == btnName((Btn)i)) { out = (Btn)i; return true; }
    return false;
}
