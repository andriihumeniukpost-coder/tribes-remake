#include "gfx.hpp"
#define STB_TRUETYPE_IMPLEMENTATION
#include "../third_party/stb_truetype.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../third_party/stb_image_write.h"
#include <cstdio>
#include <cstring>
#include "io.hpp"

bool Gfx::init(bool headless) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_AUDIO) != 0 && SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError()); return false;
    }
    win = SDL_CreateWindow("Prehistoric Tribes: Remake", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, W, H, headless ? SDL_WINDOW_HIDDEN : SDL_WINDOW_SHOWN);
    if (!win) { fprintf(stderr, "window: %s\n", SDL_GetError()); return false; }
    ren = SDL_CreateRenderer(win, -1, headless ? SDL_RENDERER_SOFTWARE : SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    if (!ren) { fprintf(stderr, "renderer: %s\n", SDL_GetError()); return false; }
    SDL_RenderSetLogicalSize(ren, W, H);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    return true;
}
void Gfx::shutdown() {
    for (auto& kv : cache) SDL_DestroyTexture(kv.second.t);
    cache.clear();
    delete (stbtt_fontinfo*)fontInfo;
    if (ren) SDL_DestroyRenderer(ren);
    if (win) SDL_DestroyWindow(win);
    SDL_Quit();
}
bool Gfx::loadFont(const std::string& path) {
    std::string bytes;
    if (!readFileBytes(path, bytes)) return false;
    fontData.assign(bytes.begin(), bytes.end());
    auto* fi = new stbtt_fontinfo();
    if (!stbtt_InitFont(fi, fontData.data(), 0)) { delete fi; return false; }
    fontInfo = fi; return true;
}
void Gfx::clear(Color c) { SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a); SDL_RenderClear(ren); }
void Gfx::fill(int x, int y, int w, int h, Color c) {
    SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a);
    SDL_Rect r{x, y, w, h}; SDL_RenderFillRect(ren, &r);
}
void Gfx::frame(int x, int y, int w, int h, Color c, int t) {
    fill(x, y, w, t, c); fill(x, y + h - t, w, t, c); fill(x, y, t, h, c); fill(x + w - t, y, t, h, c);
}
void Gfx::panel(int x, int y, int w, int h, Color f, Color b) { fill(x, y, w, h, f); frame(x, y, w, h, b, 2); }

static std::vector<int> utf8(const std::string& s) {
    std::vector<int> o; size_t i = 0;
    while (i < s.size()) {
        unsigned char c = s[i];
        int cp, n;
        if (c < 0x80) { cp = c; n = 1; } else if ((c >> 5) == 6) { cp = c & 31; n = 2; } else if ((c >> 4) == 14) { cp = c & 15; n = 3; } else { cp = c & 7; n = 4; }
        for (int k = 1; k < n && i + k < s.size(); k++) cp = (cp << 6) | (s[i + k] & 63);
        o.push_back(cp); i += n;
    }
    return o;
}
Gfx::Tex& Gfx::getText(const std::string& s, int size) {
    std::string key = std::to_string(size) + ":" + s;
    auto it = cache.find(key);
    if (it != cache.end()) return it->second;
    auto* fi = (stbtt_fontinfo*)fontInfo;
    float sc = stbtt_ScaleForPixelHeight(fi, (float)size);
    int asc, desc, gap; stbtt_GetFontVMetrics(fi, &asc, &desc, &gap);
    int base = (int)(asc * sc + 0.5f), h = (int)((asc - desc) * sc + 2);
    auto cps = utf8(s);
    int w = 2; for (size_t i = 0; i < cps.size(); i++) { int adv, lsb; stbtt_GetCodepointHMetrics(fi, cps[i], &adv, &lsb); w += (int)(adv * sc + 0.5f); }
    std::vector<unsigned char> a(w * h, 0);
    int x = 1;
    for (size_t i = 0; i < cps.size(); i++) {
        int adv, lsb; stbtt_GetCodepointHMetrics(fi, cps[i], &adv, &lsb);
        int x0, y0, x1, y1; stbtt_GetCodepointBitmapBox(fi, cps[i], sc, sc, &x0, &y0, &x1, &y1);
        int gw = x1 - x0, gh = y1 - y0;
        if (gw > 0 && gh > 0) {
            std::vector<unsigned char> g(gw * gh);
            stbtt_MakeCodepointBitmap(fi, g.data(), gw, gh, gw, sc, sc, cps[i]);
            for (int yy = 0; yy < gh; yy++) for (int xx = 0; xx < gw; xx++) {
                int px = x + x0 + xx, py = base + y0 + yy;
                if (px >= 0 && px < w && py >= 0 && py < h) { int v = a[py * w + px] + g[yy * gw + xx]; a[py * w + px] = v > 255 ? 255 : v; }
            }
        }
        x += (int)(adv * sc + 0.5f);
    }
    SDL_Surface* sf = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGBA32);
    for (int i = 0; i < w * h; i++) ((Uint32*)sf->pixels)[i] = 0x00FFFFFFu | ((Uint32)a[i] << 24);
    SDL_Texture* t = SDL_CreateTextureFromSurface(ren, sf);
    SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    SDL_FreeSurface(sf);
    return cache[key] = Tex{t, w, h};
}
int Gfx::textWidth(const std::string& s, int size) { return getText(s, size).w; }
void Gfx::text(const std::string& s, int x, int y, int size, Color c, int align) {
    if (s.empty() || !fontInfo) return;
    Tex& t = getText(s, size);
    if (align == 1) x -= t.w / 2; else if (align == 2) x -= t.w;
    SDL_SetTextureColorMod(t.t, c.r, c.g, c.b); SDL_SetTextureAlphaMod(t.t, c.a);
    SDL_Rect d{x, y, t.w, t.h}; SDL_RenderCopy(ren, t.t, nullptr, &d);
}
void Gfx::button(int cx, int cy, const std::string& label, Color c) {
    const int r = 10;
    SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, 255);
    for (int dy = -r; dy <= r; dy++) { int dx = (int)SDL_sqrtf((float)(r * r - dy * dy)); SDL_RenderDrawLine(ren, cx - dx, cy + dy, cx + dx, cy + dy); }
    text(label, cx, cy - 8, 14, {20, 20, 20}, 1);
}
void Gfx::disc(int cx, int cy, int r, Color c) {
    SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a);
    for (int dy = -r; dy <= r; dy++) { int dx = (int)SDL_sqrtf((float)(r * r - dy * dy)); SDL_RenderDrawLine(ren, cx - dx, cy + dy, cx + dx, cy + dy); }
}
void Gfx::present() { SDL_RenderPresent(ren); }
bool Gfx::screenshot(const std::string& path) {
    std::vector<unsigned char> px(W * H * 4);
    if (SDL_RenderReadPixels(ren, nullptr, SDL_PIXELFORMAT_ABGR8888, px.data(), W * 4) != 0) return false;
    return stbi_write_png(path.c_str(), W, H, 4, px.data(), W * 4) != 0;
}
