#pragma once
#include <SDL.h>
#include <string>
#include <unordered_map>
#include <vector>
struct Color { Uint8 r, g, b, a = 255; };
class Gfx {
public:
    static constexpr int W = 640, H = 480;
    bool init(bool headless);
    void shutdown();
    void clear(Color c);
    void fill(int x, int y, int w, int h, Color c);
    void frame(int x, int y, int w, int h, Color c, int t = 1);
    void panel(int x, int y, int w, int h, Color fill, Color border);
    // size в пикселях; align: 0 слева, 1 по центру, 2 справа
    void text(const std::string& s, int x, int y, int size, Color c, int align = 0);
    int textWidth(const std::string& s, int size);
    // кружок кнопки геймпада с буквой, как на консоли
    void button(int cx, int cy, const std::string& label, Color c);
    void disc(int cx, int cy, int r, Color c);
    void present();
    bool screenshot(const std::string& path);
    bool loadFont(const std::string& path);
    SDL_Renderer* renderer() { return ren; }
private:
    SDL_Window* win = nullptr; SDL_Renderer* ren = nullptr;
    std::vector<unsigned char> fontData; void* fontInfo = nullptr;
    struct Tex { SDL_Texture* t; int w, h; };
    std::unordered_map<std::string, Tex> cache;
    Tex& getText(const std::string& s, int size);
};
