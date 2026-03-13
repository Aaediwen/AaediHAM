#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>
#ifdef _WIN32
#include <cstdlib>
#include <windows.h>
#include <wingdi.h>
#else
#include <fontconfig/fontconfig.h>
#endif
#include <fstream>
#include "aaediclock.h"
#include "core/core.h"
#include "core/panels.h"
#include "plugins/host_api.h"

SDL_Renderer			*clock_renderer		=	nullptr;
SDL_TimerID 			flag_timer;
SDL_Window* 			window 			= 	nullptr;
TTF_Font* 			Sans;

std::array<SDL_Mutex*, 10> 	mutexes;
std::array<pager_node, 12> 	winboxes;
internal_mouse_event 		clock_mouse_event;
struct data_blob *data_cache = 0;               // main data cache
struct map_pin   *map_pins = 0;                 // active map pins
std::vector<struct map_pin>plugin_map_pins;

Sint64 				max_tex_size;
struct regen_mask_args* 	night_mask_args;
SDL_TimerID 			map_timer	=	0;
std::vector<PluginModule> 	loaded_plugins;

#ifdef CLOCK_DEBUG
    static std::ofstream logfile("clock_debug.log");
    std::ostream& debug_log = logfile;
#else
    static nullbuffer nb;
    static std::ostream nullout(&nb);
    std::ostream& debug_log = nullout;
#endif
SDL_Rect 			default_size;
std::string 			outfile;
SDL_ThreadID 			main_thread_id;

namespace AaediClock_Init {
    bool headless = false;		// used at the base of itterate, set during init. probably doesn't need to exist in itterate
    std::string render_engine;

    bool fs_start = false;

