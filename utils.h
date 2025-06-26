#ifndef UTILS_H
#define UTILS_H
#include <math.h>

void draw_panel_border(struct ScreenFrame panel);
double solar_altitude(double lat_deg, double lon_deg, struct tm *utc, double decl_deg);
void maidenhead(double lat, double lon, char* maiden);
void sun_times(double lat, double lon, time_t* sunrise, time_t* sunset, double *solar_alt, time_t now);
void render_text(SDL_Renderer *renderer, SDL_Texture *target, SDL_FRect *text_box, TTF_Font *font, SDL_Color color, const char* str);
int add_pin(struct map_pin* new_pin);
int delete_owner_pins(enum mod_name owner);
int delete_mod_cache(enum mod_name owner);
int add_data_cache(enum mod_name owner, const Uint32 size, void* data);
int fetch_data_cache(enum mod_name owner, time_t *age, Uint32 *size, void* data);


#endif
