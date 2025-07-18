#define SDL_MAIN_USE_CALLBACKS
#include <fstream>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL.h>
#include "aaediclock.h"
#include "utils.h"
#include "modules.h"
#ifdef _WIN32
#include <cstdlib>
#endif
SDL_Window		*window;
SDL_Renderer		*surface;
TTF_Font		*Sans;
time_t 			currenttime;
ScreenFrame 	DayMap;
ScreenFrame 	NightMap;
ScreenFrame 	CountriesMap;
struct surfaces 	winboxes;
struct map_pin 		*map_pins;
struct data_blob	*data_cache;
config 		clockconfig;
SDL_Mutex* night_mask_mutex = nullptr;
SDL_TimerID map_timer = 0;
struct regen_mask_args* night_mask_args = nullptr;


void resize_panels(struct surfaces* panels) {
        int win_x;
        int win_y;
        SDL_LockMutex(night_mask_mutex);
        if (map_timer) {
            SDL_RemoveTimer(map_timer);
            map_timer = 0;
        }
        // clean up the old surface
        if (surface) {
            DayMap.Reset();
            NightMap.Reset();
            CountriesMap.Reset();
            panels->callsign.Reset();
            panels->map.Reset();
            panels->de.Reset();
            panels->dx.Reset();
            panels->clock.Reset();
            panels->rowbox1.Reset();
            panels->rowbox2.Reset();
            panels->rowbox3.Reset();
            panels->rowbox4.Reset();
            SDL_DestroyRenderer(surface);
        }

        // create a new renderer
        surface					=	SDL_CreateRenderer(window, NULL);
        if (!surface) {
            SDL_Log("Failed to create renderer: %s", SDL_GetError());
            exit(1);
        } else {
            SDL_Log("created new renderer: %p", (void*)surface);
        }
        SDL_GetCurrentRenderOutputSize(surface, &win_x, &win_y);
        printf ("Resizing Window to %i X %i\n", win_x, win_y);
        SDL_SetRenderDrawColor(surface, 0,0,0,0);
        SDL_RenderClear(surface);
        SDL_RenderPresent(surface);


        SDL_FRect panel_dims;

        //Rebuilding Callsign panel
        SDL_Log("Creating Call Panel");
        panel_dims.x			=	0;
        panel_dims.y			=	0;
        panel_dims.w			=	( win_x / 6.0f ) * 2.0f;
        panel_dims.h			=	  win_y / 8.0f;
        if (!panels->callsign.Create(surface, panel_dims)) {
            printf("Error Creating callsign tex: %s\n", SDL_GetError());
        }

        //Rebuilding Clock panel
        SDL_Log("Creating Clock Panel");
        panel_dims.x			=	0.0f;
        panel_dims.y			=	  win_y / 8.0f;
        panel_dims.w			=	( win_x / 6.0f) * 2.0f;
        panel_dims.h			=	  win_y / 8.0f;
        if (!panels->clock.Create(surface, panel_dims)) {
            printf("Error Creating Clock: %s\n", SDL_GetError());
        }

        //Rebuilding Map panel
        SDL_Log("Creating Map Panel");
        panel_dims.x			=	  win_x / 6.0f;
        panel_dims.y			=	  win_y / 4.0f;
        panel_dims.w			= 	( win_x / 6.0f) * 5.0f;
        panel_dims.h			=	( win_y / 4.0f) * 3.0f;
        if (!panels->map.Create(surface, panel_dims)) {
            printf("Error Creating MAP: %s\n", SDL_GetError());
        }

        //Rebuilding DE panel
        SDL_Log("Creating DE Panel");
        panel_dims.x			=	0;
        panel_dims.y			=	win_y / 4.0f;
        panel_dims.w			=	win_x / 6.0f;
        panel_dims.h			=	win_y / 4.0f;
        if (!panels->de.Create(surface, panel_dims)) {
            printf("Error Creating DE: %s\n", SDL_GetError());
        }

        //Rebuilding DX panel");
        SDL_Log("Creating DX Panel");
        panel_dims.x			=	0;
        panel_dims.y			=	win_y / 2.0f;
        panel_dims.w			=	win_x / 6.0f;
        panel_dims.h			=	win_y / 4.0f;
        if (!panels->dx.Create(surface, panel_dims)) {
            printf("Error Creating DX tex: %s\n", SDL_GetError());
        }

        //Rebuilding Rowbox1 panel"
        panel_dims.x			=	( win_x / 6.0f) * 2.0f;
        panel_dims.y			=	0.0f;
        panel_dims.w			=	( win_x / 6.0f);
        panel_dims.h			=	  win_y / 4.0f;
        if (!panels->rowbox1.Create(surface, panel_dims)) {
            printf("Error Creating Rowbox1 tex: %s\n", SDL_GetError());
        }

        //Rebuilding Rowbox2 panel
        panel_dims.x			=	( win_x / 6.0f) * 3.0f;
        panel_dims.y			=	0.0f;
        panel_dims.w			=	( win_x / 6.0f);
        panel_dims.h			= 	win_y / 4.0f;
        if (!panels->rowbox2.Create(surface, panel_dims)) {
            printf("Error Creating Rowbox2 tex: %s\n", SDL_GetError());
        }

        //Rebuilding Rowbox3 panel
        panel_dims.x			=	( win_x / 6.0f) * 4.0f;
        panel_dims.y			= 	0;
        panel_dims.w			=	( win_x / 6.0f);
        panel_dims.h			=	  win_y / 4.0f;
        if (!panels->rowbox3.Create(surface, panel_dims)) {
            printf("Error Creating Rowbox3 tex: %s\n", SDL_GetError());
        }

        //"Rebuilding Rowbox4 panel
        panel_dims.x			=	( win_x / 6.0f) * 5.0f;
        panel_dims.y			=	0.0f;
        panel_dims.w			=	( win_x / 6.0f);
        panel_dims.h			=	  win_y / 4.0f;
        if (!panels->rowbox4.Create(surface, panel_dims)) {
            printf("Error Creating Rowbox4 tex: %s\n", SDL_GetError());
        }

        // recreate the map textures as well so they don't get lost
        load_maps();

        SDL_UnlockMutex(night_mask_mutex);
        SDL_Log("Done resizing panels");
        return;
}

