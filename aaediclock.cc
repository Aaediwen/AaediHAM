#define SDL_MAIN_USE_CALLBACKS
#include <fstream>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL.h>
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
static SDL_Renderer		*surface = nullptr;
TTF_Font		*Sans;
time_t 			currenttime;
ScreenFrame 	DayMap;
ScreenFrame 	NightMap;
ScreenFrame 	CountriesMap;
std::array<pager_node, 12> winboxes;
//struct surfaces 	winboxes;
struct map_pin 		*map_pins;
struct data_blob	*data_cache;
config 		clockconfig;
SDL_Mutex* night_mask_mutex = nullptr;
static SDL_Mutex* master_clock_mutex;
SDL_TimerID map_timer = 0;
Uint8 interrupt_counter = 0;
struct regen_mask_args* night_mask_args = nullptr;
map_overlay overlays;
map_icons icon_bin;
#ifdef CLOCK_DEBUG
std::fstream debug_log;
#else
struct DummyLog {
    template<typename T>
    DummyLog& operator<<(const T&) { return *this; }
    DummyLog& operator<<(std::ostream& (*)(std::ostream&)) { return *this; } // handle std::endl
} debug_log;
#endif

std::string render_engine;

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
} static master_flags;


SDL_TimerID flag_timer = 0;

Uint32 SDLCALL master_clock (void *userdata, SDL_TimerID timerID, Uint32 interval) {
    (void) userdata;
    interrupt_counter++;
    if (interrupt_counter > 200) {
        interrupt_counter = 0;
    }
//    SDL_Log ("In master flag timer");
    if (timerID) {
        SDL_LockMutex(master_clock_mutex);
        if ((interrupt_counter % 1200)==0) {	// 120 seconds
            master_flags.kindex.draw_flag = true;
            master_flags.solar.draw_flag = true;
            master_flags.wspr.draw_flag = true;
        }
        if ((interrupt_counter % 6000) == 0) {	// 60 seconds
//            SDL_Log("MOD PAGER FIRED!");
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

            for (auto& panel : winboxes) {

                if (panel.sequence.size()) {
                    panel.index++;
                    if (panel.index >= panel.sequence.size()) { panel.index = 0 ; }
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
                        case MOD_NCDXF:
                            master_flags.ncdxf.panel = &panel.panel;
                            break;
                        case MOD_SOLAR:
                            master_flags.solar.panel = &panel.panel;
                            break;
                        case MOD_WSPR:
                            master_flags.wspr.panel = &panel.panel;
                            break;

                    }
                }

            }

        }


        if ((interrupt_counter % 300)==0) {	// 30 seconds
            master_flags.callsign.draw_flag = true;
        }
        if ((interrupt_counter % 10)==0) {	// 1 second

            master_flags.map.draw_flag = true;
            master_flags.clock.draw_flag = true;
            master_flags.sat_tracker.draw_flag = true;
        }
        if ((interrupt_counter % 50)==0) {	// 5 seconds
            master_flags.de.draw_flag = true;
            master_flags.dx.draw_flag = true;
            master_flags.pota.draw_flag = true;
            master_flags.ncdxf.draw_flag = true;
        }

        if ((interrupt_counter % 20)==0) {	// 2 seconds
            master_flags.dx_spots.draw_flag = true;
        }
        SDL_UnlockMutex(master_clock_mutex);
        return (interval);
    } else {
        return 0;
    }
}


