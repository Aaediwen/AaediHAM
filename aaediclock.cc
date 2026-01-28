#define SDL_MAIN_USE_CALLBACKS
#include <fstream>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include "aaediclock.h"
#include "utils.h"
#include "modules.h"

#ifdef _WIN32
#include <cstdlib>
#include <windows.h>
#include <wingdi.h>
#else
#include <fontconfig/fontconfig.h>
#endif


static SDL_Window		*window = nullptr;
static SDL_Renderer		*clock_renderer = nullptr;
TTF_Font		        *Sans = nullptr;
time_t 			        currenttime;
//ScreenFrame 	        DayMap;
//ScreenFrame 	        NightMap;
//ScreenFrame 	        CountriesMap;
std::array<pager_node, 12> winboxes;
std::array<SDL_Mutex*, 10> mutexes = { nullptr };

struct internal_mouse_event clock_mouse_event;

struct map_pin 		    *map_pins;
struct data_blob	    *data_cache;
config 		            clockconfig;

SDL_TimerID map_timer = 0;
Uint16 interrupt_counter = 0;
struct regen_mask_args* night_mask_args = nullptr;
map_overlay overlays;
map_icons icon_bin;
//std::ostream& debug_log;
#ifdef CLOCK_DEBUG
static std::ofstream logfile("clock_debug.log");
std::ostream& debug_log = logfile;
#else
static nullbuf nb;
static std::ostream nullout(&nb);
std::ostream& debug_log = nullout;
#endif
struct celest_coords g_celestials;
bool headless = false;
bool reload_flag = false;
std::string render_engine;
Sint64 max_tex_size = 0;
struct ModuleControl {
    bool draw_flag = true;
    ScreenFrame* panel = &winboxes[PANEL_NULL].panel;
};

struct {
    ModuleControl sat_tracker;
    ModuleControl dx_spots;
    ModuleControl callsign;
    ModuleControl de;
    ModuleControl dx;
    ModuleControl pota;
    ModuleControl ncdxf;
    ModuleControl map;
    ModuleControl clock;
    ModuleControl kindex;
    ModuleControl solar;
    ModuleControl wspr;
    ModuleControl lunar;
    ModuleControl psk;
    ModuleControl contests;
    ModuleControl rss;
} static master_flags;


SDL_TimerID flag_timer = 0;

void panel_assignment(bool increment) {
            master_flags.map.panel	=	&(winboxes[PANEL_MAP].panel);
            master_flags.sat_tracker.panel	=	&(winboxes[PANEL_NULL].panel);
            master_flags.dx_spots.panel	=	&(winboxes[PANEL_NULL].panel);
            master_flags.callsign.panel	=	&(winboxes[PANEL_NULL].panel);
            master_flags.de.panel	=	&(winboxes[PANEL_NULL].panel);
            master_flags.dx.panel	=	&(winboxes[PANEL_NULL].panel);
            master_flags.pota.panel	=	&(winboxes[PANEL_NULL].panel);
            master_flags.ncdxf.panel	=	&(winboxes[PANEL_NULL].panel);
            master_flags.clock.panel	=	&(winboxes[PANEL_NULL].panel);
            master_flags.kindex.panel	=	&(winboxes[PANEL_NULL].panel);
            master_flags.solar.panel	=	&(winboxes[PANEL_NULL].panel);
            master_flags.wspr.panel	=	&(winboxes[PANEL_NULL].panel);
            master_flags.lunar.panel	=	&(winboxes[PANEL_NULL].panel);
            master_flags.psk.panel	=	&(winboxes[PANEL_NULL].panel);
            master_flags.contests.panel = 	&(winboxes[PANEL_NULL].panel);
            master_flags.rss.panel	=	&(winboxes[PANEL_NULL].panel);
    for (auto& panel : winboxes) {
        if (panel.sequence.size()) {
            if (increment) {
                panel.index++;
                if (panel.index >= panel.sequence.size()) { panel.index = 0 ; }
            }
            switch (panel.sequence[panel.index]) {
                case MOD_MAP:
                    master_flags.map.panel = &panel.panel;
                    break;
                case MOD_DE:
                    master_flags.de.panel = &panel.panel;
                    break;
                case MOD_DX:
                    master_flags.dx.panel = &panel.panel;
                    break;
                case MOD_CLOCK:
                    master_flags.clock.panel = &panel.panel;
                    break;
                case MOD_CALL:
                    master_flags.callsign.panel = &panel.panel;
                    break;
                case MOD_POTA:
                    master_flags.pota.panel = &panel.panel;
                    break;
                case MOD_PSK:
                    master_flags.psk.panel = &panel.panel;
                    break;
                case MOD_SAT:
                    master_flags.sat_tracker.panel = &panel.panel;
                    break;
                case MOD_DXSPOT:
                    master_flags.dx_spots.panel = &panel.panel;
                    break;
                case MOD_KINDEX:
                    master_flags.kindex.panel = &panel.panel;
                    break;
                case MOD_CONTESTS:
                    master_flags.contests.panel = &panel.panel;
                    break;
                case MOD_NCDXF:
                    master_flags.ncdxf.panel = &panel.panel;
                    break;
                case MOD_SOLAR:
                    master_flags.solar.panel = &panel.panel;
                    break;
                case MOD_WSPR:
                    master_flags.wspr.panel = &panel.panel;
                    break;
                case MOD_LUNAR:
                    master_flags.lunar.panel = &panel.panel;
                    break;
                case MOD_RSS:
                    master_flags.rss.panel = &panel.panel;
                    break;
                case MOD_NULL:
                    break;
            }
        }
    }
}


