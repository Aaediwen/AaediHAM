#ifndef CLASSES_H
#define CLASSES_H
#include <mutex>
enum mod_name {
    MOD_MAP             ,
    MOD_DE              ,
    MOD_DX              ,
    MOD_CLOCK           ,
    MOD_CALL            ,
    MOD_POTA            ,
    MOD_PSK             ,
    MOD_SAT             ,
    MOD_DXSPOT          ,
    MOD_KINDEX          ,
    MOD_NCDXF           ,
    MOD_SOLAR           ,
    MOD_WSPR		,
    MOD_LUNAR		,
    MOD_CONTESTS	,
    MOD_RSS		,
    MOD_AURORA		,
    MOD_NULL
};

enum panel_names : int {
    PANEL_CALLSIGN      ,
    PANEL_CLOCK         ,
    PANEL_MAP           ,
    PANEL_DE            ,
    PANEL_DX            ,
    PANEL_FLEXBOX1      ,
    PANEL_FLEXBOX2      ,
    PANEL_FLEXBOX3      ,
    PANEL_FLEXBOX4      ,
    PANEL_FLEXBOX5      ,
    PANEL_NULL
};
#define MAGIC_SCREENFRAME  0x5343524E4652414E

struct nullbuffer : std::streambuf {
    int overflow(int c) override { return c; }
};


class ScreenFrame {
    private:
        const uint64_t	    magic = MAGIC_SCREENFRAME;
        SDL_Renderer*       renderer;
        void panel_dims_check();
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
        bool Create (SDL_Renderer* parent, const SDL_FRect size);
        SDL_Renderer* GetRenderer();
        void SetRenderer(SDL_Renderer* source);
        void Reset();
        void draw_border();
        void Clear(const SDL_Color& color = {0, 0, 0, SDL_ALPHA_OPAQUE});
        void render_text(const SDL_FRect& text_box, TTF_Font *font, const SDL_Color& color, const std::string& str);
        void render_text(const SDL_FRect& text_box, TTF_Font *font, const SDL_Color& color, const char* str);
        void present();
        bool valid() const;
};
//extern ScreenFrame DayMap;
//extern ScreenFrame NightMap;
//extern ScreenFrame CountriesMap;

struct GeoCoord {
    double latitude;
    double longitude;
};


class config {
public:
    struct ip_server_t {
        std::string name;
        uint16_t port;
    };
    struct plugin {
        std::string filename;
        size_t  panel_id;
        uint16_t interval;
    };
    config();
    ~config();
    const std::string& CallSign() const;
    const std::string& PSKCall() const;
    const std::string& DXmsg() const;
    const GeoCoord& DE() const;
    const GeoCoord& DX() const;
    void set_DX(const GeoCoord& target, const std::string msg);
    const ip_server_t& dxserver() const;
    const std::vector<std::string>& Sats() const;
    const std::vector<std::string>& Rss() const;
    const std::string& qrz_key(bool refresh = false);
    void set_qrz_pass(const std::string& newpass);
    bool next_wspr(std::string *callsign, int *band);
    plugin next_plugin();
    private:

        struct WSPRTarget {
            std::string callsign;
            int band;
        };
        std::vector<struct WSPRTarget> m_WSPRList;
        Uint16 m_WSPRIndex;
        std::vector<struct plugin> m_Plugins;
        Uint16 m_PluginIndex;
        std::string m_CallSign;
        std::string m_PSKCall;
        std::vector<std::string> m_sats;
        std::vector<std::string> m_rss;
        struct GeoCoord m_DE;
        struct GeoCoord m_DX;
        std::string m_DXMsg;
        ip_server_t m_dxserver;
        mutable std::mutex dx_set_mutex;
        struct {
            std::string Secret;
            std::string Key;
        } m_QRZ;

        void qrz_sesskey();
        void write_config();
        void load_config();
};
extern config clockconfig;


class map_overlay {
    private:
        struct transparancy {
            ScreenFrame panel;
            uint16_t owner;
            uint8_t z_order = 1;
        };
        std::vector<struct transparancy> overlay_list;
        Uint8 zorder;
        Uint16 index;
    public:
        map_overlay();
        ~map_overlay();
        ScreenFrame* get_overlay(SDL_Renderer* renderer, uint16_t owner, SDL_FRect dims, uint8_t z_layer); // return existing if present, or create a new and retu
        bool overlay_check(uint16_t owner);        // check if a overlay exists
        void set_zorder(Uint8 priority);
        void remove_overlay(uint16_t owner); // remove any overlay owned by owner
        ScreenFrame* next_overlay(uint8_t z_layer);   // somehow get or use a read-only itterator through overlay_list
        void reset_index();
        void clear(); // nuke all overlays
};
extern map_overlay overlays;

class map_icons {
    private:
        struct icon_entry {
            SDL_Texture* icon = nullptr;
            uint16_t owner = 0;
        };
        std::vector<struct icon_entry> icon_list;
    public:
        map_icons();
        ~map_icons();
        map_icons(ScreenFrame&& source) = delete;
        map_icons& operator=(map_icons&& source) = delete;
        map_icons(const map_icons& source) = delete;
        map_icons& operator=(const map_icons& source) = delete;

        bool icon_check (uint16_t index, uint16_t owner);
        uint16_t icon_create(uint16_t owner, SDL_Surface* icon_image);
        bool icon_update(uint16_t owner, uint16_t index, SDL_Surface* icon_image);
        void icon_delete(uint16_t owner, uint16_t index);
        void clear_icons();
        SDL_Texture* get_icon(uint16_t index);
};

/*
class map_icons {

    public:
        enum icon_names : unsigned int {
            ICON_SAT,
            ICON_SUN,
            ICON_MOON
        };
        map_icons(SDL_Renderer* renderer = nullptr);
        ~map_icons();
        map_icons(ScreenFrame&& source) = delete;
        map_icons& operator=(map_icons&& source) = delete;
        map_icons(const map_icons& source) = delete;
        map_icons& operator=(const map_icons& source) = delete;
        void reload_icons(SDL_Renderer* renderer = nullptr);
        SDL_Texture* get_icon(enum icon_names);
        void set_dynamic(SDL_Renderer* renderer, SDL_Surface* source, enum icon_names id);
    private:
        std::array<SDL_Texture*,3> icons{};
        void load_texture (SDL_Renderer* renderer, const std::string& path, const enum icon_names index);
        void clear_icons();
};
*/
extern map_icons icon_bin;
#endif