void resize_panels(std::array<pager_node, 12>& panels) {
        int win_x;
        int win_y;

        // lock and disable the rest of the program
        SDL_LockMutex(night_mask_mutex);
        if (flag_timer) {
            SDL_RemoveTimer(flag_timer);
            flag_timer = 0;
        }
        master_flags.callsign.draw_flag = false;
        master_flags.de.draw_flag 		= false;
        master_flags.dx.draw_flag 		= false;
        master_flags.pota.draw_flag 		= false;
        master_flags.sat_tracker.draw_flag 	= false;
        master_flags.dx_spots.draw_flag 	= false;
        master_flags.map.draw_flag		= false;
        master_flags.ncdxf.draw_flag		= false;
        master_flags.kindex.draw_flag		= false;
        master_flags.clock.draw_flag		= false;
        master_flags.solar.draw_flag		= false;
        master_flags.wspr.draw_flag		= false;

        if (map_timer) {
            SDL_RemoveTimer(map_timer);
            map_timer = 0;
        }
        // clean up the old surface
        if (surface) {
            overlays.clear();
            DayMap.Reset();
            NightMap.Reset();
            CountriesMap.Reset();
            panels[PANEL_CALLSIGN].panel.Reset();
            panels[PANEL_MAP].panel.Reset();
            panels[PANEL_DE].panel.Reset();
            panels[PANEL_DX].panel.Reset();
            panels[PANEL_CLOCK].panel.Reset();
            panels[PANEL_FLEXBOX1].panel.Reset();
            panels[PANEL_FLEXBOX2].panel.Reset();
            panels[PANEL_FLEXBOX3].panel.Reset();
            panels[PANEL_FLEXBOX4].panel.Reset();
            panels[PANEL_NULL].panel.Reset();
            panels[PANEL_FLEXBOX5].panel.Reset();
            SDL_DestroyRenderer(surface);
        }

        // create a new renderer
        if (render_engine.empty()) {
            surface					=	SDL_CreateRenderer(window, NULL);
        } else {
            surface					=	SDL_CreateRenderer(window, render_engine.c_str());
        }
        if (!surface) {
            SDL_Log("Failed to create renderer: %s", SDL_GetError());
            exit(1);
        } else {
//            SDL_Log("created new renderer: %p", (void*)surface);
        }
        SDL_GetCurrentRenderOutputSize(surface, &win_x, &win_y);
        printf ("Resizing Window to %i X %i\n", win_x, win_y);
        SDL_SetRenderDrawColor(surface, 0,0,0,0);
        SDL_RenderClear(surface);
        SDL_RenderPresent(surface);


        SDL_FRect panel_dims;

        //Rebuilding Callsign panel
        panel_dims.x			=	0;
        panel_dims.y			=	0;
        panel_dims.w			=	( win_x / 6.0f ) * 2.0f;
        panel_dims.h			=	  win_y / 8.0f;
        if (!panels[PANEL_CALLSIGN].panel.Create(surface, panel_dims)) {
            printf("Error Creating callsign tex: %s\n", SDL_GetError());
        }

        //Rebuilding Clock panel
        panel_dims.x			=	0.0f;
        panel_dims.y			=	  win_y / 8.0f;
        panel_dims.w			=	( win_x / 6.0f) * 2.0f;
        panel_dims.h			=	  win_y / 8.0f;
        if (!panels[PANEL_CLOCK].panel.Create(surface, panel_dims)) {
            printf("Error Creating Clock: %s\n", SDL_GetError());
        }

        //Rebuilding Map panel
        panel_dims.x			=	  win_x / 6.0f;
        panel_dims.y			=	  win_y / 4.0f;
        panel_dims.w			= 	( win_x / 6.0f) * 5.0f;
        panel_dims.h			=	( win_y / 4.0f) * 3.0f;
        if (!panels[PANEL_MAP].panel.Create(surface, panel_dims)) {
            printf("Error Creating MAP: %s\n", SDL_GetError());
        }

        //Rebuilding DE panel
        panel_dims.x			=	0;
        panel_dims.y			=	win_y / 4.0f;
        panel_dims.w			=	win_x / 6.0f;
        panel_dims.h			=	win_y / 4.0f;
        if (!panels[PANEL_DE].panel.Create(surface, panel_dims)) {
            printf("Error Creating DE: %s\n", SDL_GetError());
        }

        //Rebuilding DX panel");
        panel_dims.x			=	0;
        panel_dims.y			=	win_y / 2.0f;
        panel_dims.w			=	win_x / 6.0f;
        panel_dims.h			=	win_y / 4.0f;
        if (!panels[PANEL_DX].panel.Create(surface, panel_dims)) {
            printf("Error Creating DX tex: %s\n", SDL_GetError());
        }

        //Rebuilding col3 panel");
        panel_dims.x			=	0;
        panel_dims.y			=	(win_y / 4.0f)*3.0f;
        panel_dims.w			=	win_x / 6.0f;
        panel_dims.h			=	win_y / 4.0f;
        if (!panels[PANEL_FLEXBOX5].panel.Create(surface, panel_dims)) {
            printf("Error Creating corner tex: %s\n", SDL_GetError());
        }


        //Rebuilding Rowbox1 panel"
        panel_dims.x			=	( win_x / 6.0f) * 2.0f;
        panel_dims.y			=	0.0f;
        panel_dims.w			=	( win_x / 6.0f);
        panel_dims.h			=	  win_y / 4.0f;
        if (!panels[PANEL_FLEXBOX1].panel.Create(surface, panel_dims)) {
            printf("Error Creating Rowbox1 tex: %s\n", SDL_GetError());
        }

        //Rebuilding Rowbox2 panel
        panel_dims.x			=	( win_x / 6.0f) * 3.0f;
        panel_dims.y			=	0.0f;
        panel_dims.w			=	( win_x / 6.0f);
        panel_dims.h			= 	win_y / 4.0f;
        if (!panels[PANEL_FLEXBOX2].panel.Create(surface, panel_dims)) {
            printf("Error Creating Rowbox2 tex: %s\n", SDL_GetError());
        }

        //Rebuilding Rowbox3 panel
        panel_dims.x			=	( win_x / 6.0f) * 4.0f;
        panel_dims.y			= 	0;
        panel_dims.w			=	( win_x / 6.0f);
        panel_dims.h			=	  win_y / 4.0f;
        if (!panels[PANEL_FLEXBOX3].panel.Create(surface, panel_dims)) {
            printf("Error Creating Rowbox3 tex: %s\n", SDL_GetError());
        }

        //"Rebuilding Rowbox4 panel
        panel_dims.x			=	( win_x / 6.0f) * 5.0f;
        panel_dims.y			=	0.0f;
        panel_dims.w			=	( win_x / 6.0f);
        panel_dims.h			=	  win_y / 4.0f;
        if (!panels[PANEL_FLEXBOX4].panel.Create(surface, panel_dims)) {
            printf("Error Creating Rowbox4 tex: %s\n", SDL_GetError());
        }

        //Rebuilding NULL panel
        panel_dims.x			=	0.0f;
        panel_dims.y			=	0.0f;
        panel_dims.w			=	100.0f;
        panel_dims.h			=	100.0f;
        if (!panels[PANEL_NULL].panel.Create(surface, panel_dims)) {
            printf("Error Creating nullframe tex: %s\n", SDL_GetError());
        }

        // recreate the map textures as well so they don't get lost
        load_maps(surface);
        icon_bin.reload_icons(surface);
        // re-enable the rest of the program
        master_flags.callsign.draw_flag		= true;
        master_flags.de.draw_flag		= true;
        master_flags.dx.draw_flag		= true;
        master_flags.pota.draw_flag		= true;
        master_flags.sat_tracker.draw_flag 	= true;
        master_flags.dx_spots.draw_flag		= true;
        master_flags.ncdxf.draw_flag		= true;
        master_flags.map.draw_flag		= true;
        master_flags.clock.draw_flag		= true;
        master_flags.solar.draw_flag		= true;
        master_flags.wspr.draw_flag 		= true;
        flag_timer = SDL_AddTimer(100, master_clock, &master_flags);
        SDL_UnlockMutex(night_mask_mutex);
        return;
}

