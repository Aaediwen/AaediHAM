#include "aaediclock.h"
#include "panels.h"
#include "core/core.h"
std::string render_engine;
Uint16 interrupt_counter = 0;


void panel_assignment(bool increment) {
    // function to assign panels to the modules
    // default them all to the NULL panel
        for (struct PluginModule& plugin : loaded_plugins ) {
            plugin.draw_flag = false;
            plugin.host_api.panel = &(winboxes[PANEL_NULL].panel);
        }
        master_flags.map.panel          =       &(winboxes[PANEL_MAP].panel);
        master_flags.sat_tracker.panel  =       &(winboxes[PANEL_NULL].panel);
        master_flags.dx_spots.panel     =       &(winboxes[PANEL_NULL].panel);
        master_flags.callsign.panel     =       &(winboxes[PANEL_NULL].panel);
        master_flags.de.panel           =       &(winboxes[PANEL_NULL].panel);
        master_flags.dx.panel           =       &(winboxes[PANEL_NULL].panel);
        master_flags.pota.panel         =       &(winboxes[PANEL_NULL].panel);
        master_flags.ncdxf.panel        =       &(winboxes[PANEL_NULL].panel);
        master_flags.clock.panel        =       &(winboxes[PANEL_NULL].panel);
        master_flags.kindex.panel       =       &(winboxes[PANEL_NULL].panel);
        master_flags.solar.panel        =       &(winboxes[PANEL_NULL].panel);
        master_flags.wspr.panel         =       &(winboxes[PANEL_NULL].panel);
        master_flags.lunar.panel        =       &(winboxes[PANEL_NULL].panel);
        master_flags.psk.panel          =       &(winboxes[PANEL_NULL].panel);
        master_flags.contests.panel     =       &(winboxes[PANEL_NULL].panel);
        master_flags.rss.panel          =       &(winboxes[PANEL_NULL].panel);
        master_flags.aurora.panel       =       &(winboxes[PANEL_NULL].panel);
        // step through each screen panel
        for (auto& panel : winboxes) {
            if (panel.sequence.size()) {
                // optionally increment the panel to the next module in its list
                if (increment) {
                    panel.index++;
                    if (panel.index >= panel.sequence.size()) { panel.index = 0 ; }

                    panel.plugin_index++;
                    if (panel.plugin_index >= panel.plugin_sequence.size()) { panel.plugin_index = 0 ; }
                }
                // assign the correct module to the panel
           switch (panel.sequence[panel.index]) {
                case MOD_MAP:
                    master_flags.map.panel = &panel.panel;
                    break;
                case MOD_DE:
//                    master_flags.de.panel = &panel.panel;
                    break;
                case MOD_DX:
//                    master_flags.dx.panel = &panel.panel;
                    break;
                case MOD_CLOCK:
//                    master_flags.clock.panel = &panel.panel;
                    break;
                case MOD_CALL:
//                    master_flags.callsign.panel = &panel.panel;
                    break;
                case MOD_POTA:
//                    master_flags.pota.panel = &panel.panel;
                    break;
                case MOD_PSK:
//                    master_flags.psk.panel = &panel.panel;
                    break;
                case MOD_SAT:
//                    master_flags.sat_tracker.panel = &panel.panel;
                    break;
                case MOD_DXSPOT:
//                    master_flags.dx_spots.panel = &panel.panel;
                    break;
                case MOD_KINDEX:
//                    master_flags.kindex.panel = &panel.panel;
                    break;
                case MOD_CONTESTS:
//                    master_flags.contests.panel = &panel.panel;
                    break;
                case MOD_NCDXF:
//                    master_flags.ncdxf.panel = &panel.panel;
                    break;
                case MOD_SOLAR:
//                    master_flags.solar.panel = &panel.panel;
                    break;
                case MOD_WSPR:
//                    master_flags.wspr.panel = &panel.panel;
                    break;
                case MOD_LUNAR:
//                    master_flags.lunar.panel = &panel.panel;
                    break;
                case MOD_RSS:
//                    master_flags.rss.panel = &panel.panel;
                    break;
                case MOD_AURORA:
//                    master_flags.aurora.panel = &panel.panel;
                    break;
                case MOD_NULL:
                    break;
            }
        }
    }
}

