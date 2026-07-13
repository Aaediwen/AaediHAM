#include "aaediclock.h"
#include "panels.h"
#include "core/core.h"
//#include "plugins/host_api.h"
std::string render_engine;
Uint16 interrupt_counter = 0;


void panel_assignment(bool increment) {
    // function to assign panels to the modules
    // default them all to the NULL panel
    for (struct PluginModule& plugin : loaded_plugins ) {
        plugin.draw_flag = false;
        plugin.host_api->panel = &(winboxes[PANEL_NULL].panel);
    }

    // step through each screen panel
    for (auto& panel : winboxes) {
        //increment plugin counter
        if (panel.plugin_sequence.size()) {
            if(increment) {
                panel.plugin_index++;
                if (panel.plugin_index >= panel.plugin_sequence.size()) { panel.plugin_index = 0 ; }
            }
        }
        // assign plugin
        if ((!loaded_plugins.empty()) && (!panel.plugin_sequence.empty())) {
            if ((panel.plugin_index < panel.plugin_sequence.size()) && (panel.plugin_index < loaded_plugins.size())) {
                loaded_plugins[panel.plugin_sequence[panel.plugin_index]].host_api->panel = &panel.panel;
                debug_log << "MOD PAGER: Index:"<< panel.plugin_index << "\tseq size:" << panel.plugin_sequence.size() <<"\tseq id:" << panel.plugin_sequence[panel.plugin_index] << "\n";
                debug_log << "MOD PAGER: Setting Plugin: "<< loaded_plugins[panel.plugin_sequence[panel.plugin_index]].name << "to panel " << &panel.panel << "\n";
            }
        }
    }
}

Uint32 SDLCALL master_clock (void *userdata, SDL_TimerID timerID, Uint32 interval) {
//    SDL_Log ("FLAG TIMER: In Master flag timer\n");
// master clock to trigger each module
    (void) userdata;
    interrupt_counter++;
//    std::cout << "FLAG_TIMER: In Master Flag Timer "<< interrupt_counter << "\t" << timerID << "\n";
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
        if ((interrupt_counter % 5) == 0) {   // 0.5 seconds
            write_image = true;
        }
        for (struct PluginModule& plugin : loaded_plugins ) {
            if (plugin.interval) {
                if ((interrupt_counter % plugin.interval) == 0) {
                    plugin.draw_flag = true;
                }
            }
        }


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

            debug_log << "RESIZE: Beginning Window resize\n";
            debug_log.flush();
            // lock and disable the rest of the program
            debug_log << "RESIZE: Disabling renders\n";
            debug_log.flush();
            SDL_LockMutex(mutexes[MUTEX_NIGHT_MASK]);
            for (struct PluginModule& plugin : loaded_plugins ) {
                plugin.draw_flag = false;
                plugin.host_api->panel = nullptr;
            }

            debug_log << "RESIZE: Destroying old surfaces\n";
            debug_log.flush();
            // clean up the old surface
            if (clock_renderer) {

                debug_log << "RESIZE: Clearing Overlays\n";
                std::cout.flush();
                overlays.clear();
                debug_log << "RESIZE: Clearing Screen Panels\n";
                debug_log.flush();
                panels[PANEL_CALLSIGN].panel.Reset();
                panels[PANEL_NULL].panel.Reset();
                panels[PANEL_DE].panel.Reset();
                panels[PANEL_DX].panel.Reset();
                panels[PANEL_CLOCK].panel.Reset();
                panels[PANEL_FLEXBOX1].panel.Reset();
                panels[PANEL_FLEXBOX2].panel.Reset();
                panels[PANEL_FLEXBOX3].panel.Reset();
                panels[PANEL_FLEXBOX4].panel.Reset();
                panels[PANEL_FLEXBOX5].panel.Reset();
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
                icon_bin.clear_icons();
                debug_log << "RESIZE: Re-enabling program loops\n";
                debug_log.flush();
             // re-enable the rest of the program

                for (struct PluginModule& plugin : loaded_plugins ) {
                    plugin.draw_flag = true;
                }
                // enable the masin system clock
                flag_timer = SDL_AddTimer(100, master_clock, nullptr);
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