bool headless=false;

#ifdef _WIN32
extern "C" int CALLBACK WinFontCallback(const LOGFONT * lpelfe, const TEXTMETRIC * lpntme, DWORD FontType, LPARAM lParam) {
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
    font_criteria.lfHeight = 0;
    font_criteria.lfWidth = 0;
    font_criteria.lfEscapement = 0;
    font_criteria.lfOrientation = 0;
    font_criteria.lfWeight = FW_NORMAL;
    font_criteria.lfItalic = FALSE;
    font_criteria.lfUnderline = FALSE;
    font_criteria.lfStrikeOut = FALSE;
    font_criteria.lfCharSet = DEFAULT_CHARSET;
    font_criteria.lfOutPrecision = OUT_DEFAULT_PRECIS;
    font_criteria.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    font_criteria.lfQuality = DEFAULT_QUALITY;
    font_criteria.lfPitchAndFamily = DEFAULT_PITCH & FF_DONTCARE;
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
            return(1);
        }
        SDL_SetWindowResizable(window, 1);
        resize_panels(winboxes);
        if (!window || !surface) {
            printf("Window Renderer error\n");
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
            return(1);
        }
        TTF_SetFontHinting(Sans, TTF_HINTING_LIGHT_SUBPIXEL);



        SDL_RenderPresent(surface);

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
        flag_timer = SDL_AddTimer(1000, master_clock, &master_flags);
    }
    return 0;
}


