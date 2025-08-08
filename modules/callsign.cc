#include "callsign.h"
#include "../aaediclock.h"

void draw_callsign(ScreenFrame& panel, TTF_Font* font, const char* callsign) {
//    if (!panel.renderer) {
//        SDL_Log("Missing Renderer!");
//        return ;
//    }
//    if (!panel.texture) {
//        SDL_Log("Missing PANEL!");
//        return ;
//    }
    panel.Clear();
//    SDL_Log("Rendering Callsign");
    SDL_Color fontcolor;
    fontcolor.r=128;
    fontcolor.g=128;
    fontcolor.b=255;
    fontcolor.a=0;
    if (!font) {
        printf("No font defined\n");
        return;
    }

    SDL_FRect TextRect;
    TextRect.x=2;
    TextRect.y=2;
    TextRect.h=(panel.dims.h)-4;
    TextRect.w=(panel.dims.w)-4;
    panel.render_text(TextRect, font, fontcolor, callsign);
}