Uint32 SDLCALL master_clock (void *userdata, SDL_TimerID timerID, Uint32 interval) {
//    SDL_Log ("FLAG TIMER: In Master flag timer\n");
    (void) userdata;
    interrupt_counter++;

    if (interrupt_counter > 4800) {
        interrupt_counter = 0;
    }
    if (timerID) {
        SDL_LockMutex(mutexes[MUTEX_MASTER_CLOCK]);

        if ((interrupt_counter % 600) == 0) {	// 60 seconds
            debug_log << "FLAG_TIMER: MOD PAGER FIRED!\n";
            debug_log.flush();
            panel_assignment(true);
        }


        if ((interrupt_counter % 300)==0) {	// 30 seconds
            master_flags.callsign.draw_flag = true;
        }

        if ((interrupt_counter % 170)==0) {	// 17 seconds

        }


        if ((interrupt_counter % 50)==0) {	// 5 seconds
            master_flags.de.draw_flag = true;
            master_flags.dx.draw_flag = true;
            master_flags.pota.draw_flag = true;
            master_flags.ncdxf.draw_flag = true;
        }

        if ((interrupt_counter % 50)==10) {	// 5 seconds +1
            master_flags.dx_spots.draw_flag = true;
            master_flags.psk.draw_flag = true;
            master_flags.contests.draw_flag = true;
        }
        if ((interrupt_counter % 50)==20) {	// 5 seconds +2

        }
        if ((interrupt_counter % 50)==30) {	// 5 seconds +3
            master_flags.kindex.draw_flag = true;
            master_flags.wspr.draw_flag = true;
            master_flags.lunar.draw_flag = true;
        }
        if ((interrupt_counter % 50)==40) {	// 5 seconds	+4
            master_flags.solar.draw_flag = true;
        }
        if ((interrupt_counter % 20)==0) {	// 2 seconds

        }

        if ((interrupt_counter % 10)==0) {	// 1 second
            master_flags.map.draw_flag = true;
            master_flags.sat_tracker.draw_flag = true;
        }
        if ((interrupt_counter % 2)==0) {	// .2 seconds
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


            master_flags.callsign.draw_flag 	= 	false;
            master_flags.de.draw_flag 		= 	false;
            master_flags.dx.draw_flag 		= 	false;
            master_flags.pota.draw_flag 	= 	false;
            master_flags.sat_tracker.draw_flag 	= 	false;
            master_flags.dx_spots.draw_flag 	=	false;
            master_flags.map.draw_flag 		= 	false;
            master_flags.ncdxf.draw_flag 	= 	false;
            master_flags.kindex.draw_flag 	= 	false;
            master_flags.clock.draw_flag 	= 	false;
            master_flags.solar.draw_flag 	= 	false;
            master_flags.wspr.draw_flag 	= 	false;
            master_flags.lunar.draw_flag 	= 	false;
            master_flags.psk.draw_flag 		= 	false;
            master_flags.contests.draw_flag 	= 	false;
            master_flags.rss.draw_flag 		=	false;

            master_flags.map.panel      	=       nullptr;
            master_flags.sat_tracker.panel      =       nullptr;
            master_flags.dx_spots.panel 	=       nullptr;
            master_flags.callsign.panel 	=       nullptr;
            master_flags.de.panel       	=       nullptr;
            master_flags.dx.panel       	=       nullptr;
            master_flags.pota.panel     	=       nullptr;
            master_flags.ncdxf.panel    	=       nullptr;
            master_flags.clock.panel    	=       nullptr;
            master_flags.kindex.panel   	=       nullptr;
            master_flags.solar.panel    	=       nullptr;
            master_flags.wspr.panel     	=       nullptr;
            master_flags.lunar.panel    	=       nullptr;
            master_flags.psk.panel    		=       nullptr;
            master_flags.contests.panel		=	nullptr;
            master_flags.rss.panel		=	nullptr;

            debug_log << "RESIZE: Destroying old surfaces\n";
            debug_log.flush();
            // clean up the old surface
            if (clock_renderer) {

                debug_log << "RESIZE: Clearing Overlays\n";
                std::cout.flush();
                overlays.clear();
                debug_log << "RESIZE: Resetting Maps\n";
                debug_log.flush();
//                DayMap.Reset();
//                NightMap.Reset();
//                CountriesMap.Reset();
                debug_log << "RESIZE: Clearing Screen Panels\n";
                debug_log.flush();
                debug_log << "RESIZE: Callsign: " << &(panels[PANEL_CALLSIGN].panel) << "\n";
                debug_log.flush();
                panels[PANEL_CALLSIGN].panel.Reset();

                debug_log << "RESIZE: Null: " << &(panels[PANEL_NULL].panel) << "\n";
                debug_log.flush();
                panels[PANEL_NULL].panel.Reset();
                debug_log << "RESIZE: DE: " << &(panels[PANEL_DE].panel) << "\n";
                debug_log.flush();
                panels[PANEL_DE].panel.Reset();
                debug_log << "RESIZE: DX: " << &(panels[PANEL_DX].panel) << "\n";
                debug_log.flush();
                panels[PANEL_DX].panel.Reset();
                debug_log << "RESIZE: Clock: " << &(panels[PANEL_CLOCK].panel) << "\n";
                debug_log.flush();
                panels[PANEL_CLOCK].panel.Reset();
                debug_log << "RESIZE: Flex1: " << &(panels[PANEL_FLEXBOX1].panel) << "\n";
                debug_log.flush();
                panels[PANEL_FLEXBOX1].panel.Reset();
                debug_log << "RESIZE: Flex2: " << &(panels[PANEL_FLEXBOX2].panel) << "\n";
                debug_log.flush();
                panels[PANEL_FLEXBOX2].panel.Reset();
                debug_log << "RESIZE: Flex3: " << &(panels[PANEL_FLEXBOX3].panel) << "\n";
                debug_log.flush();
                panels[PANEL_FLEXBOX3].panel.Reset();
                debug_log << "RESIZE: Flex4: " << &(panels[PANEL_FLEXBOX4].panel) << "\n";
                debug_log.flush();
                panels[PANEL_FLEXBOX4].panel.Reset();

                debug_log << "RESIZE: Flex5: " << &(panels[PANEL_FLEXBOX5].panel) << "\n";
                debug_log.flush();
                panels[PANEL_FLEXBOX5].panel.Reset();
                debug_log << "RESIZE: Map: " << &(panels[PANEL_MAP].panel) << "\n";
                debug_log.flush();
                panels[PANEL_MAP].panel.Reset();
                debug_log.flush();


                SDL_SetRenderTarget(clock_renderer, nullptr);
                debug_log << "RESIZE: Flushing and finishing pending renderer ops before destroy\n";


//                SDL_DestroyRenderer(clock_renderer);
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
//            for (auto& p : panels) {
//                p.panel.SetRenderer(clock_renderer);
//            }
            SDL_GetCurrentRenderOutputSize(clock_renderer, &win_x, &win_y);
            printf("Resizing Window to %i X %i\n", win_x, win_y);
            debug_log << "RESIZE: Resizing Window to " << win_x << " X " << win_y << "\n";
            debug_log.flush();
            SDL_SetRenderDrawColor(clock_renderer, 0, 0, 0, 0);
            SDL_RenderClear(clock_renderer);
            SDL_RenderPresent(clock_renderer);

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
                debug_log << "RESIZE Creating Callsign Texture -- ";
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
                debug_log << "RESIZE Creating Clock Texture -- ";
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
                debug_log << "RESIZE Creating Map Texture -- ";
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
                debug_log << "RESIZE Creating DE Texture -- ";
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
                debug_log << "RESIZE Creating DX Texture -- ";
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
                debug_log << "RESIZE Creating Flex5 Texture -- ";
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
                debug_log << "RESIZE Creating Flex1 Texture -- ";
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
                debug_log << "RESIZE Creating Flex2 Texture -- ";
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
                debug_log << "RESIZE Creating Flex3 Texture -- ";
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
                debug_log << "RESIZE Creating Flex4 Texture -- ";
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
                debug_log << "RESIZE Creating NULL Texture -- ";
                if (!panels[PANEL_NULL].panel.Create(clock_renderer, panel_dims)) {
                    printf("Error Creating nullframe tex: %s\n", SDL_GetError());
                }
                debug_log << "SDL_CreateTexture Result: " << SDL_GetError() << "\n";
                panel_assignment(false);

                debug_log << "RESIZE: Reloading Maps and Icon assets\n";
                debug_log.flush();
                // recreate the map textures as well so they don't get lost
//                load_maps(clock_renderer, panels[PANEL_MAP].panel.dims);
                icon_bin.reload_icons(clock_renderer);
                debug_log << "RESIZE: Re-enabling program loops\n";
                debug_log.flush();
                // re-enable the rest of the program
                master_flags.callsign.draw_flag 	= 	true;
                master_flags.de.draw_flag 		= 	true;
                master_flags.dx.draw_flag 		= 	true;
                master_flags.pota.draw_flag 		= 	true;
                master_flags.sat_tracker.draw_flag 	= 	true;
                master_flags.dx_spots.draw_flag 	= 	true;
                master_flags.ncdxf.draw_flag 		= 	true;
                master_flags.map.draw_flag 		= 	true;
                master_flags.clock.draw_flag 		= 	true;
                master_flags.solar.draw_flag 		= 	true;
                master_flags.wspr.draw_flag 		= 	true;
                master_flags.lunar.draw_flag 		= 	true;
                master_flags.psk.draw_flag 		= 	true;
                master_flags.contests.draw_flag 	= 	true;
                master_flags.rss.draw_flag 		= 	true;

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



#ifdef _WIN32
extern "C" int CALLBACK WinFontCallback(const LOGFONT * lpelfe, const TEXTMETRIC * lpntme, DWORD FontType, LPARAM lParam) {
    (void)FontType;
    (void)lpntme;
    std::string* outFontName = reinterpret_cast<std::string*>(lParam);
    if (lParam && lpelfe && lpelfe->lfFaceName[0]) {  // we got a font with a name
        *outFontName = lpelfe->lfFaceName;
//        SDL_Log("Font Callback Checking %s", outFontName->c_str());
        if (lpelfe->lfItalic || lpelfe->lfUnderline || lpelfe->lfStrikeOut) {
            SDL_Log("Font Callback Bad Font, %s", outFontName->c_str());
            return 1; // Next Font
        }
        return 0;

    }
    else {
        SDL_Log("Font Callback Bad Font, no name");
        return 1; // Continue enumeration
    }

}
#endif

std::string FindFont(const char* fontname) {

    std::string path;
    path.clear();
#ifndef _WIN32
    FcInit();
    FcPattern* pat = FcNameParse((const FcChar8*)fontname);	// generate a FontConfig pattern class for "sans"
    FcBool checksubs = FcConfigSubstitute(NULL, pat, FcMatchPattern);	// pattern match the font name
    if (checksubs) {
    FcDefaultSubstitute(pat);					// get default options in *pat

    FcResult fcresult;
    FcPattern *font =  FcFontMatch(NULL, pat, &fcresult);	// find closest font match for 'sans'

    if (font) {
        FcChar8 *file, *style, *family;
        FcPatternGetString(font, FC_FILE, 0, &file);
        FcPatternGetString(font, FC_FAMILY, 0, &family);
        FcPatternGetString(font, FC_STYLE, 0, &style);
        printf("Font Filename: %s (family %s, style %s)\n", file, family, style);
        path = reinterpret_cast<char*>(file);
        FcPatternDestroy(font);
    } else {
        SDL_Log("No valid font found!");
    }
    } else {
        SDL_Log("FontConfig Substitute Check Failure!");
    }
    FcPatternDestroy(pat);
    FcFini();
#else


    struct ::LOGFONTA font_criteria;
    font_criteria.lfHeight 		= 	0;
    font_criteria.lfWidth 		= 	0;
    font_criteria.lfEscapement 		= 	0;
    font_criteria.lfOrientation 	= 	0;
    font_criteria.lfWeight 		= 	FW_NORMAL;
    font_criteria.lfItalic 		= 	FALSE;
    font_criteria.lfUnderline 		= 	FALSE;
    font_criteria.lfStrikeOut 		= 	FALSE;
    font_criteria.lfCharSet 		= 	DEFAULT_CHARSET;
    font_criteria.lfOutPrecision 	= 	OUT_DEFAULT_PRECIS;
    font_criteria.lfClipPrecision 	= 	CLIP_DEFAULT_PRECIS;
    font_criteria.lfQuality 		= 	DEFAULT_QUALITY;
    font_criteria.lfPitchAndFamily 	= 	DEFAULT_PITCH & FF_DONTCARE;
    strncpy_s(font_criteria.lfFaceName, fontname, LF_FACESIZE);
    std::string facename;
    EnumFontFamiliesExA(GetDC(NULL), &font_criteria, WinFontCallback, reinterpret_cast<LPARAM>(&facename), 0);


    HKEY hKey;
    LONG result = RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Fonts", 0, KEY_READ, &hKey );
    if (result != ERROR_SUCCESS)
    {
        if (result == ERROR_FILE_NOT_FOUND) {
            printf("Font Registry Key not found.\n");
            return (path);
        }
        else {
            printf("Error Font Registry key.\n");
            return (path);
        }
    }

    char valueName[256];
    BYTE valueData[256];
    DWORD valueNameSize, valueDataSize, valueType;
    std::string fontFilename;
    for (DWORD i = 0; ; ++i) {                              // hunt through teh registry for the indicated font
        valueNameSize = sizeof(valueName);
        valueDataSize = sizeof(valueData);
        result = RegEnumValueA( hKey, i, valueName, &valueNameSize, nullptr, &valueType, valueData, &valueDataSize ); // read the next registry key

        if (result == ERROR_NO_MORE_ITEMS) break;           // check if we read a valid key
        if (result != ERROR_SUCCESS) continue;

                                                // if we did, is it the one we want?
        if (std::string(valueName).find(facename+" (") != std::string::npos && valueType == REG_SZ) {
            fontFilename = reinterpret_cast<const char*>(valueData);
            break;
        }
    }
    std::string fullFontPath;
    if (!fontFilename.empty()) {
        char winDir[MAX_PATH];
        GetWindowsDirectoryA(winDir, MAX_PATH);
        path = std::string(winDir) + "\\Fonts\\" + fontFilename;
    }



    RegCloseKey(hKey);
#endif
    return (path);

}


int window_init(int x, int y) {
    if (!window) {
        // create the main window
        window = SDL_CreateWindow("Aaediwen Ham Clock", x, y, 0);
        if (!window) {
            SDL_Log("Failed to create window: %s", SDL_GetError());
            debug_log << "INIT: Failed to Create Window: " << SDL_GetError() << "\n";
            return(1);
        }
        SDL_SetWindowResizable(window, 1);
        resize_panels(winboxes);
        if (!window || !clock_renderer) {
            printf("Window Renderer error\n");
            debug_log << "INIT: Window Renderer error\n";
            return(1);
        }
        // load assets
#ifndef _WIN32
        Sans = TTF_OpenFont(FindFont("sans").c_str(), 72);
#else
        Sans = TTF_OpenFont(FindFont("Arial").c_str(), 72);
#endif
        if (!Sans) {
            printf("Error opening font: %s\n", SDL_GetError());
            debug_log << "INIT: Error opening font: "<< SDL_GetError() << "\n";
            return(1);
        }
        TTF_SetFontHinting(Sans, TTF_HINTING_LIGHT_SUBPIXEL);



        SDL_RenderPresent(clock_renderer);

        master_flags.callsign.draw_flag		= true;
        master_flags.de.draw_flag		= true;
        master_flags.dx.draw_flag		= true;
        master_flags.pota.draw_flag		= true;
        master_flags.sat_tracker.draw_flag	= true;
        master_flags.dx_spots.draw_flag		= true;
        master_flags.map.draw_flag		= true;
        master_flags.clock.draw_flag		= true;
        master_flags.kindex.draw_flag		= true;
        master_flags.solar.draw_flag		= true;
        master_flags.wspr.draw_flag		= true;
        master_flags.lunar.draw_flag		= true;
        master_flags.contests.draw_flag 	= true;
        master_flags.rss.draw_flag		= true;
        if (!flag_timer) {
            flag_timer = SDL_AddTimer(100, master_clock, &master_flags);
        }
    }
    return 0;
}


int window_destroy() {
    debug_log << "EXIT: Exiting Normally.\n\n";
    SDL_Quit();
    return 0;
}

std::string outfile;
SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv) {
    // initialize required subsystems
    (void)appstate;
    int x, y;
    x=800;
    y=480;
#ifdef CLOCK_DEBUG
#ifdef _WIN32
#ifdef _DEBUG
 //   _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF | _CRTDBG_DELAY_FREE_MEM_DF );
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
    HANDLE hLogFile;
    hLogFile = CreateFile("CRTlog.txt", GENERIC_WRITE,
        FILE_SHARE_WRITE, NULL, CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, NULL);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR, hLogFile);
#endif
#endif
//    debug_log.open("clock_debug.log", std::fstream::out);
#else
#ifdef _WIN32
//    debug_log.open ("NUL", std::fstream::out);
#else
//    debug_log.open ("/dev/null", std::fstream::out);
#endif
#endif
    debug_log << "------------------------ NEW RUN ------------\n";
    bool fs_start = false;
    outfile.clear();
    render_engine.clear();
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--headless") {
            printf("Running Headless\n");
            debug_log << "INIT: Running Headless\n";
#ifdef _WIN32
            _putenv_s("SDL_VIDEO_DRIVER", "dummy");
#else
            setenv("SDL_VIDEO_DRIVER", "dummy", 1);
#endif
            SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER, "dummy", SDL_HINT_OVERRIDE);
            headless=true;
        } else if (arg.rfind("--renderer",0)==0) {
            size_t eqpos = arg.find('=');
             if (eqpos != std::string::npos) {
                 render_engine = arg.substr(11);
                 if (render_engine == "list" || render_engine == "help") {
                     int maxcount = SDL_GetNumRenderDrivers();
                     for (int c = 0; c < maxcount ; c++) {
                         printf("Driver %i: %s\n", c, SDL_GetRenderDriver(c));
                     }
                     return (SDL_APP_SUCCESS);
                 }
                 printf("Attempting to use SDL Rendering Engine: %s\n", render_engine.c_str());
             } else {
                    printf("Invalid Renderer: %s\n", arg.c_str());
             }
        } else if (arg.rfind("--geometry",0)==0) {
             size_t eqpos = arg.find('=');
             if (eqpos != std::string::npos) {
                 std::string geom = arg.substr(11);
                 printf("Attempting to use default geometry: %s\n", geom.c_str());
                 size_t x_pos = geom.find('x');
                 if (x_pos != std::string::npos) {
                     std::string w = geom.substr(0, x_pos);
                     std::string h = geom.substr(x_pos + 1);
                     try {
                         x = std::stoi(w);
                         y = std::stoi(h);
                    } catch (const std::exception& e) {
                        (void)e;
                        printf("Invalid Renderer Geometry: %s\n", geom.c_str());
                        return (SDL_APP_FAILURE);
                    }
                 } else {
                     printf("Invalid Renderer Geometry: %s\n", geom.c_str());
                 }
             } else {
                    printf("Invalid Renderer Geometry: %s\n", arg.c_str());
             }
        } else if (arg.rfind("--outfile",0)==0) {
            outfile = arg.substr(10);
            printf("Attempting to use output file: %s", outfile.c_str());
        } else if (arg == "--fullscreen") {
            fs_start = true;
        } else if (arg == "--help") {
            printf("Usage: %s [--headless] [geometry=<width>x<height>] [--output=<outfile>]\n", argv[0]);
            printf("Options:\n");
            printf("\t--headless\tRun in a headless mode with graphical output redirected to a disk file\n");
            printf("\t--fullscreen\tStart in fullscreen mode\n");
            printf("\t--renderer\tset the SDL renderer to use. renderer=help or renderer=list will show a list of avaliable rendering engines\n");
            printf("\t--geometry\tResolution of the output from --headless, or the starting window resolution in a GUI environment\n");
            printf("\t--output\tOutput file path for --headless\n");
            printf("\t--QRZ_Pass\tSet the password to use for QRZ.com (uses the Callsign for UserName)\n");
            printf("\t--help\t\tThis help text\n");
            return (SDL_APP_SUCCESS);
        } else if (arg.rfind("--QRZ_Pass",0)==0) {
            std::string password;
            password = arg.substr(11);
            clockconfig.set_qrz_pass(password);
            printf("QRZ password set.\n");
            return (SDL_APP_SUCCESS);
        } else if (arg.rfind("--lunartest",0)==0) {
            // lunar test code
            outfile = "lunartest.jpg";
            x=1280;
            y=720;
#ifdef _WIN32
            _putenv_s("SDL_VIDEO_DRIVER", "dummy");
#else
            setenv("SDL_VIDEO_DRIVER", "dummy", 1);
#endif
            SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER, "dummy", SDL_HINT_OVERRIDE);
            headless=true;
            if (!(SDL_InitSubSystem(SDL_INIT_VIDEO))) {
                SDL_Log("Unable to initialize SDL: %s", SDL_GetError());
                debug_log << "INIT: Unable to initialize SDL:" << SDL_GetError() << "\n";
                return (SDL_APP_FAILURE);
            }

            if(!TTF_Init()) {
                printf("TTF_Init Error: %s\n", SDL_GetError());
                debug_log << "INIT: TTF Init Error:" << SDL_GetError() << "\n";
                return(SDL_APP_FAILURE);
            }
            winboxes[PANEL_CALLSIGN].panel.Reset();
            winboxes[PANEL_MAP].panel.Reset();
            winboxes[PANEL_DX].panel.Reset();
            winboxes[PANEL_DE].panel.Reset();
            winboxes[PANEL_CLOCK].panel.Reset();
            winboxes[PANEL_FLEXBOX1].panel.Reset();
            winboxes[PANEL_FLEXBOX2].panel.Reset();
            winboxes[PANEL_FLEXBOX3].panel.Reset();
            winboxes[PANEL_FLEXBOX4].panel.Reset();
            winboxes[PANEL_NULL].panel.Reset();
            winboxes[PANEL_FLEXBOX5].panel.Reset();
            if (window_init(x, y)) {
                return (SDL_APP_FAILURE);
            }
            time_t lunartime = time(NULL);
            for (int count = 0; count < 365 ; count++) {
                sdo_image(*(master_flags.solar.panel), lunartime);

                lunar_module((winboxes[PANEL_MAP].panel), lunartime);

                winboxes[PANEL_MAP].panel.present();
                SDL_RenderPresent(clock_renderer);
                    // dump surface to image file here
                    int width, height;
                    SDL_GetCurrentRenderOutputSize(clock_renderer, &width, &height);
                    SDL_Surface* savesurface = SDL_RenderReadPixels(clock_renderer, NULL);
                    std::string filename = std::to_string(count)+outfile;
                    SDL_Log("Rendering to %s", filename.c_str());
                    IMG_SaveJPG(savesurface, filename.c_str(), 75);
                    SDL_DestroySurface(savesurface);
                    savesurface = nullptr;
                lunartime += (24*60*60);
            }

            return (SDL_APP_SUCCESS);
            // end lunar test code
        }
    }

    if (!(SDL_InitSubSystem(SDL_INIT_VIDEO))) {
        SDL_Log("Unable to initialize SDL: %s", SDL_GetError());
        debug_log << "INIT: Unable to initialize SDL:" << SDL_GetError() << "\n";
        return (SDL_APP_FAILURE);
    }

    if(!TTF_Init()) {
        printf("TTF_Init Error: %s\n", SDL_GetError());
        debug_log << "INIT: TTF Init Error:" << SDL_GetError() << "\n";
        return(SDL_APP_FAILURE);
    }

    // init globals
    map_pins			=	0;
    data_cache			=	0;
    clock_mouse_event.mod_cords = {0.0, 0.0};
    clock_mouse_event.mod_count = 0;
    clock_mouse_event.mod_owner = MOD_NULL;

    winboxes[PANEL_CALLSIGN].panel.Reset();
    winboxes[PANEL_MAP].panel.Reset();
    winboxes[PANEL_DX].panel.Reset();
    winboxes[PANEL_DE].panel.Reset();
    winboxes[PANEL_CLOCK].panel.Reset();
    winboxes[PANEL_FLEXBOX1].panel.Reset();
    winboxes[PANEL_FLEXBOX2].panel.Reset();
    winboxes[PANEL_FLEXBOX3].panel.Reset();
    winboxes[PANEL_FLEXBOX4].panel.Reset();
    winboxes[PANEL_NULL].panel.Reset();
    winboxes[PANEL_FLEXBOX5].panel.Reset();

    winboxes[PANEL_MAP].sequence.push_back(MOD_MAP);
    winboxes[PANEL_CALLSIGN].sequence.push_back(MOD_CALL);
    winboxes[PANEL_CLOCK].sequence.push_back(MOD_CLOCK);
    winboxes[PANEL_DE].sequence.push_back(MOD_DE);
    winboxes[PANEL_DX].sequence.push_back(MOD_DX);
    winboxes[PANEL_FLEXBOX1].sequence.push_back(MOD_POTA);
    winboxes[PANEL_FLEXBOX1].sequence.push_back(MOD_NCDXF);
    winboxes[PANEL_FLEXBOX2].sequence.push_back(MOD_SAT);
    winboxes[PANEL_FLEXBOX2].sequence.push_back(MOD_PSK);
    winboxes[PANEL_FLEXBOX3].sequence.push_back(MOD_DXSPOT);
    winboxes[PANEL_FLEXBOX4].sequence.push_back(MOD_KINDEX);
    winboxes[PANEL_FLEXBOX4].sequence.push_back(MOD_CONTESTS);
    winboxes[PANEL_FLEXBOX5].sequence.push_back(MOD_SOLAR);
    winboxes[PANEL_FLEXBOX5].sequence.push_back(MOD_WSPR);
    winboxes[PANEL_FLEXBOX5].sequence.push_back(MOD_LUNAR);
    debug_log << "INIT: Globals Initialized\n";


    mutexes[MUTEX_NIGHT_MASK] 	= SDL_CreateMutex();
    mutexes[MUTEX_RESIZE]	= SDL_CreateMutex();
    mutexes[MUTEX_CACHE]	= SDL_CreateMutex();
    mutexes[MUTEX_HTTP]		= SDL_CreateMutex();
    mutexes[MUTEX_CELESTRAK]	= SDL_CreateMutex();
    mutexes[MUTEX_WSPR]		= SDL_CreateMutex();
    mutexes[MUTEX_CONTESTS]	= SDL_CreateMutex();

    night_mask_args = (struct regen_mask_args*)malloc(sizeof(struct regen_mask_args));
    map_timer = 0;
    debug_log << "INIT: Map Variables Initialized\n";
    // create the main window
    if (window_init(x, y)) {
        return (SDL_APP_FAILURE);
    }
    if (fs_start) {
        SDL_SetWindowFullscreen(window, 1);
    }

    return(SDL_APP_CONTINUE);

}


    // SDL Loop
