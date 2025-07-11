#pragma once
#ifndef AAEDICLOCK
#define AAEDICLOCK
//#define SDL_MAIN_USE_CALLBACKS
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fstream>
//#include <unistd.h>
//#include <math.h>

//#include <SDL3/SDL_main.h>
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <vector>

extern SDL_Window* window;
extern SDL_Renderer* surface;
extern TTF_Font* Sans;
extern time_t currenttime;

class ScreenFrame {
    public:
        SDL_Renderer*	    renderer;
        SDL_Surface*        surface;
        SDL_Texture*        texture;
        SDL_FRect           dims;
        ScreenFrame();
        ~ScreenFrame();
        bool Create (SDL_Renderer* parent, SDL_FRect size);
        void Reset();
        void draw_border();
        void Clear(const SDL_Color& color = {0, 0, 0, 255});
        void render_text(const SDL_FRect& text_box, TTF_Font *font, const SDL_Color& color, const char* str);

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

struct GeoCoord {
    double latitude;
    double longitude;
};

class config {
    private:
        std::string m_CallSign;
        std::vector<std::string> m_sats;
        struct GeoCoord m_DE;
        struct GeoCoord m_DX;
        struct {
            std::string Secret;
            std::string Key;
        } m_QRZ;

        void qrz_sesskey();
        void write_config();
        void load_config();

    public:
        config();
        ~config();
        const std::string& CallSign() const;
        const GeoCoord& DE() const;
        const GeoCoord& DX() const;
        const std::vector<std::string>& Sats() const;
        const std::string& qrz_key(bool refresh=false);
        void set_qrz_pass(const std::string& newpass);

};



extern config clockconfig;


enum mod_name {
        MOD_MAP		,
        MOD_DE		,
        MOD_DX		,
        MOD_CLOCK       ,
        MOD_CALL	,
        MOD_POTA	,
        MOD_PSK		,
        MOD_SAT		,
        MOD_DXSPOT
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
