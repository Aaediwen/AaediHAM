#include "clock.h"
#include "../aaediclock.h"

int draw_clock(ScreenFrame& panel, TTF_Font* font) {
    if (SDL_TryLockMutex(resize_mutex)) {
        SDL_UnlockMutex(resize_mutex);
    }
    else {
        SDL_Log("Clock Module during resize event!");
        return (0);
    }
    SDL_Color fontcolor;
    fontcolor.r=128;
    fontcolor.g=128;
    fontcolor.b=255;
    fontcolor.a=0;
    char timestr[64];
    SDL_FRect TextRect;

    float oldsize = TTF_GetFontSize(font);
    TTF_SetFontSize(font,72);
    if (!font) {
        debug_log << "CLOCK: No font defined\n";
        return 1;
    }

    // blank the box
    panel.Clear();

    // generate the time strings
     // utc
    TextRect.x=2;
    TextRect.y=2;
    TextRect.h=(panel.dims.h/5)*2;
    TextRect.w=((panel.dims.w/5)*2)-4;
    time_t currenttime = time(NULL);
    struct tm* clocktime = gmtime(&currenttime);
    strftime(timestr, sizeof(timestr), "%Y-%m-%d", clocktime);
    panel.render_text(TextRect, font, fontcolor, timestr);
    TextRect.x=((panel.dims.w/5)*3)-4;;

#ifndef _WIN32
    strftime(timestr, sizeof(timestr), "%H:%M:%S %Z", clocktime);
#else
    strftime(timestr, sizeof(timestr), "%H:%M:%S UTC", clocktime);
#endif
    panel.render_text(TextRect, font, fontcolor, timestr);


     // local
    TextRect.x=2;
    TextRect.y=(panel.dims.h/5)*2;
    clocktime = localtime(&currenttime);
    strftime(timestr, sizeof(timestr), "%Y-%m-%d", clocktime);
    panel.render_text(TextRect, font, fontcolor, timestr);
    TextRect.x=((panel.dims.w/5)*3)-4;;
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
    panel.render_text(TextRect, font, fontcolor, timestr);
    TTF_SetFontSize(font,oldsize);
    return 0;
}
