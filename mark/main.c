#define SDL_MAIN_HANDLED 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdbool.h>

#include "game.h"

#define WINDOW_W 800
#define WINDOW_H 600

int main(int argc, char* argv[])
{
    (void)argc; (void)argv;

    SDL_SetMainReady();

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("MARK", WINDOW_W, WINDOW_H, 0);
    if (!window) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return 2;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, NULL);
    if (!renderer) {
        SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 3;
    }
    SDL_StartTextInput(window);

    Game game;
    Game_Init(&game, (float)WINDOW_W, (float)WINDOW_H);

    Uint64 last = SDL_GetPerformanceCounter();
    bool running = true;

    while (running) {
        Uint64 now = SDL_GetPerformanceCounter();
        float dt = (float)(now - last) / (float)SDL_GetPerformanceFrequency();
        last = now;

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) running = false;
            Game_HandleEvent(&game, &e);
        }

        const bool* keys = SDL_GetKeyboardState(NULL);
        Game_Update(&game, keys, dt);

        SDL_SetRenderDrawColor(renderer, 15, 15, 18, 255);
        SDL_RenderClear(renderer);

        Game_Render(&game, renderer);

        SDL_RenderPresent(renderer);
    }

    SDL_StopTextInput(window);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
