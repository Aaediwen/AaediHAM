#include "callsign.h"


aaediclock_host_api* host_api = nullptr;
extern "C" DllExport aaediclock_plugin_api* createPlugin() {
    return new callsign_plugin();
}
extern "C" DllExport void destroyPlugin(aaediclock_plugin_api* target) {
    if (target) {
        delete target;
    }
}


void callsign_plugin::plugin_init() const {
    return;
}

void callsign_plugin::plugin_exit() const {
    return;
}

void callsign_plugin::plugin_main(const aaediclock_FRect& dims) const {
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
//    panel.render_text(TextRect, font, fontcolor, callsign);
}

const char* callsign_plugin::getName() const {
    return "Callsign Module";
}

void callsign_plugin::set_host(aaediclock_host_api* host) {
    host_api = host;
}




/*


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
    if (!font) {
        debug_log << "CALLSIGN: No font defined\n";
        return;
    }
    if (clock_mouse_event.mod_owner == MOD_CALL) {
        SDL_Log ("Click event in Callsign module at %f, %f", clock_mouse_event.mod_cords.x, clock_mouse_event.mod_cords.y);
        clock_mouse_event.mod_owner = MOD_NULL;
    }

    panel.Clear();
//    SDL_Log("Rendering Callsign");
    SDL_Color fontcolor;
    fontcolor.r=128;
    fontcolor.g=128;
    fontcolor.b=255;
    fontcolor.a=0;

    SDL_FRect TextRect;
    TextRect.x=2;
    TextRect.y=2;
    TextRect.h=(panel.dims.h)-4;
    TextRect.w=(panel.dims.w)-4;
    panel.render_text(TextRect, font, fontcolor, callsign);
}
*/