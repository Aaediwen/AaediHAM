#ifndef LUNAR_H
#define LUNAR_H
#include <ctime>
#include "aaediclock.h"
#include "utils/celestials.h"
#include "plugin_api.h"
double moon_phase_angle(time_t& t);
//void lunar_module(ScreenFrame& panel, time_t timestamp = 0);

class DllExport lunar_plugin : public aaediclock_plugin_api {
        void plugin_init() const override;
        void plugin_main(const aaediclock_FRect& dims) const override;
        const char* getName() const override;
        void plugin_exit() const override;
        void set_host(aaediclock_host_api* host);
};

#endif