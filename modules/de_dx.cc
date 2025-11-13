#include "de_dx.h"
#include "../aaediclock.h"
#include "../utils.h"

void draw_de_dx(ScreenFrame& panel, TTF_Font* font, double lat, double lon, int de_dx) {
    if (SDL_TryLockMutex(mutexes[MUTEX_RESIZE])) {
        SDL_UnlockMutex(mutexes[MUTEX_RESIZE]);
    }
    else {
        SDL_Log("DE/DX Drawduring resize event!");
        return ;
    }
    if (!panel.GetRenderer()) {
        debug_log << "DE/DX: Missing Renderer!\n";
        return ;
    }
    if (!panel.texture) {
        debug_log << "DE/DX: Missing PANEL!\n";
        return ;
    }
    char tempstr[64];
    SDL_FRect TextRect;
    SDL_Color fontcolor;
    if (de_dx) {
        fontcolor.r=128;
        fontcolor.g=255;
        fontcolor.b=128;
        fontcolor.a=0;
    } else {
        fontcolor.r=255;
        fontcolor.g=128;
        fontcolor.b=128;
        fontcolor.a=0;
    }
    if (!font) {
        printf("No font defined\n");
        return;
    }
    float oldsize = TTF_GetFontSize(font);
    TTF_SetFontSize(font,72);

    // blank the box
    panel.Clear();

    time_t sunrise;
    time_t sunset;
    double solar_alt;
    // find the next zero crossing for sunrise if current alt <0

    sun_times(lat, lon, &sunrise, &sunset, &solar_alt, time(NULL));
    // render the header
    TextRect.x=2;
    TextRect.y=2;
    TextRect.h=(panel.dims.h)/4;
    TextRect.w=(panel.dims.w)-4;
    struct map_pin de_dx_pin;
    if (de_dx) {
        panel.render_text(TextRect, font, fontcolor, "DE:");
        de_dx_pin.owner=MOD_DE;
        sprintf(de_dx_pin.label, "DE");


    } else {
        panel.render_text(TextRect, font, fontcolor, "DX:");
        de_dx_pin.owner=MOD_DX;
        sprintf(de_dx_pin.label, "DX");
    }
    if (mouse_event.mod_owner == de_dx_pin.owner) {
        SDL_Log ("Click event in DE/DX module at %f, %f", mouse_event.mod_cords.x, mouse_event.mod_cords.y);
        mouse_event.mod_owner = MOD_NULL;
    }

    de_dx_pin.lat=lat;
    de_dx_pin.lon=lon;
    de_dx_pin.icon=0;
    de_dx_pin.color=fontcolor;
    de_dx_pin.color.a=255;
    de_dx_pin.tooltip[0]=0;
    delete_owner_pins(de_dx_pin.owner);
    add_pin(&de_dx_pin);

    // generate maidenhead grid square

    char latstr[2];
    char lonstr[2];
    char maiden[7];
    maidenhead(lat, lon, maiden);
    // lat/lon string generation
    if (lat < 0) {
        lat *= -1;
        latstr[0]='S';
        latstr[1]=0;
    } else {
        latstr[0]='N';
        latstr[1]=0;
    }

    if (lon < 0) {
        lon *= -1;
        lonstr[0]='W';
        lonstr[1]=0;
    } else {
        lonstr[0]='E';
        lonstr[1]=0;
    }
    sprintf(tempstr, "%2.2f%s %2.2f%s", lat, latstr, lon, lonstr);
    // render lat/lon
    TextRect.x=2;
    TextRect.y=(panel.dims.h)/4;
    TextRect.h=(panel.dims.h)/4;
    TextRect.w=panel.dims.w-4;
    panel.render_text(TextRect, font, fontcolor, tempstr);


    // render maidenhead
    TextRect.x=2;
    TextRect.y=(panel.dims.h)/2;
    TextRect.h=(panel.dims.h)/4;
    TextRect.w=panel.dims.w-4;
    sprintf(tempstr, "%s", maiden);
    panel.render_text(TextRect, font, fontcolor, tempstr);

    // render sunrise time
    TextRect.x=2;
    TextRect.y=((panel.dims.h)/4)*3;
    TextRect.h=(panel.dims.h)/8;
    TextRect.w=(panel.dims.w/3)-4;
    tm* test_time = localtime(&sunrise);
    strftime(tempstr, 12, "R%H:%M", test_time);
    panel.render_text(TextRect, font, fontcolor, tempstr);

    // render solar angle
    TextRect.x=(panel.dims.w/3)+8;
    TextRect.y=((panel.dims.h)/4)*3;
    TextRect.h=(panel.dims.h)/10;
    TextRect.w=(panel.dims.w/3)-16;
    test_time = localtime(&sunset);
    sprintf (tempstr, "%2.2f", solar_alt);
    panel.render_text(TextRect, font, fontcolor, tempstr);

    // render sunset time
    TextRect.x=(panel.dims.w/3)*2;
    TextRect.y=((panel.dims.h)/4)*3;
    TextRect.h=(panel.dims.h)/8;
    TextRect.w=(panel.dims.w/3)-4;
    test_time = localtime(&sunset);
    strftime(tempstr, 12, "S%H:%M", test_time);
    panel.render_text(TextRect, font, fontcolor, tempstr);
    if (!de_dx) {
        TextRect.x=(panel.dims.w/20);
        TextRect.y=((panel.dims.h)/8)*7;
        TextRect.h=(panel.dims.h)/8;
        TextRect.w=(panel.dims.w/10)*8;
        panel.render_text(TextRect, font, fontcolor, clockconfig.DXmsg().c_str());
    }
    // clean up
    TTF_SetFontSize(font,oldsize);
}
