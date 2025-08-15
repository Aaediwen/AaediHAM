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


struct regen_mask_args {
    SDL_Surface* source;
    SDL_Surface* dest;
    SDL_FRect panel_dims;
};

extern struct regen_mask_args* night_mask_args;
extern SDL_Mutex* night_mask_mutex;
extern SDL_TimerID map_timer;
extern TTF_Font* Sans;
extern time_t currenttime;

class ScreenFrame {
    private:
        SDL_Renderer*	    renderer;
    public:
        SDL_Surface*        surface;
        SDL_Texture*        texture;
        SDL_FRect           dims;
        ScreenFrame();
        ~ScreenFrame();
        ScreenFrame(ScreenFrame&& source) noexcept;
        ScreenFrame& operator=(ScreenFrame&& source) noexcept;
        ScreenFrame(const ScreenFrame& source);
        ScreenFrame& operator=(const ScreenFrame& source);
        bool Create (SDL_Renderer* parent, SDL_FRect size);
        SDL_Renderer* GetRenderer();
        void Reset();
        void draw_border();
        void Clear(const SDL_Color& color = {0, 0, 0, 255});
        void render_text(const SDL_FRect& text_box, TTF_Font *font, const SDL_Color& color, const char* str);
        void present();
};
extern ScreenFrame DayMap;
extern ScreenFrame NightMap;
extern ScreenFrame CountriesMap;

/*struct surfaces {
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
    ScreenFrame  nullframe;
    ScreenFrame  corner;

} extern winboxes;
*/
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
    MOD_DXSPOT	,
    MOD_KINDEX	,
    MOD_NCDXF	,
    MOD_SOLAR
};

enum panel_names {
    PANEL_CALLSIGN	,
    PANEL_CLOCK		,
    PANEL_MAP		,
    PANEL_DE		,
    PANEL_DX		,
    PANEL_FLEXBOX1	,
    PANEL_FLEXBOX2	,
    PANEL_FLEXBOX3	,
    PANEL_FLEXBOX4	,
    PANEL_FLEXBOX5	,
    PANEL_NULL
};

struct pager_node {
    std::vector<enum mod_name> sequence;
    ScreenFrame panel;
    int index=0;
};
extern std::array<pager_node, 12> winboxes;

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

class map_overlay {
    private:
        struct transparancy {
            ScreenFrame panel;
            enum mod_name owner;
        };
        std::vector<struct transparancy> overlay_list;
        int index;
    public:
        map_overlay();
        ~map_overlay();
        ScreenFrame* get_overlay(SDL_Renderer* renderer, enum mod_name owner, SDL_FRect dims); // return existing if present, or create a new and return that
        bool overlay_check(enum mod_name owner);	// check if a overlay exists
        void remove_overlay(enum mod_name owner); // remove any overlay owned by owner
        ScreenFrame* next_overlay();   // somehow get or use a read-only itterator through overlay_list
        void reset_index();
        void clear(); // nuke all overlays
};

extern map_overlay overlays;

class map_icons {

    public:
        enum icon_names {
            ICON_SAT
        };
        map_icons(SDL_Renderer* renderer = nullptr);
        ~map_icons();
        map_icons(ScreenFrame&& source) = delete;
        map_icons& operator=(map_icons&& source) = delete;
        map_icons(const map_icons& source) = delete;
        map_icons& operator=(const map_icons& source) = delete;
        void reload_icons(SDL_Renderer* renderer = nullptr);
        SDL_Texture* get_icon(enum icon_names);
    private:
        std::array<SDL_Texture*,1> icons{};
        void load_texture (SDL_Renderer* renderer, const std::string& path, const enum icon_names index);
        void clear_icons();
};

extern map_icons icon_bin;
#endif
