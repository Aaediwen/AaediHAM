#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include "aaediclock.h"
#include "utils.h"

void draw_panel_border(struct ScreenFrame panel) {
    SDL_SetRenderDrawColor(surface, 128, 128, 128, 255);
    SDL_FRect border;
    border.x=0;
    border.y=0;
    border.w=panel.dims.w;
    border.h=panel.dims.h;
    SDL_SetRenderTarget(surface, panel.texture);
    SDL_RenderRect(surface, &(border));
    SDL_SetRenderTarget(surface, NULL);
    SDL_RenderTexture(surface, panel.texture, NULL, &(panel.dims));
    return;
}

double solar_altitude(double lat_deg, double lon_deg, struct tm *utc, double decl_deg) {
    //Converts latitude and solar declination from degrees to radians
    double lat = lat_deg * M_PI / 180.0;
    double decl = decl_deg * M_PI / 180.0;

    double utc_hours = utc->tm_hour + utc->tm_min / 60.0 + utc->tm_sec / 3600.0;
    double solar_time = utc_hours + (lon_deg / 15.0);  // Local solar time for pixel
    double hour_angle = (15.0 * (solar_time - 12.0)) * M_PI / 180.0;
    double sin_alt = sin(lat) * sin(decl) + cos(lat) * cos(decl) * cos(hour_angle);
    return asin(sin_alt) * 180.0 / M_PI;
}
