#pragma once
#include <SDL3/SDL.h>

extern "C" {
    SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv);
    SDL_AppResult SDL_AppIterate(void *appstate);
    SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event);
    void SDL_AppQuit(void *appstate, SDL_AppResult result);
}