Uint32 SDLCALL master_clock (void *userdata, SDL_TimerID timerID, Uint32 interval) {
//    SDL_Log ("FLAG TIMER: In Master flag timer\n");
// master clock to trigger each module
    (void) userdata;
    interrupt_counter++;

    if (interrupt_counter > 4800) {
        interrupt_counter = 0;
    }
    if (timerID) {
        SDL_LockMutex(mutexes[MUTEX_MASTER_CLOCK]);

        if ((interrupt_counter % 600) == 0) {   // 60 seconds
            debug_log << "FLAG_TIMER: MOD PAGER FIRED!\n";
            debug_log.flush();
            panel_assignment(true);
        }


        if ((interrupt_counter % 300)==0) {     // 30 seconds
            master_flags.callsign.draw_flag = true;
        }

        if ((interrupt_counter % 170)==0) {     // 17 seconds
            master_flags.aurora.draw_flag = true;
        }


        if ((interrupt_counter % 50)==0) {      // 5 seconds
            master_flags.de.draw_flag = true;
            master_flags.dx.draw_flag = true;
            master_flags.pota.draw_flag = true;
            master_flags.ncdxf.draw_flag = true;
        }

        if ((interrupt_counter % 50)==10) {     // 5 seconds +1
            master_flags.dx_spots.draw_flag = true;
            master_flags.psk.draw_flag = true;
            master_flags.contests.draw_flag = true;
        }
        if ((interrupt_counter % 50)==20) {     // 5 seconds +2

        }
        if ((interrupt_counter % 50)==30) {     // 5 seconds +3
            master_flags.kindex.draw_flag = true;
            master_flags.wspr.draw_flag = true;
            master_flags.lunar.draw_flag = true;
        }
        if ((interrupt_counter % 50)==40) {     // 5 seconds    +4
            master_flags.solar.draw_flag = true;
        }
        if ((interrupt_counter % 20)==0) {      // 2 seconds

        }

        if ((interrupt_counter % 10)==0) {      // 1 second
            master_flags.map.draw_flag = true;
            master_flags.sat_tracker.draw_flag = true;
        }
        if ((interrupt_counter % 2)==0) {       // .2 seconds
            master_flags.clock.draw_flag = true;
        }
        master_flags.rss.draw_flag = true;
//        debug_log << "FLAG_TIMER: Master flag timer done.\n";
        SDL_UnlockMutex(mutexes[MUTEX_MASTER_CLOCK]);
        return (interval);
    } else {
        return 0;
    }
}

