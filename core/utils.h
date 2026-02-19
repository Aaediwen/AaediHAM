#ifndef UTILS_H
#define UTILS_H
#ifdef _WIN32
using dx_socket_t = uintptr_t;
#else
using dx_socket_t = int;
#endif

#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <string>
/* -- migrated -- */
struct Celestial_Coordinates {
        time_t timestamp= 0;
        double RA       = 0.0;
        double Dec      = 0.0;
        double Lon      = 0.0;
        double Lat      = 0.0;
        double Dist     = 0.0;
};
struct celest_coords {
    struct Celestial_Coordinates moon;
    struct Celestial_Coordinates sun;
};
extern struct celest_coords g_celestials;

double solar_altitude(double lat_deg, double lon_deg, struct tm *utc, double decl_deg);
void maidenhead(double lat, double lon, char* maiden);
struct GeoCoord loc_to_geo (const std::string locator);
struct GeoCoord subsolar(const time_t now);
void sun_times(double lat, double lon, time_t* sunrise, time_t* sunset, double *solar_alt, time_t now);
int add_pin(struct map_pin* new_pin);
int delete_owner_pins(enum mod_name owner);
int delete_mod_cache(enum mod_name owner);
void dump_cache();
int add_data_cache(enum mod_name owner, const Uint64 size, const void* data);
int fetch_data_cache(enum mod_name owner, time_t *age, Uint64 *size, void* data);
std::string htmldecode(const std::string source);
Uint64 http_loader(const char* source_url, void** result);
Uint64 cache_loader(const enum mod_name owner, void** result, time_t *result_time);
std::string url_encode(const std::string& input);
SDL_Texture* SDLCLOCK_CreateTexture(SDL_Renderer* renderer, SDL_PixelFormat format, SDL_TextureAccess access, int w, int h, const char* owner, const char* where);
void SDLCLOCK_DestroyTexture(SDL_Texture* t, const char* where);
void mutex_checker();
//void draw_panel_border(ScreenFrame panel);
int read_socket(dx_socket_t fd, std::string &result);

void cords_to_px(double lat, double lon, int w, int h, SDL_FPoint* result);
int month_to_int(const std::string& month);


/* -- to migrate -- */


#endif
