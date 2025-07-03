#define SDL_MAIN_USE_CALLBACKS
#include <fstream>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL.h>
#include "aaediclock.h"
#include "utils.h"
#include "modules.h"

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
struct config 		clockconfig;


void resize_panels(struct surfaces* panels) {
        int win_x;
        int win_y;

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
        panel_dims.w			=	( win_x / 6 ) * 2;
        panel_dims.h			=	  win_y / 8;
        if (!panels->callsign.Create(surface, panel_dims)) {
            printf("Error Creating callsign tex: %s\n", SDL_GetError());
        }

        //Rebuilding Clock panel
        SDL_Log("Creating Clock Panel");
        panel_dims.x			=	0;
        panel_dims.y			=	  win_y / 8;
        panel_dims.w			=	( win_x / 6 ) * 2;
        panel_dims.h			=	  win_y / 8;
        if (!panels->clock.Create(surface, panel_dims)) {
            printf("Error Creating Clock: %s\n", SDL_GetError());
        }

        //Rebuilding Map panel
        panel_dims.x			=	  win_x / 6;
        panel_dims.y			=	  win_y / 4;
        panel_dims.w			= 	( win_x / 6 ) * 5;
        panel_dims.h			=	( win_y / 4 ) * 3;
        if (!panels->map.Create(surface, panel_dims)) {
            printf("Error Creating MAP: %s\n", SDL_GetError());
        }

        //Rebuilding DE panel
        panel_dims.x			=	0;
        panel_dims.y			=	win_y / 4;
        panel_dims.w			=	win_x / 6;
        panel_dims.h			=	win_y / 4;
        if (!panels->de.Create(surface, panel_dims)) {
            printf("Error Creating DE: %s\n", SDL_GetError());
        }

        //Rebuilding DX panel");
        panel_dims.x			=	0;
        panel_dims.y			=	win_y / 2;
        panel_dims.w			=	win_x / 6;
        panel_dims.h			=	win_y / 4;
        if (!panels->dx.Create(surface, panel_dims)) {
            printf("Error Creating DX tex: %s\n", SDL_GetError());
        }

        //Rebuilding Rowbox1 panel"
        panel_dims.x			=	( win_x / 6 ) * 2;
        panel_dims.y			=	0;
        panel_dims.w			=	( win_x / 6 );
        panel_dims.h			=	  win_y / 4;
        if (!panels->rowbox1.Create(surface, panel_dims)) {
            printf("Error Creating Rowbox1 tex: %s\n", SDL_GetError());
        }

        //Rebuilding Rowbox2 panel
        panel_dims.x			=	( win_x / 6 ) * 3;
        panel_dims.y			=	0;
        panel_dims.w			=	( win_x / 6 );
        panel_dims.h			= 	win_y / 4;
        if (!panels->rowbox2.Create(surface, panel_dims)) {
            printf("Error Creating Rowbox2 tex: %s\n", SDL_GetError());
        }

        //Rebuilding Rowbox3 panel
        panel_dims.x			=	( win_x / 6 ) * 4;
        panel_dims.y			= 	0;
        panel_dims.w			=	( win_x / 6 );
        panel_dims.h			=	  win_y / 4;
        if (!panels->rowbox3.Create(surface, panel_dims)) {
            printf("Error Creating Rowbox3 tex: %s\n", SDL_GetError());
        }

        //"Rebuilding Rowbox4 panel
        panel_dims.x			=	( win_x / 6 ) * 5;
        panel_dims.y			=	0;
        panel_dims.w			=	( win_x / 6 );
        panel_dims.h			=	  win_y / 4;
        if (!panels->rowbox4.Create(surface, panel_dims)) {
            printf("Error Creating Rowbox4 tex: %s\n", SDL_GetError());
        }

        // recreate the map textures as well so they don't get lost
        load_maps();
        SDL_Log("Done resizing panels");
        return;
}

