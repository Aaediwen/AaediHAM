#include "clock.h"
#include <ctime>
aaediclock_host_api* host_api = nullptr;
extern "C" DllExport aaediclock_plugin_api* createPlugin() {
    return new clock_plugin();
}
extern "C" DllExport void destroyPlugin(aaediclock_plugin_api* target) {
    if (target) {
        delete target;
    }
}

void clock_plugin::plugin_init() const {
    return;
}

void clock_plugin::plugin_exit() const {
    return;
}

void clock_plugin::plugin_main(const aaediclock_FRect& dims) const {
/*
    need some way to replicate these sanity checks
    if (SDL_TryLockMutex(mutexes[MUTEX_RESIZE])) {
        SDL_UnlockMutex(mutexes[MUTEX_RESIZE]);
    }
    else {
        SDL_Log("Clock Module during resize event!");
        return (0);
    }
    if (!font) {
        debug_log << "CLOCK: No font defined\n";
        return 1;
    }
    if (!panel.GetRenderer()) {
        debug_log << "CLOCK: Missing Renderer!\n";
        return 1;
    }
    if (!panel.texture) {
        debug_log << "CLOCK: Missing PANEL!\n";
        return 1;
    }

*/

    char timestr[64];
    // blank the box
    host_api->AaediHAM_GraphicsClear();
    aaediclock_Color fontcolor;
    fontcolor.r=128;
    fontcolor.g=128;
    fontcolor.b=255;
    fontcolor.a=0;
    aaediclock_FRect TextRect;
    // need to be able to process a mouse event
    /*
        if (clock_mouse_event.mod_owner == MOD_CLOCK) {
        SDL_Log ("Click event in Clock module at %f, %f", clock_mouse_event.mod_cords.x, clock_mouse_event.mod_cords.y);
        clock_mouse_event.mod_owner = MOD_NULL;
    }
    */

     // generate the time strings
     // utc
    TextRect.x=2;
    TextRect.y=2;
    TextRect.h=(dims.h/5)*2;
    TextRect.w=((dims.w/5)*2)-4;
    time_t currenttime = time(NULL);
    *(host_api->AaediHAM_LogDebug) << "\t\t\t\t\tCLOCK: "<< currenttime;
    struct tm* clocktime = gmtime(&currenttime);
    strftime(timestr, sizeof(timestr), "%Y-%m-%d", clocktime);
    host_api->AaediHAM_GraphicsDrawText(timestr, fontcolor, TextRect);
    TextRect.x=((dims.w/5)*3)-4;;
#ifndef _WIN32
    strftime(timestr, sizeof(timestr), "%H:%M:%S %Z", clocktime);
#else
    strftime(timestr, sizeof(timestr), "%H:%M:%S UTC", clocktime);
#endif
    host_api->AaediHAM_GraphicsDrawText(timestr, fontcolor, TextRect);
    *(host_api->AaediHAM_LogDebug) << timestr << "\n";
     // local
    TextRect.x=2;
    TextRect.y=(dims.h/5)*2;
    clocktime = localtime(&currenttime);
    strftime(timestr, sizeof(timestr), "%Y-%m-%d", clocktime);
    host_api->AaediHAM_GraphicsDrawText(timestr, fontcolor, TextRect);
    TextRect.x=((dims.w/5)*3)-4;;
#ifndef _WIN32

    strftime(timestr, sizeof(timestr), "%H:%M:%S %Z", clocktime);


#else
    std::string wintime;
    strftime(timestr, sizeof(timestr), "%H:%M:%S", clocktime);
    wintime = timestr;
    wintime += " ";
    std::string zonestring = (_daylight ? _tzname[1] : _tzname[0]);
    wintime.push_back(zonestring[0]);
    size_t pos = zonestring.find(' ');
    while (pos != std::string::npos) {
        if (pos + 1 < zonestring.size() && std::isalpha(zonestring[pos + 1])) {
            wintime.push_back(zonestring[pos + 1]);
        }
        pos = zonestring.find(' ', pos + 1);
    }
    sprintf(timestr, "%s", wintime.c_str());
#endif
    host_api->AaediHAM_GraphicsDrawText(timestr, fontcolor, TextRect);

}

const char* clock_plugin::getName() const {
    return "Clock Module";
}

void clock_plugin::set_host(aaediclock_host_api* host) {
    host_api = host;
}


