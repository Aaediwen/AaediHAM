#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include "aaediclock.h"
#include "utils.h"
#include "utils/http_fetch.h"


struct data_blob *data_cache = 0;		// main data cache
struct map_pin   *map_pins = 0;			// active map pins
std::vector<struct map_pin>plugin_map_pins;
/*
void cords_to_px(double lat, double lon, int w, int h, SDL_FPoint* result) {
    if (!result) return;
    if (lon < -180.0) {
        lon = -180.0;
    }
    if (lon > 180.0) {
        lon = 180.0;
    }
    if (lat < -90.0) {
        lat = -90.0;
    }
    if (lat > 90.0) {
        lat = 90.0;
    }
    result->x=static_cast<float>((lon/180.0f)*(w/2.0f)+(w/2.0f));
    result->y= static_cast<float>(((-1*lat)/90.0f)*(h/2.0f)+(h/2.0f));
    return ;
}

*/