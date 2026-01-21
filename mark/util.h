#pragma once
#include <stdbool.h>
#include <SDL3/SDL.h>

static inline float clampf(float v, float minv, float maxv) {
    if (v < minv) return minv;
    if (v > maxv) return maxv;
    return v;
}

static inline bool pressed(bool now, bool* prev) {
    bool p = (now && !(*prev));
    *prev = now;
    return p;
}

static inline void draw_rect(SDL_Renderer* r, float x, float y, float w, float h) {
    SDL_FRect fr = { x, y, w, h };
    SDL_RenderFillRect(r, &fr);
}

static inline void draw_frame(SDL_Renderer* r, float x, float y, float w, float h) {
    SDL_FRect fr = { x, y, w, h };
    SDL_RenderRect(r, &fr);
}
