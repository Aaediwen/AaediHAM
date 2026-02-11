#include "aaediclock.h"
#include "core/core.h"
#include "modules/pskreporter.h"
#include "sdl_callbacks.h"

int window_destroy() {
    debug_log << "EXIT: Exiting Normally.\n\n";
    SDL_Quit();
    return 0;
}

    /* This function runs once at shutdown. */
void SDL_AppQuit(void *appstate, SDL_AppResult result) {
    (void)appstate;
    (void)result;
    debug_log << "EXIT: Killing System Timers.\n\n";
    if (flag_timer) {
        SDL_RemoveTimer(flag_timer);
        flag_timer = 0;
    }
    if (map_timer) {
        SDL_RemoveTimer(map_timer);
        map_timer = 0;
    }
    free (night_mask_args);
    night_mask_args=nullptr;
    debug_log << "EXIT: Disabling Draw flags and panels.\n\n";
    for (struct PluginModule& plugin : loaded_plugins ) {
        plugin.draw_flag = false;
//        plugin.host_api->panel = nullptr;
        unregister_module(&plugin);
    }
    master_flags.callsign.draw_flag     =       false;
    master_flags.de.draw_flag           =       false;
    master_flags.dx.draw_flag           =       false;
    master_flags.pota.draw_flag         =       false;
    master_flags.sat_tracker.draw_flag  =       false;
    master_flags.dx_spots.draw_flag     =       false;
    master_flags.map.draw_flag          =       false;
    master_flags.ncdxf.draw_flag        =       false;
    master_flags.kindex.draw_flag       =       false;
    master_flags.clock.draw_flag        =       false;
    master_flags.solar.draw_flag        =       false;
    master_flags.wspr.draw_flag         =       false;
    master_flags.lunar.draw_flag        =       false;
    master_flags.psk.draw_flag          =       false;
    master_flags.contests.draw_flag     =       false;
    master_flags.rss.draw_flag          =       false;
    master_flags.map.panel              =       nullptr;
    master_flags.sat_tracker.panel      =       nullptr;
    master_flags.dx_spots.panel         =       nullptr;
    master_flags.callsign.panel         =       nullptr;
    master_flags.de.panel               =       nullptr;
    master_flags.dx.panel               =       nullptr;
    master_flags.pota.panel             =       nullptr;
    master_flags.ncdxf.panel            =       nullptr;
    master_flags.clock.panel            =       nullptr;
    master_flags.kindex.panel           =       nullptr;
    master_flags.solar.panel            =       nullptr;
    master_flags.wspr.panel             =       nullptr;
    master_flags.lunar.panel            =       nullptr;
    master_flags.psk.panel              =       nullptr;
    master_flags.contests.panel         =       nullptr;
    master_flags.rss.panel              =       nullptr;
    master_flags.aurora.panel           =       nullptr;
    debug_log << "EXIT: Cleaning Mutexes.\n\n";
   for (SDL_Mutex*& mtx : mutexes) {
        if (mtx) {
            SDL_LockMutex(mtx);
            SDL_UnlockMutex(mtx);
            SDL_DestroyMutex(mtx);
            mtx = nullptr;
        }
    }
    debug_log << "EXIT: Cleaning SDL Panels.\n\n";
    overlays.clear();
    DayMap.Reset();
    NightMap.Reset();
    CountriesMap.Reset();
    winboxes[PANEL_CALLSIGN].panel.Reset();
    winboxes[PANEL_NULL].panel.Reset();
    winboxes[PANEL_DE].panel.Reset();
    winboxes[PANEL_DX].panel.Reset();
    winboxes[PANEL_CLOCK].panel.Reset();
    winboxes[PANEL_FLEXBOX1].panel.Reset();
    winboxes[PANEL_FLEXBOX2].panel.Reset();
    winboxes[PANEL_FLEXBOX3].panel.Reset();
    winboxes[PANEL_FLEXBOX4].panel.Reset();
    winboxes[PANEL_FLEXBOX5].panel.Reset();
    winboxes[PANEL_MAP].panel.Reset();
    debug_log << "EXIT: PSKreporter Cleanup.\n\n";
    psk_cleanup();
    debug_log << "EXIT: Destroying Window.\n\n";
    debug_log.flush();

    /* SDL will clean up the window/renderer for us. */
}