void resize_panels(std::array<pager_node, 12>& panels) {
        int win_x;
        int win_y;
#ifdef _WIN32
#ifdef _DEBUG
        _ASSERTE(_CrtCheckMemory());
#endif
#endif
        if (SDL_TryLockMutex(mutexes[MUTEX_RESIZE])) {
            if (flag_timer) {
                SDL_RemoveTimer(flag_timer);
                flag_timer = 0;
            }
            if (map_timer) {
                SDL_RemoveTimer(map_timer);
                map_timer = 0;
            }

            debug_log << "RESIZE: Beginning Window resize\n";
            debug_log.flush();
            // lock and disable the rest of the program
            debug_log << "RESIZE: Disabling renders\n";
            debug_log.flush();
            SDL_LockMutex(mutexes[MUTEX_NIGHT_MASK]);
            for (struct PluginModule& plugin : loaded_plugins ) {
                plugin.draw_flag = false;
                plugin.host_api.panel = nullptr;
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
            master_flags.aurora.draw_flag       =       false;

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

            debug_log << "RESIZE: Destroying old surfaces\n";
            debug_log.flush();
            // clean up the old surface
            if (clock_renderer) {

                debug_log << "RESIZE: Clearing Overlays\n";
                std::cout.flush();
                overlays.clear();
                debug_log << "RESIZE: Clearing Screen Panels\n";
                debug_log.flush();
//                debug_log << "RESIZE: Callsign: " << &(panels[PANEL_CALLSIGN].panel) << "\n";
//                debug_log.flush();
                panels[PANEL_CALLSIGN].panel.Reset();
//                debug_log << "RESIZE: Null: " << &(panels[PANEL_NULL].panel) << "\n";
//                debug_log.flush();
                panels[PANEL_NULL].panel.Reset();
//                debug_log << "RESIZE: DE: " << &(panels[PANEL_DE].panel) << "\n";
//                debug_log.flush();
                panels[PANEL_DE].panel.Reset();
//                debug_log << "RESIZE: DX: " << &(panels[PANEL_DX].panel) << "\n";
//                debug_log.flush();
                panels[PANEL_DX].panel.Reset();
//                debug_log << "RESIZE: Clock: " << &(panels[PANEL_CLOCK].panel) << "\n";
//                debug_log.flush();
               panels[PANEL_CLOCK].panel.Reset();
//                debug_log << "RESIZE: Flex1: " << &(panels[PANEL_FLEXBOX1].panel) << "\n";
//                debug_log.flush();
                panels[PANEL_FLEXBOX1].panel.Reset();
//                debug_log << "RESIZE: Flex2: " << &(panels[PANEL_FLEXBOX2].panel) << "\n";
//                debug_log.flush();
                panels[PANEL_FLEXBOX2].panel.Reset();
//                debug_log << "RESIZE: Flex3: " << &(panels[PANEL_FLEXBOX3].panel) << "\n";
//                debug_log.flush();
                panels[PANEL_FLEXBOX3].panel.Reset();
//                debug_log << "RESIZE: Flex4: " << &(panels[PANEL_FLEXBOX4].panel) << "\n";
//                debug_log.flush();
                panels[PANEL_FLEXBOX4].panel.Reset();
//                debug_log << "RESIZE: Flex5: " << &(panels[PANEL_FLEXBOX5].panel) << "\n";
//                debug_log.flush();
                panels[PANEL_FLEXBOX5].panel.Reset();
//                debug_log << "RESIZE: Map: " << &(panels[PANEL_MAP].panel) << "\n";
//                debug_log.flush();
                panels[PANEL_MAP].panel.Reset();
                debug_log.flush();

                SDL_SetRenderTarget(clock_renderer, nullptr);

            } else {
                // create a new renderer
                debug_log << "RESIZE: Creating new Renderer\n";
                debug_log.flush();
                if (render_engine.empty()) {
                    clock_renderer = SDL_CreateRenderer(window, NULL);
                } else {
                    clock_renderer = SDL_CreateRenderer(window, render_engine.c_str());
                }
                if (!clock_renderer) {
                    SDL_Log("Failed to create renderer: %s", SDL_GetError());
                    debug_log << "RESIZE: Failed to create renderer: " << SDL_GetError();
                    exit(1);
                } else {
                    debug_log << "RESIZE: created new renderer: " << (void*)clock_renderer << "\n";
                    debug_log << "RESIZE: Using Driver : " << SDL_GetRendererName(clock_renderer) << " on " << SDL_GetCurrentVideoDriver() << "\n";
                    std::cout << "RESIZE: Using Driver : " << SDL_GetRendererName(clock_renderer) << " on " << SDL_GetCurrentVideoDriver() << "\n";
                    // we need this to prevent exceeding the system's max texture size
                    SDL_PropertiesID RendererProperties = SDL_GetRendererProperties(clock_renderer);
                    max_tex_size = SDL_GetNumberProperty(RendererProperties,SDL_PROP_RENDERER_MAX_TEXTURE_SIZE_NUMBER, 0);
                    // debug trap
//                    max_tex_size = 2048;
                    debug_log << "RESIZE: Renderer Max Texture Size : " << max_tex_size << "\n";
                    std::cout << "RESIZE: Renderer Max Texture Size : " << max_tex_size << "\n";
                }
            }
            // clear the window at its new size
            SDL_GetCurrentRenderOutputSize(clock_renderer, &win_x, &win_y);
            printf("Resizing Window to %i X %i\n", win_x, win_y);
            debug_log << "RESIZE: Resizing Window to " << win_x << " X " << win_y << "\n";
            debug_log.flush();
            SDL_SetRenderDrawColor(clock_renderer, 0, 0, 0, 0);
            SDL_RenderClear(clock_renderer);
            SDL_RenderPresent(clock_renderer);
            // recreate the screen panels
           if ((win_x > 10) && (win_y > 10)) {
                SDL_FRect panel_dims;
                debug_log << "RESIZE: Creating New surfaces\n";
                debug_log.flush();
                //Rebuilding Callsign panel
                panel_dims.x = 0;
                panel_dims.y = 0;
                panel_dims.w = (win_x / 6.0f) * 2.0f;
                panel_dims.h = win_y / 8.0f;

                SDL_ClearError();
                debug_log << "RESIZE: Creating Callsign Texture -- ";
                if (!panels[PANEL_CALLSIGN].panel.Create(clock_renderer, panel_dims)) {
                    printf("Error Creating callsign tex: %s\n", SDL_GetError());
                }
                debug_log << "SDL_CreateTexture Result: " << SDL_GetError() << "\n";
                //Rebuilding Clock panel
                panel_dims.x = 0.0f;
                panel_dims.y = win_y / 8.0f;
                panel_dims.w = (win_x / 6.0f) * 2.0f;
                panel_dims.h = win_y / 8.0f;
                SDL_ClearError();
                debug_log << "RESIZE: Creating Clock Texture -- ";
                if (!panels[PANEL_CLOCK].panel.Create(clock_renderer, panel_dims)) {
                    printf("Error Creating Clock: %s\n", SDL_GetError());
                }
                debug_log << "SDL_CreateTexture Result: " << SDL_GetError() << "\n";
                //Rebuilding Map panel
                panel_dims.x = win_x / 6.0f;
                panel_dims.y = win_y / 4.0f;
                panel_dims.w = (win_x / 6.0f) * 5.0f;
                panel_dims.h = (win_y / 4.0f) * 3.0f;
                SDL_ClearError();
                debug_log << "RESIZE: Creating Map Texture -- ";
                if (!panels[PANEL_MAP].panel.Create(clock_renderer, panel_dims)) {
                    printf("Error Creating MAP: %s\n", SDL_GetError());
                }
                debug_log << "SDL_CreateTexture Result: " << SDL_GetError() << "\n";
                //Rebuilding DE panel
                panel_dims.x = 0;
                panel_dims.y = win_y / 4.0f;
                panel_dims.w = win_x / 6.0f;
                panel_dims.h = win_y / 4.0f;
                SDL_ClearError();
                debug_log << "RESIZE: Creating DE Texture -- ";
                if (!panels[PANEL_DE].panel.Create(clock_renderer, panel_dims)) {
                    printf("Error Creating DE: %s\n", SDL_GetError());
                }
               debug_log << "SDL_CreateTexture Result: " << SDL_GetError() << "\n";
                //Rebuilding DX panel");
                panel_dims.x = 0;
                panel_dims.y = win_y / 2.0f;
                panel_dims.w = win_x / 6.0f;
                panel_dims.h = win_y / 4.0f;
                SDL_ClearError();
                debug_log << "RESIZE: Creating DX Texture -- ";
                if (!panels[PANEL_DX].panel.Create(clock_renderer, panel_dims)) {
                    printf("Error Creating DX tex: %s\n", SDL_GetError());
                }
                debug_log << "SDL_CreateTexture Result: " << SDL_GetError() << "\n";
                //Rebuilding col3 panel");
                panel_dims.x = 0;
                panel_dims.y = (win_y / 4.0f) * 3.0f;
                panel_dims.w = win_x / 6.0f;
                panel_dims.h = win_y / 4.0f;
                SDL_ClearError();
                debug_log << "RESIZE: Creating Flex5 Texture -- ";
                if (!panels[PANEL_FLEXBOX5].panel.Create(clock_renderer, panel_dims)) {
                    printf("Error Creating corner tex: %s\n", SDL_GetError());
                }
                debug_log << "SDL_CreateTexture Result: " << SDL_GetError() << "\n";

                //Rebuilding Rowbox1 panel"
                panel_dims.x = (win_x / 6.0f) * 2.0f;
                panel_dims.y = 0.0f;
                panel_dims.w = (win_x / 6.0f);
                panel_dims.h = win_y / 4.0f;
                SDL_ClearError();
                debug_log << "RESIZE: Creating Flex1 Texture -- ";
                if (!panels[PANEL_FLEXBOX1].panel.Create(clock_renderer, panel_dims)) {
                    printf("Error Creating Rowbox1 tex: %s\n", SDL_GetError());
                }
                debug_log << "SDL_CreateTexture Result: " << SDL_GetError() << "\n";
                //Rebuilding Rowbox2 panel
                panel_dims.x = (win_x / 6.0f) * 3.0f;
                panel_dims.y = 0.0f;
                panel_dims.w = (win_x / 6.0f);
                panel_dims.h = win_y / 4.0f;
                SDL_ClearError();
                debug_log << "RESIZE: Creating Flex2 Texture -- ";
                if (!panels[PANEL_FLEXBOX2].panel.Create(clock_renderer, panel_dims)) {
                    printf("Error Creating Rowbox2 tex: %s\n", SDL_GetError());
                }
                debug_log << "SDL_CreateTexture Result: " << SDL_GetError() << "\n";
                //Rebuilding Rowbox3 panel
                panel_dims.x = (win_x / 6.0f) * 4.0f;
                panel_dims.y = 0;
                panel_dims.w = (win_x / 6.0f);
                panel_dims.h = win_y / 4.0f;
                SDL_ClearError();
                debug_log << "RESIZE: Creating Flex3 Texture -- ";
                if (!panels[PANEL_FLEXBOX3].panel.Create(clock_renderer, panel_dims)) {
                    printf("Error Creating Rowbox3 tex: %s\n", SDL_GetError());
                }
                debug_log << "SDL_CreateTexture Result: " << SDL_GetError() << "\n";
                //"Rebuilding Rowbox4 panel
                panel_dims.x = (win_x / 6.0f) * 5.0f;
                panel_dims.y = 0.0f;
                panel_dims.w = (win_x / 6.0f);
                panel_dims.h = win_y / 4.0f;
                SDL_ClearError();
                debug_log << "RESIZE: Creating Flex4 Texture -- ";
                if (!panels[PANEL_FLEXBOX4].panel.Create(clock_renderer, panel_dims)) {
                    printf("Error Creating Rowbox4 tex: %s\n", SDL_GetError());
                }
                debug_log << "SDL_CreateTexture Result: " << SDL_GetError() << "\n";
                //Rebuilding NULL panel
                panel_dims.x = 0.0f;
                panel_dims.y = 0.0f;
                panel_dims.w = 100.0f;
                panel_dims.h = 100.0f;
                SDL_ClearError();
                debug_log << "RESIZE: Creating NULL Texture -- ";
                if (!panels[PANEL_NULL].panel.Create(clock_renderer, panel_dims)) {
                    printf("Error Creating nullframe tex: %s\n", SDL_GetError());
                }
                debug_log << "SDL_CreateTexture Result: " << SDL_GetError() << "\n";
                panel_assignment(false);

                debug_log << "RESIZE: Reloading Maps and Icon assets\n";
                debug_log.flush();
                icon_bin.reload_icons(clock_renderer);
                debug_log << "RESIZE: Re-enabling program loops\n";
                debug_log.flush();
             // re-enable the rest of the program
                master_flags.callsign.draw_flag         =       true;
                master_flags.de.draw_flag               =       true;
                master_flags.dx.draw_flag               =       true;
                master_flags.pota.draw_flag             =       true;
                master_flags.sat_tracker.draw_flag      =       true;
                master_flags.dx_spots.draw_flag         =       true;
                master_flags.ncdxf.draw_flag            =       true;
                master_flags.map.draw_flag              =       true;
                master_flags.clock.draw_flag            =       true;
                master_flags.solar.draw_flag            =       true;
                master_flags.wspr.draw_flag             =       true;
                master_flags.lunar.draw_flag            =       true;
                master_flags.psk.draw_flag              =       true;
                master_flags.contests.draw_flag         =       true;
                master_flags.rss.draw_flag              =       true;
                master_flags.aurora.draw_flag           =       true;

                for (struct PluginModule& plugin : loaded_plugins ) {
                    plugin.draw_flag = true;
                }
                // enable the masin system clock
                flag_timer = SDL_AddTimer(100, master_clock, &master_flags);
                debug_log << "RESIZE: Window resize complete\n";
                debug_log.flush();
            }
            SDL_UnlockMutex(mutexes[MUTEX_NIGHT_MASK]);
            SDL_UnlockMutex(mutexes[MUTEX_RESIZE]);
        }
 else {
     SDL_Log("DUPLICATE RESIZE CALL ERROR!");
        }
#ifdef _WIN32
#ifdef _DEBUG
        _ASSERTE(_CrtCheckMemory());
#endif
#endif
        return;
}

