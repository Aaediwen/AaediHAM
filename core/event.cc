#include "aaediclock.h"
#include "core.h"
#include "quit.h"
#include "panels.h"
#include "sdl_callbacks.h"
    // SDL Event Handler
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
 (void)appstate;
    if (event->type==SDL_EVENT_QUIT) {
        window_destroy();
        return SDL_APP_FAILURE;
    }
    if ((event->type==SDL_EVENT_WINDOW_RESIZED)||(event->type==SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)) {
        if (!resizing) {
            resizing=1;
            debug_log << "EVENT: Resize event triggered!\n";
            resize_panels(winboxes);
            debug_log << "EVENT: Resize event complete!\n";
            debug_log.flush();
            SDL_Event pending;
            while (SDL_PeepEvents(&pending, 1, SDL_GETEVENT, SDL_EVENT_WINDOW_FIRST, SDL_EVENT_WINDOW_LAST) > 0) {
                debug_log << "EVENT: Purge event buffer\n";
                // discard spurious window events (resize, expose, etc.)
            }
            resizing=0;
        }
    }
    if (event->type==SDL_EVENT_MOUSE_BUTTON_UP) {
        if (event->button.button ==1 && event->button.clicks ==1) {
//            SDL_Log ("Got single left click at %f, %f", event->button.x, event->button.y);
            debug_log << "EVENT: Got single left click at " << event->button.x << ", " << event->button.y << "\n";
            for (auto& pager : winboxes) {
                if ((event->button.x > pager.panel.dims.x) && (event->button.x < (pager.panel.dims.x+pager.panel.dims.w)) &&
                    (event->button.y > pager.panel.dims.y) && (event->button.y < (pager.panel.dims.y+pager.panel.dims.h))) {
                        float modx, mody;
                        modx = event->button.x - pager.panel.dims.x;
                        mody = event->button.y - pager.panel.dims.y;
//                        SDL_Log ("Panel event coords: %f, %f", modx, mody);
                        debug_log << "EVENT: Panel event coords: " << modx << ", " << mody << "\n";
                        pager.clickpoint={modx, mody};
                        pager.clickcount = event->button.clicks;
                        if (pager.plugin_sequence.size()) {
                            clock_mouse_event.mod_cords = {modx, mody};
                            clock_mouse_event.mod_count = event->button.clicks;
                            if (pager.plugin_sequence.size()) {
                                clock_mouse_event.plugin_owner = pager.plugin_sequence[pager.plugin_index];
                                debug_log << "EVENT: Panel event owner: " << clock_mouse_event.plugin_owner << "("<< loaded_plugins[clock_mouse_event.plugin_owner].name << ")\n";
                            }
                        }
                    }
            }
        }
    }
    if (event->type==SDL_EVENT_KEY_UP) {
        SDL_WindowFlags winstate = SDL_GetWindowFlags(window);
        switch (event->key.key) {
            case SDLK_RETURN:
                if  (event->key.mod & (SDL_KMOD_LALT | SDL_KMOD_RALT)) {
                    if (winstate & SDL_WINDOW_FULLSCREEN) {
                        SDL_SetWindowFullscreen(window, 0);
                    } else {
                        SDL_SetWindowFullscreen(window, 1);
                    }
                    SDL_SyncWindow(window);
                }
                break;
            case SDLK_F11:
                if (winstate & SDL_WINDOW_FULLSCREEN) {
                    SDL_SetWindowFullscreen(window, 0);
                } else {
                    SDL_SetWindowFullscreen(window, 1);
                }
                SDL_SyncWindow(window);
                break;
//            case SDLK_C:
//                if  (event->key.mod & (SDL_KMOD_LALT | SDL_KMOD_RALT)) {
//                    dump_cache();
//                }
//                break;
            case SDLK_Q:
                window_destroy();
                return SDL_APP_FAILURE;
                break;
            case SDLK_R:
                reload_flag = !reload_flag;
                break;
            case SDLK_F4:
                if  (event->key.mod & (SDL_KMOD_LALT | SDL_KMOD_RALT)) {
                    window_destroy();
                    return SDL_APP_FAILURE;
                }
            default:
                break;
        }

    }
    return SDL_APP_CONTINUE;
}