int window_destroy() {
    SDL_DestroyWindow(window);
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
    debug_log.open("clock_debug.log", std::fstream::out);
#endif
    debug_log << "------------------------ NEW RUN ------------\n";
    bool fs_start = false;
    outfile.clear();
    render_engine.clear();
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--headless") {
            printf("Running Headless\n");
            debug_log << "Running Headless\n";
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
                     return (SDL_APP_FAILURE);
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
            printf("\t--QRZ_Pass\tSet the password to use for QRZ.com (uses the Callsign for UserName\n");
            printf("\t--help\t\tThis help text\n");
            return (SDL_APP_FAILURE);
        } else if (arg.rfind("--QRZ_Pass",0)==0) {
            std::string password;
            password = arg.substr(11);
            clockconfig.set_qrz_pass(password);
            printf("Done.\n");


            return (SDL_APP_FAILURE);
        }
    }

    if (!(SDL_InitSubSystem(SDL_INIT_VIDEO))) {
        SDL_Log("Unable to initialize SDL: %s", SDL_GetError());
        return (SDL_APP_FAILURE);
    }

    if(!TTF_Init()) {
        printf("TTF_Init Error: %s\n", SDL_GetError());
        return(SDL_APP_FAILURE);
    }

    // init globals
    map_pins			=	0;
    data_cache			=	0;
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
    winboxes[PANEL_FLEXBOX3].sequence.push_back(MOD_DXSPOT);
    winboxes[PANEL_FLEXBOX4].sequence.push_back(MOD_KINDEX);
    winboxes[PANEL_FLEXBOX5].sequence.push_back(MOD_SOLAR);
    winboxes[PANEL_FLEXBOX5].sequence.push_back(MOD_WSPR);



    DayMap.Reset();
    NightMap.Reset();
    CountriesMap.Reset();
    night_mask_mutex = SDL_CreateMutex();
    night_mask_args = (struct regen_mask_args*)malloc(sizeof(struct regen_mask_args));
    map_timer = 0;
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
    if (!resizing) {
        currenttime=time(NULL);
        SDL_LockMutex(master_clock_mutex);
        if (master_flags.clock.draw_flag) {
            draw_clock(winboxes[PANEL_CLOCK].panel, Sans);
            master_flags.clock.draw_flag = false;
        }
        if (master_flags.callsign.draw_flag) {
            draw_callsign(*(master_flags.callsign.panel), Sans, clockconfig.CallSign().c_str());
            master_flags.callsign.draw_flag = false;
        }
        if (master_flags.map.draw_flag) {
            draw_map(*(master_flags.map.panel));
            winboxes[PANEL_MAP].panel.draw_border();
            master_flags.map.draw_flag = false;
        }
        if (master_flags.de.draw_flag) {
            draw_de_dx(*(master_flags.de.panel), Sans, clockconfig.DE().latitude, clockconfig.DE().longitude, 1);
            master_flags.de.draw_flag = false;
        }
        if (master_flags.dx.draw_flag) {
            draw_de_dx(*(master_flags.dx.panel), Sans, clockconfig.DX().latitude, clockconfig.DX().longitude, 0);
            master_flags.dx.draw_flag = false;
        }
        if (master_flags.pota.draw_flag) {
            pota_spots(*(master_flags.pota.panel), Sans);
            lunar_module(*(master_flags.solar.panel));
            master_flags.pota.draw_flag = false;
        }
        if (master_flags.kindex.draw_flag) {
            k_index_chart (*(master_flags.kindex.panel));
            master_flags.kindex.draw_flag = false;
        }
        if (master_flags.sat_tracker.draw_flag) {
            sat_tracker (*(master_flags.sat_tracker.panel), Sans, winboxes[PANEL_MAP].panel);
            master_flags.sat_tracker.draw_flag = false;
        }
        if (master_flags.dx_spots.draw_flag) {
            dx_cluster(*(master_flags.dx_spots.panel));
            master_flags.dx_spots.draw_flag = false;
        }
        if (master_flags.ncdxf.draw_flag) {
            ncdxf_module(*(master_flags.ncdxf.panel));
            master_flags.ncdxf.draw_flag = false;
        }
        if (master_flags.solar.draw_flag) {
            sdo_image(*(master_flags.solar.panel));
            master_flags.solar.draw_flag = false;
        }
        if (master_flags.wspr.draw_flag) {
            wspr_tracker (*(master_flags.wspr.panel), Sans, winboxes[PANEL_MAP].panel);
            master_flags.wspr.draw_flag = false;
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
            SDL_UnlockMutex(master_clock_mutex);
            SDL_RenderPresent(surface);
            if (headless && (!outfile.empty())) {
                // dump surface to image file here
                int width, height;
                SDL_GetCurrentRenderOutputSize(surface, &width, &height);
                SDL_Surface* savesurface = SDL_RenderReadPixels(surface, NULL);
                SDL_SaveBMP(savesurface, outfile.c_str());  // output_file_path from --output
                SDL_DestroySurface(savesurface);
            }
    }
    return(SDL_APP_CONTINUE);
}


    /* This function runs once at shutdown. */
