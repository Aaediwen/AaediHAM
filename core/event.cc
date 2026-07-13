#include "aaediclock.h"
#include "core/event.h"
#include "core/init.h"
#include "core.h"
#include "quit.h"
#include "panels.h"
#include "sdl_callbacks.h"

void config_reload() {
    std::cout << "RELOAD: Reloading config! Stand by\n";
    // kill main system timer
    debug_log << "RELOAD: Killing System Timers.\n\n";
    SDL_LockMutex(mutexes[MUTEX_MASTER_CLOCK]);
    if (flag_timer) {
        SDL_RemoveTimer(flag_timer);
        flag_timer = 0;
    }
    // unload plugins
    for (struct PluginModule& plugin : loaded_plugins ) {
        plugin.draw_flag = false;
        plugin.host_api->panel = nullptr;
        unregister_module(&plugin);
    }
    loaded_plugins.clear();
    overlays.clear();
    SDL_UnlockMutex(mutexes[MUTEX_MASTER_CLOCK]);
    // clear mutexes
    debug_log << "RELOAD: Cleaning Mutexes.\n\n";
    for (SDL_Mutex*& mtx : mutexes) {
        if (mtx) {
            SDL_LockMutex(mtx);
            SDL_UnlockMutex(mtx);
        }
    }

    // clear mouse event
    clock_mouse_event.mod_cords = {0.0, 0.0};
    clock_mouse_event.mod_count = 0;
    clock_mouse_event.mod_owner = MOD_NULL;

    winboxes[PANEL_CALLSIGN].plugin_index=0;
    winboxes[PANEL_NULL].plugin_index=0;
    winboxes[PANEL_DE].plugin_index=0;
    winboxes[PANEL_DX].plugin_index=0;
    winboxes[PANEL_CLOCK].plugin_index=0;
    winboxes[PANEL_FLEXBOX1].plugin_index=0;
    winboxes[PANEL_FLEXBOX2].plugin_index=0;
    winboxes[PANEL_FLEXBOX3].plugin_index=0;
    winboxes[PANEL_FLEXBOX4].plugin_index=0;
    winboxes[PANEL_FLEXBOX5].plugin_index=0;
    winboxes[PANEL_MAP].plugin_index=0;
    winboxes[PANEL_CALLSIGN].plugin_sequence.clear();
    winboxes[PANEL_NULL].plugin_sequence.clear();
    winboxes[PANEL_DE].plugin_sequence.clear();
    winboxes[PANEL_DX].plugin_sequence.clear();
    winboxes[PANEL_CLOCK].plugin_sequence.clear();
    winboxes[PANEL_FLEXBOX1].plugin_sequence.clear();
    winboxes[PANEL_FLEXBOX2].plugin_sequence.clear();
    winboxes[PANEL_FLEXBOX3].plugin_sequence.clear();
    winboxes[PANEL_FLEXBOX4].plugin_sequence.clear();
    winboxes[PANEL_FLEXBOX5].plugin_sequence.clear();
    winboxes[PANEL_MAP].plugin_sequence.clear();

    clockconfig.reload(configfile);
    AaediClock_Init::Plugin_Loader();
    for (struct PluginModule& plugin : loaded_plugins ) {
        plugin.draw_flag = false;
        plugin.host_api->panel = nullptr;
    }
    panel_assignment(false);
    AaediClock_Init::Init_System_Timer();
    std::cout << "RELOAD: config reloaded\n";
    return;
}

void interrupt_handler(int signal) {
    if ((signal == SIGINT) || (signal == SIGTERM)) {
        interrupt_flag = true;
//    	window_destroy();
#ifndef _WIN32
    } else if ((signal == SIGHUP)) {
        reload_flag = true;
#endif
    }
//    window_destroy();
    return;
}

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
            case SDLK_C:
                if  (event->key.mod & (SDL_KMOD_LALT | SDL_KMOD_RALT)) {
                    config_reload();
                }
                break;
            case SDLK_Q:
                window_destroy();
                return SDL_APP_FAILURE;
                break;
//            case SDLK_R:
//                resize_flag = !resize_flag;
//                break;
            case SDLK_F4:
                if  (event->key.mod & (SDL_KMOD_LALT | SDL_KMOD_RALT)) {
                    window_destroy();
                    return SDL_APP_FAILURE;
                }
                break;
            default:
                break;
        }

    }
    return SDL_APP_CONTINUE;
}
