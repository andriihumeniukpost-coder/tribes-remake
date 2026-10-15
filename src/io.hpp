#pragma once
#include <SDL.h>
#include <string>
// Чтение файла через SDL: на Android ресурсы лежат внутри APK, обычный fopen их не видит.
inline bool readFileBytes(const std::string& path, std::string& out) {
    SDL_RWops* rw = SDL_RWFromFile(path.c_str(), "rb");
    if (!rw) return false;
    Sint64 size = SDL_RWsize(rw);
    if (size < 0) { SDL_RWclose(rw); return false; }
    out.resize((size_t)size);
    size_t got = size ? SDL_RWread(rw, &out[0], 1, (size_t)size) : 0;
    SDL_RWclose(rw);
    return got == (size_t)size;
}
