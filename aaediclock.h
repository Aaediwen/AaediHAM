
#pragma once
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#endif
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <iostream>
#include <string>
#include <vector>
#include <array>
#include "core/classes.h"
#include "plugins/host_api.h"
#ifdef _WIN32
#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif
#endif


struct PluginModule;
/*
struct regen_mask_args {
    SDL_Surface* source;
    SDL_Surface* dest;
    SDL_FRect panel_dims;
};

extern struct regen_mask_args* night_mask_args;
*/
extern SDL_Renderer		*clock_renderer;
enum mutex_name {
    MUTEX_NIGHT_MASK    ,
    MUTEX_RESIZE        ,
    MUTEX_CACHE         ,
    MUTEX_HTTP          ,
    MUTEX_MASTER_CLOCK  ,
    MUTEX_CELESTRAK     ,
    MUTEX_WSPR          ,
    MUTEX_CONTESTS      ,
    MUTEX_AURORA
};
extern std::array<SDL_Mutex*, 10> mutexes;
//extern SDL_TimerID map_timer;
extern TTF_Font* Sans;
extern std::ostream& debug_log;

extern Sint64 max_tex_size;
extern SDL_ThreadID main_thread_id;
extern bool interrupt_flag;

struct pager_node {
    std::vector<enum mod_name> sequence;
    std::vector<int> plugin_sequence;
    ScreenFrame panel;
    unsigned int index=0;
    unsigned int plugin_index = 0;
    SDL_FPoint clickpoint;
    int clickcount = 0;
};
extern std::array<pager_node, 12> winboxes;

struct internal_mouse_event {
    SDL_FPoint mod_cords;
    int mod_count;
    enum mod_name mod_owner;
    int plugin_owner;
};
extern struct internal_mouse_event clock_mouse_event;
struct map_pin {
    enum mod_name owner;
    int plugin_owner;
    double lat;
    double lon;
    SDL_Texture* icon;
    SDL_Color color;
    char label[16];
    char tooltip[512];
    struct map_pin *next;
}  extern *map_pins;
extern std::vector<struct map_pin>plugin_map_pins;
struct data_blob {
    enum mod_name owner;
    time_t fetch_time;
    Uint64 size;
    void *data;
    struct data_blob *next;
} extern *data_cache;
