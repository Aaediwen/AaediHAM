#include "callsign.h"
#include "../aaediclock.h"

void draw_callsign(ScreenFrame& panel, TTF_Font* font, const char* callsign) {
    if (SDL_TryLockMutex(mutexes[MUTEX_RESIZE])) {
        SDL_UnlockMutex(mutexes[MUTEX_RESIZE]);
    }
    else {
        SDL_Log("Callsign Render call during resize event!");
        return;
    }
    if (!panel.GetRenderer()) {
        debug_log << "CALLSIGN: Missing Renderer!\n";
        return ;
    }
    if (!panel.texture) {
        debug_log << "CALLSIGN: Missing PANEL!\n";
        return ;
    }
    if (mouse_event.mod_owner == MOD_CALL) {
        SDL_Log ("Click event in Callsign module at %f, %f", mouse_event.mod_cords.x, mouse_event.mod_cords.y);
        mouse_event.mod_owner = MOD_NULL;
    }

    panel.Clear();
//    SDL_Log("Rendering Callsign");
    SDL_Color fontcolor;
    fontcolor.r=128;
    fontcolor.g=128;
    fontcolor.b=255;
    fontcolor.a=0;
    if (!font) {
        debug_log << "CALLSIGN: No font defined\n";
        return;
    }

    SDL_FRect TextRect;
    TextRect.x=2;
    TextRect.y=2;
    TextRect.h=(panel.dims.h)-4;
    TextRect.w=(panel.dims.w)-4;
    panel.render_text(TextRect, font, fontcolor, callsign);
}
