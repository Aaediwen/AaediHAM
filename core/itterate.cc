#include "aaediclock.h"
#include "core/core.h"
#include "modules/modules.h"
#include <SDL3_image/SDL_image.h>
#include "sdl_callbacks.h"

bool resizing = false;
bool reload_flag = false;
SDL_AppResult SDL_AppIterate(void *appstate) {
    (void)appstate;
    SDL_Delay(10);                      // slow down the program
#ifdef _WIN32
#ifdef _DEBUG
    _ASSERTE(_CrtCheckMemory());
#endif
#endif
//mutex_checker();
    if (SDL_TryLockMutex(mutexes[MUTEX_RESIZE])) {
        SDL_UnlockMutex(mutexes[MUTEX_RESIZE]);
    }
    else {
        SDL_Log("Itterate during resize event!");
        return (SDL_APP_CONTINUE);
    }
    if (!resizing) {
        const Uint64 StartTicks = SDL_GetTicks();	// timer for how long this has taken
        // resize stress test on R
        if (reload_flag) {
            SDL_WindowFlags winstate = SDL_GetWindowFlags(window);
            if (winstate & SDL_WINDOW_FULLSCREEN) {
                SDL_SetWindowFullscreen(window, 0);
            }
            else {
                SDL_SetWindowFullscreen(window, 1);
            }
            SDL_SyncWindow(window);
        }
        SDL_LockMutex(mutexes[MUTEX_MASTER_CLOCK]);
        draw_overlays(*(master_flags.map.panel));
        winboxes[PANEL_MAP].panel.draw_border();
        if (master_flags.clock.draw_flag) {
            debug_log << "ITTERATE: Calling Clock ("<< MOD_CLOCK <<") with panel " << loaded_plugins[2].host_api->panel << "\n";
//            draw_clock(winboxes[PANEL_CLOCK].panel, Sans);
            aaediclock_FRect module_dims;

            module_dims.w = loaded_plugins[2].host_api->panel->dims.w;
            module_dims.h = loaded_plugins[2].host_api->panel->dims.h;
            module_dims.x = loaded_plugins[2].host_api->panel->dims.x;
            module_dims.y = loaded_plugins[2].host_api->panel->dims.y;
            loaded_plugins[2].plugin->plugin_main(module_dims);
            SDL_SetRenderTarget(clock_renderer, NULL);
            master_flags.clock.draw_flag = false;
            debug_log << "ITTERATE: Module Timer Clock -- " << (SDL_GetTicks() - StartTicks) << " MIlliseconds\n";
            debug_log.flush();
        }
        if (master_flags.rss.draw_flag) {
            debug_log << "ITTERATE: Calling Rss ("<< MOD_RSS <<") with panel " << master_flags.map.panel << "\n";
            rss_ticker(*(master_flags.map.panel));
            master_flags.rss.draw_flag = false;
            debug_log << "ITTERATE: Module Timer RSS -- " << (SDL_GetTicks() - StartTicks) << " MIlliseconds\n";
            debug_log.flush();
        }
        if (master_flags.callsign.draw_flag) {
            debug_log << "ITTERATE: Calling Callsign ("<< MOD_CALL <<")with panel " <<  loaded_plugins[1].host_api->panel << "\n";
//            draw_callsign(*(master_flags.callsign.panel), Sans, clockconfig.CallSign().c_str());
            aaediclock_FRect module_dims;

            module_dims.w = loaded_plugins[1].host_api->panel->dims.w;
            module_dims.h = loaded_plugins[1].host_api->panel->dims.h;
            module_dims.x = loaded_plugins[1].host_api->panel->dims.x;
            module_dims.y = loaded_plugins[1].host_api->panel->dims.y;
            loaded_plugins[1].plugin->plugin_main(module_dims);
            SDL_SetRenderTarget(clock_renderer, NULL);
            master_flags.callsign.draw_flag = false;
            debug_log << "ITTERATE: Module Timer Callsign -- " << (SDL_GetTicks() - StartTicks) << " MIlliseconds\n";
            debug_log.flush();
        }
        if (master_flags.map.draw_flag) {
            debug_log << "ITTERATE: Calling Map ("<< MOD_MAP <<")with panel " << master_flags.map.panel << "\n";
            draw_map(*(master_flags.map.panel));
            winboxes[PANEL_MAP].panel.draw_border();
            master_flags.map.draw_flag = false;
            debug_log << "ITTERATE: Module Timer Map -- " << (SDL_GetTicks() - StartTicks) << " MIlliseconds\n";
            debug_log.flush();
        }
        if (master_flags.de.draw_flag) {
        //4

            debug_log << "ITTERATE: Calling DE ("<< MOD_DE <<")with panel " << loaded_plugins[4].host_api->panel << "\n";
//            draw_de_dx(*(master_flags.de.panel), Sans, clockconfig.DE().latitude, clockconfig.DE().longitude, 1);
            aaediclock_FRect module_dims;

            module_dims.w = loaded_plugins[4].host_api->panel->dims.w;
            module_dims.h = loaded_plugins[4].host_api->panel->dims.h;
            module_dims.x = loaded_plugins[4].host_api->panel->dims.x;
            module_dims.y = loaded_plugins[4].host_api->panel->dims.y;
            loaded_plugins[4].plugin->plugin_main(module_dims);
            SDL_SetRenderTarget(clock_renderer, NULL);
            master_flags.de.draw_flag = false;
            debug_log << "ITTERATE: Module Timer DE -- " << (SDL_GetTicks() - StartTicks) << " MIlliseconds\n";
            debug_log.flush();
        }
        if (master_flags.dx.draw_flag) {
        // 5
            debug_log << "ITTERATE: Calling DX ("<< MOD_DX <<")with panel " << loaded_plugins[5].host_api->panel << "\n";
//            draw_de_dx(*(master_flags.dx.panel), Sans, clockconfig.DX().latitude, clockconfig.DX().longitude, 0);
            aaediclock_FRect module_dims;

            module_dims.w = loaded_plugins[5].host_api->panel->dims.w;
            module_dims.h = loaded_plugins[5].host_api->panel->dims.h;
            module_dims.x = loaded_plugins[5].host_api->panel->dims.x;
            module_dims.y = loaded_plugins[5].host_api->panel->dims.y;
            loaded_plugins[5].plugin->plugin_main(module_dims);
            SDL_SetRenderTarget(clock_renderer, NULL);
            master_flags.dx.draw_flag = false;
            debug_log << "ITTERATE: Module Timer DX -- " << (SDL_GetTicks() - StartTicks) << " MIlliseconds\n";
            debug_log.flush();
        }
        if (master_flags.aurora.draw_flag) {
            debug_log << "ITTERATE: Calling AURORA ("<< MOD_AURORA <<")with panel " << master_flags.map.panel << "\n";
            aaediclock_FRect module_dims;
            loaded_plugins[8].plugin->plugin_main(module_dims);
            SDL_SetRenderTarget(clock_renderer, NULL);
//            aurora_spots(*(master_flags.map.panel));
            master_flags.aurora.draw_flag = false;
            debug_log << "ITTERATE: Module Timer AURORA -- " << (SDL_GetTicks() - StartTicks) << " MIlliseconds\n";
            debug_log.flush();
        }
        if (master_flags.pota.draw_flag) {
            debug_log << "ITTERATE: Calling POTA ("<< MOD_POTA <<")with panel " << loaded_plugins[3].host_api->panel << "\n";
//            pota_spots(*(master_flags.pota.panel), Sans);
            aaediclock_FRect module_dims;

            module_dims.w = loaded_plugins[3].host_api->panel->dims.w;
            module_dims.h = loaded_plugins[3].host_api->panel->dims.h;
            module_dims.x = loaded_plugins[3].host_api->panel->dims.x;
            module_dims.y = loaded_plugins[3].host_api->panel->dims.y;
//            SDL_Log ("Attempting to correct plugin panel settings");
//            loaded_plugins[1].host_api.panel = &(winboxes[loaded_plugins[1].position].panel);

            //loaded_plugins[1].host_api.panel = master_flags.callsign.panel;
//            SDL_Log ("Attempting to call plugin main");
//            loaded_plugins[1].plugin->set_host(&(loaded_plugins[1].host_api));
            loaded_plugins[3].plugin->plugin_main(module_dims);
            SDL_SetRenderTarget(clock_renderer, NULL);
//            aurora_spots(*(master_flags.map.panel));
            master_flags.pota.draw_flag = false;
            debug_log << "ITTERATE: Module Timer POTA -- " << (SDL_GetTicks() - StartTicks) << " MIlliseconds\n";
            debug_log.flush();
        }
        if (master_flags.lunar.draw_flag) {
            debug_log << "ITTERATE: Calling Lunar ("<< MOD_LUNAR <<")with panel " << loaded_plugins[12].host_api->panel << "\n";
            debug_log.flush();
//            lunar_module(*(master_flags.lunar.panel));
            aaediclock_FRect module_dims;
            module_dims.w = loaded_plugins[12].host_api->panel->dims.w;
            module_dims.h = loaded_plugins[12].host_api->panel->dims.h;
            module_dims.x = loaded_plugins[12].host_api->panel->dims.x;
            module_dims.y = loaded_plugins[12].host_api->panel->dims.y;
            loaded_plugins[12].plugin->plugin_main(module_dims);
            SDL_SetRenderTarget(clock_renderer, NULL);
            debug_log << "ITTERATE: Module Timer LUNAR -- " << (SDL_GetTicks() - StartTicks) << " MIlliseconds\n";
            master_flags.lunar.draw_flag = false;

        }
        if (master_flags.kindex.draw_flag) {
            debug_log << "ITTERATE: Calling Kindex ("<< MOD_KINDEX <<")with panel " << loaded_plugins[11].host_api->panel << "\n";
            debug_log.flush();
//            k_index_chart (*(master_flags.kindex.panel));
            aaediclock_FRect module_dims;
            module_dims.w = loaded_plugins[11].host_api->panel->dims.w;
            module_dims.h = loaded_plugins[11].host_api->panel->dims.h;
            module_dims.x = loaded_plugins[11].host_api->panel->dims.x;
            module_dims.y = loaded_plugins[11].host_api->panel->dims.y;
            loaded_plugins[11].plugin->plugin_main(module_dims);
            SDL_SetRenderTarget(clock_renderer, NULL);
            debug_log << "ITTERATE: Module Timer Kindex -- " << (SDL_GetTicks() - StartTicks) << " MIlliseconds\n";
            master_flags.kindex.draw_flag = false;
        }
        if (master_flags.contests.draw_flag) {
            debug_log << "ITTERATE: Calling Contests ("<< MOD_CONTESTS <<")with panel " << loaded_plugins[10].host_api->panel << "\n";
            debug_log.flush();
//            contest_module (*(master_flags.contests.panel));
            aaediclock_FRect module_dims;
            module_dims.w = loaded_plugins[10].host_api->panel->dims.w;
            module_dims.h = loaded_plugins[10].host_api->panel->dims.h;
            module_dims.x = loaded_plugins[10].host_api->panel->dims.x;
            module_dims.y = loaded_plugins[10].host_api->panel->dims.y;
            loaded_plugins[10].plugin->plugin_main(module_dims);
            SDL_SetRenderTarget(clock_renderer, NULL);
            master_flags.contests.draw_flag = false;
            debug_log << "ITTERATE: Module Timer Contests -- " << (SDL_GetTicks() - StartTicks) << " MIlliseconds\n";
        }
        if (master_flags.sat_tracker.draw_flag) {
            debug_log << "ITTERATE: Calling Sat Tracker ("<< MOD_SAT <<")with panel " << loaded_plugins[7].host_api->panel << "\n";
            debug_log.flush();
//            sat_tracker (*(master_flags.sat_tracker.panel), Sans, winboxes[PANEL_MAP].panel);
            aaediclock_FRect module_dims;
            module_dims.w = loaded_plugins[7].host_api->panel->dims.w;
            module_dims.h = loaded_plugins[7].host_api->panel->dims.h;
            module_dims.x = loaded_plugins[7].host_api->panel->dims.x;
            module_dims.y = loaded_plugins[7].host_api->panel->dims.y;
            loaded_plugins[7].plugin->plugin_main(module_dims);
            SDL_SetRenderTarget(clock_renderer, NULL);
            master_flags.sat_tracker.draw_flag = false;
            debug_log << "ITTERATE: Module Timer Sat Tracker -- " << (SDL_GetTicks() - StartTicks) << " MIlliseconds\n";
        }
        if (master_flags.dx_spots.draw_flag) {
            debug_log << "ITTERATE: Calling DX Spots ("<< MOD_DXSPOT <<")with panel " << master_flags.dx_spots.panel << "\n";
            debug_log.flush();
  //          dx_cluster(*(master_flags.dx_spots.panel));
            aaediclock_FRect module_dims;
            module_dims.w = loaded_plugins[6].host_api->panel->dims.w;
            module_dims.h = loaded_plugins[6].host_api->panel->dims.h;
            module_dims.x = loaded_plugins[6].host_api->panel->dims.x;
            module_dims.y = loaded_plugins[6].host_api->panel->dims.y;
            loaded_plugins[6].plugin->plugin_main(module_dims);
            SDL_SetRenderTarget(clock_renderer, NULL);
            debug_log << "ITTERATE: Module Timer DX Spots -- " << (SDL_GetTicks() - StartTicks) << " MIlliseconds\n";
            master_flags.dx_spots.draw_flag = false;
        }
        if (master_flags.ncdxf.draw_flag) {
            debug_log << "ITTERATE: Calling NCDXF ("<< MOD_NCDXF <<")with panel " <<  loaded_plugins[0].host_api->panel << "\n";
            debug_log.flush();
            aaediclock_FRect module_dims;
            module_dims.w = loaded_plugins[0].host_api->panel->dims.w;
            module_dims.h = loaded_plugins[0].host_api->panel->dims.h;
            module_dims.x = loaded_plugins[0].host_api->panel->dims.x;
            module_dims.y = loaded_plugins[0].host_api->panel->dims.y;
            SDL_Log ("Attempting to correct plugin panel settings");
            SDL_Log ("Attempting to call plugin main");
            loaded_plugins[0].plugin->plugin_main(module_dims);
            SDL_SetRenderTarget(clock_renderer, NULL);
            debug_log << "ITTERATE: Module Timer NCDXF -- " << (SDL_GetTicks() - StartTicks) << " MIlliseconds\n";
            master_flags.ncdxf.draw_flag = false;
        }
        if (master_flags.solar.draw_flag) {
            debug_log << "ITTERATE: Calling SDO ("<< MOD_SOLAR <<")with panel " << loaded_plugins[9].host_api->panel << "\n";
            debug_log.flush();
//            sdo_image(*(master_flags.solar.panel));
            aaediclock_FRect module_dims;
            module_dims.w = loaded_plugins[9].host_api->panel->dims.w;
            module_dims.h = loaded_plugins[9].host_api->panel->dims.h;
            module_dims.x = loaded_plugins[9].host_api->panel->dims.x;
            module_dims.y = loaded_plugins[9].host_api->panel->dims.y;
            loaded_plugins[9].plugin->plugin_main(module_dims);
            SDL_SetRenderTarget(clock_renderer, NULL);
            debug_log << "ITTERATE: Module Timer SDO -- " << (SDL_GetTicks() - StartTicks) << " MIlliseconds\n";
            master_flags.solar.draw_flag = false;
        }
        if (master_flags.wspr.draw_flag) {
            debug_log << "ITTERATE: Calling WSPR Tracker ("<< MOD_WSPR <<")with panel " << loaded_plugins[13].host_api->panel << "\n";
            debug_log.flush();
//            wspr_tracker (*(master_flags.wspr.panel), winboxes[PANEL_MAP].panel);
            aaediclock_FRect module_dims;
            module_dims.w = loaded_plugins[13].host_api->panel->dims.w;
            module_dims.h = loaded_plugins[13].host_api->panel->dims.h;
            module_dims.x = loaded_plugins[13].host_api->panel->dims.x;
            module_dims.y = loaded_plugins[13].host_api->panel->dims.y;
            loaded_plugins[13].plugin->plugin_main(module_dims);
            SDL_SetRenderTarget(clock_renderer, NULL);
            master_flags.wspr.draw_flag = false;
            debug_log << "ITTERATE: Module Timer WSPR -- " << (SDL_GetTicks() - StartTicks) << " MIlliseconds\n";
        }
        if (master_flags.psk.draw_flag) {
            debug_log << "ITTERATE: Calling PSK Reporter ("<< MOD_PSK <<")with panel " << master_flags.psk.panel << "\n";
            debug_log.flush();
            psk_reporter(*(master_flags.psk.panel));
            master_flags.psk.draw_flag = false;
            debug_log << "ITTERATE: Module Timer PSK Reporter -- " << (SDL_GetTicks() - StartTicks) << " MIlliseconds\n";
        }
        winboxes[PANEL_CALLSIGN].panel.present();
        winboxes[PANEL_CLOCK].panel.present();
        winboxes[PANEL_MAP].panel.present();
        winboxes[PANEL_DE].panel.present();
        winboxes[PANEL_DX].panel.present();
        winboxes[PANEL_FLEXBOX1].panel.present();
        winboxes[PANEL_FLEXBOX2].panel.present();
        winboxes[PANEL_FLEXBOX3].panel.present();
        winboxes[PANEL_FLEXBOX4].panel.present();
        winboxes[PANEL_FLEXBOX5].panel.present();
        SDL_UnlockMutex(mutexes[MUTEX_MASTER_CLOCK]);
        SDL_RenderPresent(clock_renderer);
//        if (headless && (!outfile.empty())) {
        if (!outfile.empty()) {
            // dump surface to image file here
            int width, height;
            SDL_GetCurrentRenderOutputSize(clock_renderer, &width, &height);
            SDL_Surface* savesurface = SDL_RenderReadPixels(clock_renderer, NULL);
            IMG_SaveJPG(savesurface, outfile.c_str(), 75);
//            SDL_SaveBMP(savesurface, outfile.c_str());  // output_file_path from --output
            SDL_DestroySurface(savesurface);
        }
        debug_log << "ITTERATE: Took " << (SDL_GetTicks() - StartTicks) << " MIlliseconds\n";
    }
    return(SDL_APP_CONTINUE);
}

