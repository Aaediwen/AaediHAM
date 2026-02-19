#include "sample.h"


aaediclock_host_api* host_api = nullptr;
extern "C" DllExport aaediclock_plugin_api* createPlugin() {
    return new sample_plugin();
}
extern "C" DllExport void destroyPlugin(aaediclock_plugin_api* target) {
    if (target) {
        delete target;
    }
}

void sample_plugin::plugin_init() const {
    return;
}

void sample_plugin::plugin_exit() const {
    return;
}

void sample_plugin::plugin_main(const aaediclock_FRect& dims) const {
    host_api->AaediHAM_GraphicsClear();
    aaediclock_Color fontcolor;
    fontcolor.r=128;
    fontcolor.g=128;
    fontcolor.b=255;
    fontcolor.a=0;

    aaediclock_FRect TextRect;
    TextRect.x=2;
    TextRect.y=2;
    TextRect.h=(dims.h)-4;
    TextRect.w=(dims.w)-4;
    const char* callsign = host_api->AaediHAM_ConfigGetCall();
    host_api->AaediHAM_GraphicsDrawText(callsign, fontcolor, TextRect);
}

const char* sample_plugin::getName() const {
    return "Sample Module";
}

void sample_plugin::set_host(aaediclock_host_api* host) {
    host_api = host;
}

