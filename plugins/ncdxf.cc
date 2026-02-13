#include "ncdxf.h"
#include <ctime>
#include <iostream>

const struct beacon beacons[18] = {
    {"4U1UN",  "New York, USA"},         // UN HQ, often offline, replaced by WB4MBF
    {"VE8AT",  "Inuvik, Canada"},
    {"W6WX",   "California, USA"},
    {"KH6RS",  "Hawaii, USA"},
    {"ZL6B",   "Masterton, New Zealand"},
    {"VK6RBP", "Perth, Australia"},
    {"JA2IGY", "Tokyo, Japan"},
    {"RR9O",   "Novosibirsk, Russia"},
    {"VR2B",   "Hong Kong"},
    {"4S7B",   "Colombo, Sri Lanka"},
    {"ZS6DN",  "Pretoria, South Africa"},
    {"5Z4B",   "Nairobi, Kenya"},
    {"4X6TU",  "Tel Aviv, Israel"},
    {"OH2B",   "Espoo, Finland"},
    {"CS3B",   "Madeira Island, Portugal"},
    {"LU4AA",  "Buenos Aires, Argentina"},
    {"OA4B",   "Lima, Peru"},
    {"YV5B",   "Caracas, Venezuela"}
};

aaediclock_host_api* host_api = nullptr;
const double beacon_freqs[5] = {14.100, 18.110, 21.150, 24.930, 28.200};


extern "C" DllExport aaediclock_plugin_api* createPlugin() {
    return new ncdxf_plugin();
}
extern "C" DllExport void destroyPlugin(aaediclock_plugin_api* target) {
    if (target) {
        delete target;
    }
}


void ncdxf_plugin::plugin_init() const {
    return;
}

void ncdxf_plugin::plugin_exit() const {
    return;
}

void ncdxf_plugin::plugin_main(const aaediclock_FRect& dims) const {
    std::cout << "In Plugin ncdxf main\n";
    time_t time_now=time(NULL);
    aaediclock_FRect TextBox;
    if (!host_api) {
        return;
    }
//    std::cout << "Plugin calling host clear\n";
    host_api->AaediHAM_GraphicsClear();

    // Header
    TextBox.x = dims.w/20;
    TextBox.y = dims.h/20;
    TextBox.h = dims.h/15;
    TextBox.w = dims.w - dims.w/10;
    host_api->AaediHAM_GraphicsDrawText("NCDXF BEACONS", {128,0,64,0}, TextBox);
    char tempstr[64];
    TextBox.y += dims.h/10;
    TextBox.w = (dims.w/3) - (dims.w/20);
    int cycle_sec = (time_now) % 180; // 3 minutes = 180 seconds
    int beacon_index = cycle_sec / 10; // which 10-second slot


    for (int i = 0; i < 5; ++i) {
        // Render the current beacon for each frequency slot
        int beacon_offset = (beacon_index - (i)) % 18;
        if (beacon_offset <0) {
            beacon_offset +=18;
        }
        // frequency
        sprintf(tempstr, "%4.3f", beacon_freqs[i]);
        *(host_api->AaediHAM_LogDebug) <<"NCDXF: " << tempstr << "\n";
        host_api->AaediHAM_GraphicsDrawText(tempstr, {128,128,64,0}, TextBox);
        // station callsign
        TextBox.x = (dims.w/3)*2;
        sprintf(tempstr, "%s", beacons[beacon_offset].call);
        host_api->AaediHAM_GraphicsDrawText(tempstr, {128,128,64,0}, TextBox);
        // station location
        TextBox.y += dims.h/15.0f;
        TextBox.x = dims.w/20.0f;
        TextBox.w = (dims.w * .75f) - dims.w/20.0f;
        sprintf(tempstr, "%s", beacons[beacon_offset].location.c_str());
        host_api->AaediHAM_GraphicsDrawText(tempstr, {64,64,32,0}, TextBox);
        // next line!
        TextBox.y += dims.h/10;
        TextBox.x = dims.w/20;
        TextBox.w = (dims.w/3) - (dims.w/20);
    }
    return;
}

const char* ncdxf_plugin::getName() const {
    return "NCDXF Beacon Module";
}

void ncdxf_plugin::set_host(aaediclock_host_api* host) {
    host_api = host;
}