bool headless=false;
int window_init(int x, int y) {
    if (!window) {
        // create the main window
        window = SDL_CreateWindow("Aaediwen Ham Clock", x, y, 0);
        if (!window) {
            SDL_Log("Failed to create window: %s", SDL_GetError());
            return(1);
        }
        SDL_SetWindowResizable(window, 1);
        resize_panels(&winboxes);
        if (!window || !surface) {
            printf("Window Renderer error\n");
            return(1);
        }
        // load assets
        Sans = TTF_OpenFont("arial.ttf", 72);
        if (!Sans) {
            printf("Error opening font: %s\n", SDL_GetError());
            return(1);
        }
        TTF_SetFontHinting(Sans, TTF_HINTING_LIGHT_SUBPIXEL);
        load_maps();

        // initial draws
        currenttime=time(NULL);
//        draw_callsign(winboxes.callsign, Sans, clockconfig.CallSign.c_str());
          draw_callsign(winboxes.callsign, Sans, clockconfig.CallSign().c_str());
        dx_cluster (winboxes.rowbox3);
//        winboxes.clock.draw_border(surface);
//        draw_clock(winboxes.clock, Sans);

//        draw_map(winboxes.map);
/*
        winboxes.de.draw_border(surface);

        winboxes.dx.draw_border(surface);

        winboxes.rowbox1.draw_border(surface);

        winboxes.rowbox2.draw_border(surface);

        winboxes.rowbox3.draw_border(surface);

        winboxes.rowbox4.draw_border(surface);
*/
        SDL_RenderPresent(surface);

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

    outfile.clear();
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--headless") {
            printf("Running Headless\n");
#ifdef _WIN32
            _putenv_s("SDL_VIDEO_DRIVER", "dummy");
#else
            setenv("SDL_VIDEO_DRIVER", "dummy", 1);
#endif
            headless=true;
        } else if (arg.rfind("--geometry",0)==0) {
             std::string geom = arg.substr(11); // skip "--geometry="
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
        } else if (arg.rfind("--outfile",0)==0) {
            outfile = arg.substr(10); // skip "--geometry="
            printf("Attempting to use output file: %s", outfile.c_str());
        } else if (arg == "--help") {
            printf("Usage: %s [--headless] [geometry=<width>x<height>] [--output=<outfile>]\n", argv[0]);
            printf("Options:\n");
            printf("\t--headless\tRun in a headless mode with graphical output redirected to a disk file\n");
            printf("\t--geometry\tResolution of the output from --headless, or the starting window resolution in a GUI environment\n");
            printf("\t--output\tOutput file path for --headless\n");
            printf("\t--QRZ_Pass\tSet the password to use for QRZ.com (uses the Callsign for UserName\n");
            printf("\t--help\t\tThis help text\n");
            return (SDL_APP_FAILURE);
        } else if (arg.rfind("--QRZ_Pass",0)==0) {
            std::string password;
            password = arg.substr(11);
            printf("Attempting to set QRZ pass: %s... ", password.c_str());
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
        printf("TTF_Init: %s\n", SDL_GetError());
        return(SDL_APP_FAILURE);
    }

//    load_config();
    // init globals
    map_pins			=	0;
    data_cache			=	0;
    winboxes.callsign.Reset();
    winboxes.map.Reset();
    winboxes.dx.Reset();
    winboxes.de.Reset();
    winboxes.clock.Reset();
    winboxes.rowbox1.Reset();
    winboxes.rowbox2.Reset();
    winboxes.rowbox3.Reset();
    winboxes.rowbox4.Reset();
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
    SDL_Log("WINDOW INIT DONE");
//    SDL_Delay(1000);
    return(SDL_APP_CONTINUE);

}


    // SDL Loop