    SDL_AppResult cmd_line_parser(int argc, char **argv) {
        // command line parser
        //-----------------------------------------------
        outfile.clear();
        render_engine.clear();
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--headless") {
                std::cout << "Running Headless\n";
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
                            std::cout << "Driver "<< c << ": "<< SDL_GetRenderDriver(c) << "\n";
                         }
                         return (SDL_APP_SUCCESS);
                     }
                     std::cout << "Attempting to use SDL Rendering Engine: " <<  render_engine.c_str() << "\n";
                 } else {
                     std::cout << "Invalid Renderer: "<< arg.c_str() << "\n";
                 }
            } else if (arg.rfind("--geometry",0)==0) {
                 size_t eqpos = arg.find('=');
                 if (eqpos != std::string::npos) {
                     std::string geom = arg.substr(11);
                     std::cout << "Attempting to use default geometry: "<< geom.c_str() << "\n";
                     size_t x_pos = geom.find('x');
                     if (x_pos != std::string::npos) {
                         std::string w = geom.substr(0, x_pos);
                         std::string h = geom.substr(x_pos + 1);
                         try {
                             default_size.w = std::stoi(w);
                             default_size.h = std::stoi(h);
                        } catch (const std::exception& e) {
                            (void)e;
                            std::cout << "Invalid Renderer Geometry: " << geom.c_str()<< "\n";
                            return (SDL_APP_FAILURE);
                        }
                     } else {
                        std::cout << "Invalid Renderer Geometry: " << geom.c_str()<< "\n";
                     }
                 } else {
                     std::cout << "Invalid Renderer Geometry: " << arg.c_str()<< "\n";
                 }
            } else if (arg.rfind("--outfile",0)==0) {
                  outfile = arg.substr(10);
                  std::cout << "Attempting to use output file: "<< outfile.c_str()<< "\n";
            } else if (arg == "--fullscreen") {
                fs_start = true;
            } else if (arg == "--help") {
                std::cout << "Usage: " << argv[0] << " [--headless] [geometry=<width>x<height>] [--output=<outfile>]\n";
                std::cout << "Options:\n";
                std::cout << "\t--headless\tRun in a headless mode with graphical output redirected to a disk file\n";
                std::cout << "\t--fullscreen\tStart in fullscreen mode\n";
                std::cout << "\t--renderer\tset the SDL renderer to use. renderer=help or renderer=list will show a list of avaliable rendering engines\n";
                std::cout << "\t--geometry\tResolution of the output from --headless, or the starting window resolution in a GUI environment\n";
                std::cout << "\t--output\tOutput file path for --headless\n";
                std::cout << "\t--QRZ_Pass\tSet the password to use for QRZ.com (uses the Callsign for UserName)\n";
                std::cout << "\t--help\t\tThis help text\n";
                return (SDL_APP_SUCCESS);
            } else if (arg.rfind("--QRZ_Pass",0)==0) {
                std::string password;
                password = arg.substr(11);
                clockconfig.set_qrz_pass(password);
                std::cout << "QRZ password set.\n";
                return (SDL_APP_SUCCESS);
            }
        } // parser for loop
        return (SDL_APP_CONTINUE);
    }

    SDL_AppResult System_Init() {
        if (!(SDL_InitSubSystem(SDL_INIT_VIDEO))) {
            std::cout << "Unable to initialize SDL: " << SDL_GetError() << "\n";
            debug_log << "INIT: Unable to initialize SDL: " << SDL_GetError() << "\n";
            return (SDL_APP_FAILURE);
        }
        if(!TTF_Init()) {
            std::cout << "TTF_Init Error: " << SDL_GetError() << "\n";
            debug_log << "INIT: TTF Init Error:" << SDL_GetError() << "\n";
            return(SDL_APP_FAILURE);
        }
        main_thread_id = SDL_GetCurrentThreadID();

        return (SDL_APP_CONTINUE);
    }

    SDL_AppResult Global_Init() {
        // init globals
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

        // end panel assignment
        debug_log << "INIT: Globals Initialized\n";
        // create mutexes
        for (SDL_Mutex*& mtx : mutexes) {
            mtx = SDL_CreateMutex();;
        }

        debug_log << "INIT: Map Variables Initialized\n";

        return (SDL_APP_CONTINUE);
    }

    SDL_AppResult Window_Init(int x, int y) {
        if (!window) {
            window = SDL_CreateWindow("Aaediwen Ham Clock", x, y, 0);
            if (!window) {
                std::cout << "Failed to Create Window: " << SDL_GetError() << "\n";
                debug_log << "INIT: Failed to Create Window: " << SDL_GetError() << "\n";
                return (SDL_APP_FAILURE);
            }
            SDL_SetWindowResizable(window, 1);
            resize_panels(winboxes);
            if (!window || !clock_renderer) {
                std::cout << "Window or Render Create Error: " << SDL_GetError() << "\n";
                debug_log << "INIT: Window or Renderer Create Error: " << SDL_GetError() << "\n";
                return (SDL_APP_FAILURE);
            }
            SDL_RenderPresent(clock_renderer);
        }
        return (SDL_APP_CONTINUE);
    }

    #ifdef _WIN32
    // Windows Font search callback
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
        // routine to find a suitable font on the system
        std::string path;
        path.clear();
    #ifndef _WIN32
        // POSIX FontConfig method
        FcInit();
        FcPattern* pat = FcNameParse((const FcChar8*)fontname);     // generate a FontConfig pattern class for "sans"
        FcBool checksubs = FcConfigSubstitute(NULL, pat, FcMatchPattern);   // pattern match the font name
        if (checksubs) {
        FcDefaultSubstitute(pat);                                   // get default options in *pat

        FcResult fcresult;
        FcPattern *font =  FcFontMatch(NULL, pat, &fcresult);       // find closest font match for 'sans'

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

       // Windows Registry search
        struct ::LOGFONTA font_criteria;
        font_criteria.lfHeight              =       0;
        font_criteria.lfWidth               =       0;
        font_criteria.lfEscapement          =       0;
        font_criteria.lfOrientation         =       0;
        font_criteria.lfWeight              =       FW_NORMAL;
        font_criteria.lfItalic              =       FALSE;
        font_criteria.lfUnderline           =       FALSE;
        font_criteria.lfStrikeOut           =       FALSE;
        font_criteria.lfCharSet             =       DEFAULT_CHARSET;
        font_criteria.lfOutPrecision        =       OUT_DEFAULT_PRECIS;
        font_criteria.lfClipPrecision       =       CLIP_DEFAULT_PRECIS;
        font_criteria.lfQuality             =       DEFAULT_QUALITY;
        font_criteria.lfPitchAndFamily      =       DEFAULT_PITCH & FF_DONTCARE;
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
        for (DWORD i = 0; ; ++i) {                              // hunt through the registry for the indicated font
            valueNameSize = sizeof(valueName);
            valueDataSize = sizeof(valueData);
            result = RegEnumValueA( hKey, i, valueName, &valueNameSize, nullptr, &valueType, valueData, &valueDataSize ); // read the ne

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


    SDL_AppResult Font_Init() {
    #ifndef _WIN32
        Sans = TTF_OpenFont(FindFont("sans").c_str(), 72);
    #else
        Sans = TTF_OpenFont(FindFont("Arial").c_str(), 72);
    #endif
        if (!Sans) {
            std::cout << "Error opening font: " << SDL_GetError() << "\n";
            debug_log << "INIT: Error opening font: " << SDL_GetError() << "\n";
            return (SDL_APP_FAILURE);
        }
        TTF_SetFontHinting(Sans, TTF_HINTING_LIGHT_SUBPIXEL);
        return (SDL_APP_CONTINUE);
    }

    SDL_AppResult Init_System_Timer() {
        for (struct PluginModule& plugin : loaded_plugins ) {
            plugin.draw_flag = true;
        }

        if (!flag_timer) {
            flag_timer = SDL_AddTimer(100, master_clock, nullptr);
        }
        return (SDL_APP_CONTINUE);
    }

    SDL_AppResult Plugin_Loader() {
        struct plugin_entry {
            std::string filename;
            size_t position;
            uint16_t interval;
        };
        std::vector<struct plugin_entry>plugin_list;

//      plugin_list.push_back({"plugins/librss_plugin.so",2,1});
//      plugin_list.push_back({"plugins/libpsk_plugin.so",2,50});
        config::plugin plugin_load;
        plugin_load.filename="null";
        while (!plugin_load.filename.empty()) {
            plugin_load = clockconfig.next_plugin();
            if (!plugin_load.filename.empty()) {
                plugin_list.push_back({plugin_load.filename, plugin_load.panel_id, plugin_load.interval});
            }
        }
        for (auto& plugin: plugin_list) {
            register_module(plugin.filename);
            loaded_plugins.back().position = static_cast<uint16_t>(plugin.position);
            loaded_plugins.back().interval = plugin.interval;
            winboxes[plugin.position].plugin_sequence.push_back(loaded_plugins.back().id);
        }

        return(SDL_APP_CONTINUE);
    }
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv) {
    (void)appstate;
    default_size.h	=	480;
    default_size.w	= 	800;

    // debug init
#ifdef CLOCK_DEBUG
#ifdef _WIN32
#ifdef _DEBUG
    SDL_Delay(10000);
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
#endif

    debug_log << "------------------------ NEW RUN ------------\n";
    SDL_AppResult init_result;
    init_result = AaediClock_Init::cmd_line_parser(argc, argv);
    if (init_result != SDL_APP_CONTINUE) {
        return init_result;
    }
    init_result = AaediClock_Init::System_Init();
    if (init_result != SDL_APP_CONTINUE) {
        return init_result;
    }
    init_result = AaediClock_Init::Global_Init();
    if (init_result != SDL_APP_CONTINUE) {
        return init_result;
    }
    init_result = AaediClock_Init::Window_Init(default_size.w, default_size.h);
    if (init_result != SDL_APP_CONTINUE) {
        return init_result;
    }
    if (AaediClock_Init::fs_start) {
        SDL_SetWindowFullscreen(window, 1);
    }
    init_result = AaediClock_Init::Font_Init();
    if (init_result != SDL_APP_CONTINUE) {
        return init_result;
    }
    init_result = AaediClock_Init::Init_System_Timer();
    if (init_result != SDL_APP_CONTINUE) {
        return init_result;
    }
    init_result = AaediClock_Init::Plugin_Loader();
    if (init_result != SDL_APP_CONTINUE) {
        return init_result;
    }
    return(SDL_APP_CONTINUE);
}

