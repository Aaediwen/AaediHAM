#include "de_dx.h"
#include "../aaediclock.h"
#include "../utils.h"

void draw_de_dx(ScreenFrame& panel, TTF_Font* font, double lat, double lon, int de_dx) {

//    if (!panel.renderer) {
//        SDL_Log("Missing Renderer!");
//        return ;
//    }
//    if (!panel.texture) {
//        SDL_Log("Missing PANEL!");
//        return ;
//    }
//    SDL_Log("Drawing DE_DX");
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
//    SDL_Log("Set Font");

    // blank the box
    panel.Clear();

    time_t sunrise;
    time_t sunset;
    double solar_alt;
    // find the next zero crossing for sunrise if current alt <0

    sun_times(lat, lon, &sunrise, &sunset, &solar_alt, currenttime);
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


    // clean up
    TTF_SetFontSize(font,oldsize);

//    SDL_Log("Done drawing DE/DX");
}