time_t oldtime;
int resizing 			= 	0;
SDL_AppResult SDL_AppIterate(void *appstate) {
    (void)appstate;
    SDL_Delay(100);			// slow down the program
    if (!resizing) {
        currenttime=time(NULL);
        if (currenttime != oldtime) {	// temporary one second timer
        draw_callsign(winboxes.callsign, Sans, clockconfig.CallSign().c_str());
            draw_clock(winboxes.clock, Sans);
            draw_map(winboxes.map);
//            winboxes.map.draw_border(surface);
            draw_de_dx(winboxes.de, Sans, clockconfig.DE().latitude, clockconfig.DE().longitude, 1);
            draw_de_dx(winboxes.dx, Sans, clockconfig.DX().latitude, clockconfig.DX().longitude, 0);


            winboxes.de.draw_border();

            winboxes.dx.draw_border();



            pota_spots(winboxes.rowbox1, Sans);
            winboxes.rowbox1.draw_border();


            sat_tracker (winboxes.rowbox2, Sans, winboxes.map);
            winboxes.rowbox2.draw_border();

            winboxes.rowbox3.draw_border();

            winboxes.rowbox4.draw_border();
            dx_cluster (winboxes.rowbox3);
            SDL_RenderPresent(surface);
            if (headless && (!outfile.empty())) {
                // dump surface to image file here
                int width, height;
                SDL_GetCurrentRenderOutputSize(surface, &width, &height);
//                SDL_Surface* saveSurface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_ARGB8888);
                SDL_Surface* savesurface = SDL_RenderReadPixels(surface, NULL);
//                SDL_SaveBMP(saveSurface, output_file_path.c_str());  // output_file_path from --output
                SDL_SaveBMP(savesurface, outfile.c_str());  // output_file_path from --output
                SDL_DestroySurface(savesurface);
            }
            oldtime = currenttime;
        }
    }
    return(SDL_APP_CONTINUE);
}


    /* This function runs once at shutdown. */
void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
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
            SDL_Log("RESIZING");
            resize_panels(&winboxes);
//            SDL_Delay(1000);
            SDL_Log("drawing call on resize");
            draw_callsign(winboxes.callsign, Sans, clockconfig.CallSign().c_str());
            SDL_Log("Completed drawing call on resize");
//            SDL_Delay(1000);
            resizing=0;
        }
    }
    return SDL_APP_CONTINUE;
}