void SDL_AppQuit(void *appstate, SDL_AppResult result) {
     (void)appstate;
    (void)result;
    free (night_mask_args);
    night_mask_args=nullptr;
     SDL_RemoveTimer(map_timer);
        map_timer = 0;
        SDL_DestroyMutex(night_mask_mutex);
    night_mask_mutex = nullptr;
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
            resize_panels(winboxes);
            resizing=0;
        }
    }
    if (event->type==SDL_EVENT_MOUSE_BUTTON_UP) {
        if (event->button.button ==1 && event->button.clicks ==1) {
            SDL_Log ("Got single left click at %f, %f", event->button.x, event->button.y);
            for (auto& pager : winboxes) {
                if ((event->button.x > pager.panel.dims.x) && (event->button.x < (pager.panel.dims.x+pager.panel.dims.w)) &&
                    (event->button.y > pager.panel.dims.y) && (event->button.y < (pager.panel.dims.y+pager.panel.dims.h))) {
                        float modx, mody;
                        modx = event->button.x - pager.panel.dims.x;
                        mody = event->button.y - pager.panel.dims.y;
                        SDL_Log ("Panel event coords: %f, %f", modx, mody);
                        pager.clickpoint={modx, mody};
                        pager.clickcount = event->button.clicks;
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
            case SDLK_Q:
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