time_t oldtime;
int resizing 			= 	0;
SDL_AppResult SDL_AppIterate(void *appstate) {
    (void)appstate;
    SDL_Delay(10);			// slow down the program
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
        const Uint64 StartTicks = SDL_GetTicks();
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
        currenttime=time(NULL);
        SDL_LockMutex(mutexes[MUTEX_MASTER_CLOCK]);
        draw_overlays(*(master_flags.map.panel));
        winboxes[PANEL_MAP].panel.draw_border();
        if (master_flags.clock.draw_flag) {
            debug_log << "ITTERATE: Calling Clock ("<< MOD_CLOCK <<") with panel " << &(winboxes[PANEL_CLOCK].panel) << "\n";
            draw_clock(winboxes[PANEL_CLOCK].panel, Sans);
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
            debug_log << "ITTERATE: Calling Callsign ("<< MOD_CALL <<")with panel " << master_flags.callsign.panel << "\n";
            draw_callsign(*(master_flags.callsign.panel), Sans, clockconfig.CallSign().c_str());
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
            debug_log << "ITTERATE: Calling DE ("<< MOD_DE <<")with panel " << master_flags.de.panel << "\n";
            draw_de_dx(*(master_flags.de.panel), Sans, clockconfig.DE().latitude, clockconfig.DE().longitude, 1);
            master_flags.de.draw_flag = false;
            debug_log << "ITTERATE: Module Timer DE -- " << (SDL_GetTicks() - StartTicks) << " MIlliseconds\n";
            debug_log.flush();
        }
        if (master_flags.dx.draw_flag) {
            debug_log << "ITTERATE: Calling DX ("<< MOD_DX <<")with panel " << master_flags.dx.panel << "\n";
            draw_de_dx(*(master_flags.dx.panel), Sans, clockconfig.DX().latitude, clockconfig.DX().longitude, 0);
            master_flags.dx.draw_flag = false;
            debug_log << "ITTERATE: Module Timer DX -- " << (SDL_GetTicks() - StartTicks) << " MIlliseconds\n";
            debug_log.flush();
        }
        if (master_flags.pota.draw_flag) {
            debug_log << "ITTERATE: Calling POTA ("<< MOD_POTA <<")with panel " << master_flags.pota.panel << "\n";
            pota_spots(*(master_flags.pota.panel), Sans);
            master_flags.pota.draw_flag = false;
            debug_log << "ITTERATE: Module Timer POTA -- " << (SDL_GetTicks() - StartTicks) << " MIlliseconds\n";
            debug_log.flush();
        }
        if (master_flags.lunar.draw_flag) {
            debug_log << "ITTERATE: Calling Lunar ("<< MOD_LUNAR <<")with panel " << master_flags.lunar.panel << "\n";
            debug_log.flush();
            lunar_module(*(master_flags.lunar.panel));
            debug_log << "ITTERATE: Module Timer LUNAR -- " << (SDL_GetTicks() - StartTicks) << " MIlliseconds\n";
            master_flags.lunar.draw_flag = false;

        }
        if (master_flags.kindex.draw_flag) {
            debug_log << "ITTERATE: Calling Kindex ("<< MOD_KINDEX <<")with panel " << master_flags.kindex.panel << "\n";
            debug_log.flush();
            k_index_chart (*(master_flags.kindex.panel));
            debug_log << "ITTERATE: Module Timer Kindex -- " << (SDL_GetTicks() - StartTicks) << " MIlliseconds\n";
            master_flags.kindex.draw_flag = false;
        }
        if (master_flags.contests.draw_flag) {
            debug_log << "ITTERATE: Calling Contests ("<< MOD_CONTESTS <<")with panel " << master_flags.contests.panel << "\n";
            debug_log.flush();
             contest_module (*(master_flags.contests.panel));
            master_flags.contests.draw_flag = false;
            debug_log << "ITTERATE: Module Timer Contests -- " << (SDL_GetTicks() - StartTicks) << " MIlliseconds\n";
        }
        if (master_flags.sat_tracker.draw_flag) {
            debug_log << "ITTERATE: Calling Sat Tracker ("<< MOD_SAT <<")with panel " << master_flags.sat_tracker.panel << "\n";
            debug_log.flush();
            sat_tracker (*(master_flags.sat_tracker.panel), Sans, winboxes[PANEL_MAP].panel);
            master_flags.sat_tracker.draw_flag = false;
            debug_log << "ITTERATE: Module Timer Sat Tracker -- " << (SDL_GetTicks() - StartTicks) << " MIlliseconds\n";
        }
        if (master_flags.dx_spots.draw_flag) {
            debug_log << "ITTERATE: Calling DX Spots ("<< MOD_DXSPOT <<")with panel " << master_flags.dx_spots.panel << "\n";
            debug_log.flush();
            dx_cluster(*(master_flags.dx_spots.panel));
            debug_log << "ITTERATE: Module Timer DX Spots -- " << (SDL_GetTicks() - StartTicks) << " MIlliseconds\n";
            master_flags.dx_spots.draw_flag = false;
        }
        if (master_flags.ncdxf.draw_flag) {
            debug_log << "ITTERATE: Calling NCDXF ("<< MOD_NCDXF <<")with panel " << master_flags.ncdxf.panel << "\n";
            debug_log.flush();
            ncdxf_module(*(master_flags.ncdxf.panel));
            debug_log << "ITTERATE: Module Timer NCDXF -- " << (SDL_GetTicks() - StartTicks) << " MIlliseconds\n";
            master_flags.ncdxf.draw_flag = false;
        }
        if (master_flags.solar.draw_flag) {
            debug_log << "ITTERATE: Calling SDO ("<< MOD_SOLAR <<")with panel " << master_flags.solar.panel << "\n";
            debug_log.flush();
            sdo_image(*(master_flags.solar.panel));
            debug_log << "ITTERATE: Module Timer SDO -- " << (SDL_GetTicks() - StartTicks) << " MIlliseconds\n";
            master_flags.solar.draw_flag = false;
        }
        if (master_flags.wspr.draw_flag) {
            debug_log << "ITTERATE: Calling WSPR Tracker ("<< MOD_WSPR <<")with panel " << master_flags.wspr.panel << "\n";
            debug_log.flush();
            wspr_tracker (*(master_flags.wspr.panel), winboxes[PANEL_MAP].panel);
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
        int foo = 5*5;
        SDL_UnlockMutex(mutexes[MUTEX_MASTER_CLOCK]);
        SDL_RenderPresent(clock_renderer);
        if (headless && (!outfile.empty())) {
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
                        if (pager.sequence.size()) {
                            clock_mouse_event.mod_cords = {modx, mody};
                            clock_mouse_event.mod_count = event->button.clicks;
                            clock_mouse_event.mod_owner = pager.sequence[pager.index];
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
                    dump_cache();
                }
                break;
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

