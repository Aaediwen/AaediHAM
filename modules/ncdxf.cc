#include "ncdxf.h"
#include "../aaediclock.h"


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


const double beacon_freqs[5] = {14.100, 18.110, 21.150, 24.930, 28.200};


void ncdxf_module(ScreenFrame& panel) {
    time_t time_now=time(NULL);

    panel.Clear();
    SDL_FRect TextBox;
    TextBox.x = panel.dims.w/20;
    TextBox.y = panel.dims.h/20;
    TextBox.h = panel.dims.h/15;
    TextBox.w = panel.dims.w - panel.dims.w/10;
    panel.render_text(TextBox, Sans, {128,0,64,0}, "NCDXF BEACONS");
    char tempstr[64];
    TextBox.y += panel.dims.h/10;
    TextBox.w = (panel.dims.w/3) - (panel.dims.w/20);
    int cycle_sec = (time_now) % 180; // 3 minutes = 180 seconds
    int beacon_index = cycle_sec / 10; // which 10-second slot
    for (int i = 0; i < 5; ++i) {
        int beacon_offset = (beacon_index - (i)) % 18;
        if (beacon_offset <0) {
            beacon_offset +=18;
        }
        sprintf(tempstr, "%4.3f", beacon_freqs[i]);
//        SDL_Log (tempstr);
        panel.render_text(TextBox, Sans, {128,128,64,0}, tempstr);
        TextBox.x = (panel.dims.w/3)*2;
        sprintf(tempstr, "%s", beacons[beacon_offset].call);
        panel.render_text(TextBox, Sans, {128,128,64,0}, tempstr);
        TextBox.y += panel.dims.h/15;
        TextBox.x = panel.dims.w/20;
        TextBox.w = (panel.dims.w * .75) - panel.dims.w/20;
        sprintf(tempstr, "%s", beacons[beacon_offset].location.c_str());
        panel.render_text(TextBox, Sans, {64,64,32,0}, tempstr);
        TextBox.y += panel.dims.h/10;
        TextBox.x = panel.dims.w/20;
        TextBox.w = (panel.dims.w/3) - (panel.dims.w/20);

    }
    return;
}