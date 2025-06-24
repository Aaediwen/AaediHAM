#pragma once
#ifndef AAEDICLOCK
#define AAEDICLOCK
//#define SDL_MAIN_USE_CALLBACKS
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
//#include <unistd.h>
//#include <math.h>

//#include <SDL3/SDL_main.h>
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

extern SDL_Window* window;
extern SDL_Renderer* surface;
extern TTF_Font* Sans;
extern time_t currenttime;
struct ScreenFrame {
    SDL_Surface*        surface;
    SDL_Texture*        texture;
    SDL_FRect           dims;
};
extern struct ScreenFrame DayMap;
extern struct ScreenFrame NightMap;
extern struct ScreenFrame CountriesMap;

struct surfaces {
    struct ScreenFrame  map;
    struct ScreenFrame  callsign;
    struct ScreenFrame  de;
    struct ScreenFrame  dx;
    struct ScreenFrame  clock;
    struct ScreenFrame  rowbox1;
    struct ScreenFrame  rowbox2;
    struct ScreenFrame  rowbox3;
    struct ScreenFrame  rowbox4;
    struct ScreenFrame  ticker;

} extern winboxes;

struct map_pin {
    double lat;
    double lon;
    SDL_Texture* icon;
    SDL_Color color;
    char label[16];
    struct map_pin *next;
}  extern *map_pins;



#endif
