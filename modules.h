#ifndef MODULES_H

#define MODULES_H


void draw_de_dx(struct ScreenFrame panel, TTF_Font* font, double lat, double lon, int de_dx);
void draw_callsign(struct ScreenFrame panel, TTF_Font* font, const char* callsign);
void load_maps();
int draw_map(struct ScreenFrame panel);
int draw_clock(struct ScreenFrame panel, TTF_Font* font);
void pota_spots(struct ScreenFrame panel, TTF_Font* font);

#endif

