#pragma once
#ifndef AAEDICLOCK
#define AAEDICLOCK
//#define SDL_MAIN_USE_CALLBACKS
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fstream>
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <vector>
#include <array>
#include "classes.h"
#ifdef _WIN32
#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif
#endif

struct regen_mask_args {
    SDL_Surface* source;
    SDL_Surface* dest;
    SDL_FRect panel_dims;
};

extern struct regen_mask_args* night_mask_args;

enum mutex_name {
    MUTEX_NIGHT_MASK	,
    MUTEX_RESIZE	,
    MUTEX_CACHE		,
    MUTEX_HTTP		,
    MUTEX_MASTER_CLOCK	,
    MUTEX_CELESTRAK	,
    MUTEX_WSPR
};
extern std::array<SDL_Mutex*, 10> mutexes;
//extern SDL_Mutex* night_mask_mutex;
//extern SDL_Mutex* resize_mutex;
//extern SDL_Mutex* cache_mutex;
//extern SDL_Mutex* http_mutex;
extern SDL_TimerID map_timer;
extern TTF_Font* Sans;
extern std::fstream debug_log;

extern ScreenFrame DayMap;
extern ScreenFrame NightMap;
extern ScreenFrame CountriesMap;

struct pager_node {
    std::vector<enum mod_name> sequence;
    ScreenFrame panel;
    unsigned int index=0;
    SDL_FPoint clickpoint;
    int clickcount = 0;
};
extern std::array<pager_node, 12> winboxes;

struct internal_mouse_event {
    SDL_FPoint mod_cords;
    int mod_count;
    enum mod_name mod_owner;
};
extern struct internal_mouse_event clock_mouse_event;


struct Celestial_Coordinates {
        time_t timestamp= 0;
        double RA	= 0.0;
        double Dec	= 0.0;
        double Lon	= 0.0;
        double Lat	= 0.0;
        double Dist	= 0.0;
};
struct celest_coords {
    struct Celestial_Coordinates moon;
    struct Celestial_Coordinates sun;
};
extern struct celest_coords g_celestials;

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
    Uint64 size;
    void *data;
    struct data_blob *next;
} extern *data_cache;

#endif
