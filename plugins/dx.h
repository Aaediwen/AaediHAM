//void draw_de_dx(ScreenFrame& panel, TTF_Font* font, double lat, double lon, int de_dx);#include "aaediclock.h"

#ifndef DX_H
#define DX_H
#include "aaediclock.h"
#include "plugin_api.h"


class DllExport dx_plugin : public aaediclock_plugin_api {
        void plugin_init() const override;
        void plugin_main(const aaediclock_FRect& dims) const override;
        const char* getName() const override;
        void plugin_exit() const override;
        void set_host(aaediclock_host_api* host);
};

#endif