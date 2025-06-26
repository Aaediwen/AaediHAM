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

enum mod_name {
        MOD_MAP		,
        MOD_DE		,
        MOD_DX		,
        MOD_CLOCK       ,
        MOD_CALL	,
        MOD_POTA	,
        MOD_PSK
};

struct map_pin {
    enum mod_name owner;
    double lat;
    double lon;
    SDL_Texture* icon;
    SDL_Color color;
    char label[16];
    char tooltip[512];
    struct map_pin *next;
}  extern *map_pins;

struct data_blob {
    enum mod_name owner;
    time_t fetch_time;
    Uint32 size;
    void *data;
    struct data_blob *next;
} extern *data_cache;
#endif