void load_config() {

    bool goodread = false;
    json data;

    clockconfig = {
        "N0CALL",                      // CallSign
        {},     // sats
        { 0, 0 },        // DE (lat/lon)
        { 0, 0 }           // DX (lat/lon)
    };

    SDL_Log ("Loading CONFIG");
    std::ifstream f("aaediclock_config.json");
    if (f.good()) {
        SDL_Log("CONFIG file opened.");
        try {
//            data = json::parse(f);
            f >> data;
            SDL_Log("CONFIG file parsed.");
            goodread=true;
        } catch (const json::parse_error &e) {
            SDL_Log ("JSON parse error: %s",  e.what());
            goodread=false;
        }
    } else {
        SDL_Log ("File Read error: ");
        goodread=false;
    }

    if (goodread) {
        SDL_Log ("Reading CONFIG");
         if (data.contains("DE")) {
             if (data["DE"].contains("Latitude") && data["DE"].contains("Longitude")) {
                 if (data["DE"]["Latitude"].is_number() && data["DE"]["Longitude"].is_number() ) {
                     clockconfig.DE.lat=data["DE"]["Latitude"];
                     clockconfig.DE.lon=data["DE"]["Longitude"];
                 }
             }
         }
         if (data.contains("DX")) {
             if (data["DX"].contains("Latitude") && data["DX"].contains("Longitude")) {
                 if (data["DX"]["Latitude"].is_number() && data["DX"]["Longitude"].is_number() ) {
                     clockconfig.DX.lat=data["DX"]["Latitude"];
                     clockconfig.DX.lon=data["DX"]["Longitude"];
                 }
             }
         }
         if (data.contains("CallSign")) {
             if (data["CallSign"].is_string()) {
                 clockconfig.CallSign=data["CallSign"];
             }
         }
         if (data.contains("SatList")) {
             if (data["SatList"].is_array()) {
                 for (const auto& item : data["SatList"]) {
                     if (item.is_string()) {
                         clockconfig.sats.push_back(item);
                     }
                 }
             }
         }
    } else {
        SDL_Log ("ERROR Reading CONFIG");
    }
}



int window_init() {
    if (!window) {
        // create the main window
        window = SDL_CreateWindow("Aaediwen Ham Clock", 800, 480, 0);
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
//        winboxes.callsign.draw_border(surface);
        draw_callsign(winboxes.callsign, Sans, clockconfig.CallSign.c_str());

//        winboxes.clock.draw_border(surface);
//        draw_clock(winboxes.clock, Sans);

//        winboxes.map.draw_border(surface);
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

SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv) {
    // initialize required subsystems
    if (!(SDL_InitSubSystem(SDL_INIT_VIDEO))) {
        SDL_Log("Unable to initialize SDL: %s", SDL_GetError());
        return (SDL_APP_FAILURE);
    }

    if(TTF_Init()==-1) {
        printf("TTF_Init: %s\n", SDL_GetError());
        return(SDL_APP_FAILURE);
    }
    load_config();
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

    // create the main window
    if (window_init()) {
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
    SDL_Delay(100);			// slow down the program
    if (!resizing) {
        currenttime=time(NULL);
        if (currenttime != oldtime) {	// temporary one second timer
        draw_callsign(winboxes.callsign, Sans, clockconfig.CallSign.c_str());
            draw_clock(winboxes.clock, Sans);
            draw_map(winboxes.map);
//            winboxes.map.draw_border(surface);
            draw_de_dx(winboxes.de, Sans, clockconfig.DE.lat, clockconfig.DE.lon, 1);
            draw_de_dx(winboxes.dx, Sans, clockconfig.DX.lat, clockconfig.DX.lon, 0);


            winboxes.de.draw_border(surface);

            winboxes.dx.draw_border(surface);



            pota_spots(winboxes.rowbox1, Sans);
            winboxes.rowbox1.draw_border(surface);


            sat_tracker (winboxes.rowbox2, Sans, winboxes.map);
            winboxes.rowbox2.draw_border(surface);

            winboxes.rowbox3.draw_border(surface);

            winboxes.rowbox4.draw_border(surface);
            SDL_RenderPresent(surface);
            oldtime = currenttime;
        }
    }
    return(SDL_APP_CONTINUE);
}


    /* This function runs once at shutdown. */
void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    /* SDL will clean up the window/renderer for us. */
}

    // SDL Event Handler
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
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
            SDL_Log("drawing call");
            draw_callsign(winboxes.callsign, Sans, clockconfig.CallSign.c_str());
//            SDL_Delay(1000);
            resizing=0;
        }
    }
    return SDL_APP_CONTINUE;
}

