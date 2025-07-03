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
#include <vector>
#include "json.hpp"
using json = nlohmann::json;

extern SDL_Window* window;
extern SDL_Renderer* surface;
extern TTF_Font* Sans;
extern time_t currenttime;
class ScreenFrame {
    public:
        SDL_Surface*        surface;
        SDL_Texture*        texture;
        SDL_FRect           dims;
        ScreenFrame() {
            this->dims={0,0,0,0};
            this->texture = nullptr;
            this->surface = nullptr;
        }

        ~ScreenFrame() {
            Reset();
        }

        bool Create (SDL_Renderer* parent, SDL_FRect size) {
            dims=size;
            int h = static_cast<int>(size.h);
            int w = static_cast<int>(size.w);
            if (texture) {
                SDL_DestroyTexture(texture);
            }
            texture = SDL_CreateTexture (parent, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET,
                                               w, h );
        SDL_Log("Creating panel texture: %.1fx%.1f -> %dx%d", size.w, size.h, w, h);
           if (!this->texture) {
               SDL_Log("Error Creating Texture!");
               return false;
           }
           SDL_SetRenderTarget(parent, texture);
           SDL_SetRenderDrawColor(parent, 0, 0, 0, 255);
           SDL_RenderClear(parent);
           SDL_SetRenderTarget(parent, nullptr);
           return true;
        }

        void Reset() {
            if (this->texture) {
                SDL_DestroyTexture(this->texture);
            }
            if (this->surface) {
                SDL_DestroySurface(this->surface);
            }
            this->texture=nullptr;
            this->surface=nullptr;
            return;
        }
        void draw_border(SDL_Renderer* surface) {
            if (texture && surface) {
                SDL_SetRenderDrawColor(surface, 128, 128, 128, 255);
                SDL_FRect border;
                border.x=0;
                border.y=0;
                border.w=dims.w;
                border.h=dims.h;
                SDL_SetRenderTarget(surface, texture);
                SDL_RenderRect(surface, &(border));
                SDL_SetRenderTarget(surface, NULL);
                SDL_RenderTexture(surface, texture, NULL, &(dims));
            }
            return;
        }

};
extern ScreenFrame DayMap;
extern ScreenFrame NightMap;
extern ScreenFrame CountriesMap;

struct surfaces {
    ScreenFrame  map;
    ScreenFrame  callsign;
    ScreenFrame  de;
    ScreenFrame  dx;
    ScreenFrame  clock;
    ScreenFrame  rowbox1;
    ScreenFrame  rowbox2;
    ScreenFrame  rowbox3;
    ScreenFrame  rowbox4;
    ScreenFrame  ticker;

} extern winboxes;

struct config {
    std::string CallSign;
    std::vector<std::string> sats;
    struct {
        double lat;
        double lon;
    } DE;
    struct {
        double lat;
        double lon;
    } DX;
} extern clockconfig;


enum mod_name {
        MOD_MAP		,
        MOD_DE		,
        MOD_DX		,
        MOD_CLOCK       ,
        MOD_CALL	,
        MOD_POTA	,
        MOD_PSK		,
        MOD_SAT
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
