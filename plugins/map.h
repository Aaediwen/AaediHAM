#ifndef MAP_H
#define MAP_H
#include "aaediclock.h"
#include "plugin_api.h"
//int draw_map(ScreenFrame& panel);
void draw_overlays(ScreenFrame& panel);

struct map_layer {
    uint16_t texture = 0;
    SDL_Surface* surface = nullptr;
};

class DllExport map_plugin : public aaediclock_plugin_api {
    void plugin_init() const override;
    void plugin_main(const aaediclock_FRect& dims) const override;
    const char* getName() const override;
    void plugin_exit() const override;
    void set_host(aaediclock_host_api* host);
};

#endif