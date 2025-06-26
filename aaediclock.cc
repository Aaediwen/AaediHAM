#define SDL_MAIN_USE_CALLBACKS

#include <SDL3/SDL_main.h>
#include <SDL3/SDL.h>
#include "aaediclock.h"
#include "utils.h"
#include "modules.h"

SDL_Window		*window;
SDL_Renderer		*surface;
TTF_Font		*Sans;
time_t 			currenttime;
struct ScreenFrame 	DayMap;
struct ScreenFrame 	NightMap;
struct ScreenFrame 	CountriesMap;
struct surfaces 	winboxes;
struct map_pin 		*map_pins;
struct data_blob	*data_cache;

void resize_panels(struct surfaces* panels) {
        int win_x;
        int win_y;

        // clean up the old surface
        if (surface) {
            if (DayMap.texture) {
                SDL_DestroyTexture(DayMap.texture);
                DayMap.texture	=	0;
            }
            if (NightMap.texture) {
                SDL_DestroyTexture(NightMap.texture);
                NightMap.texture	=	0;
            }
            if (CountriesMap.texture) {
                SDL_DestroyTexture(CountriesMap.texture);
                CountriesMap.texture	=	0;
            }
            if (panels->callsign.texture) {
                SDL_DestroyTexture(panels->callsign.texture);
                panels->callsign.texture	=	0;
            }
            if (panels->clock.texture) {
                SDL_DestroyTexture(panels->clock.texture);
                panels->clock.texture		=	0;
            }
            if (panels->map.texture) {
                SDL_DestroyTexture(panels->map.texture);
                panels->map.texture		=	0;
            }
            if (panels->de.texture) {
                SDL_DestroyTexture(panels->de.texture);
                panels->de.texture		=	0;
            }
            if (panels->dx.texture) {
                SDL_DestroyTexture(panels->dx.texture);
                panels->dx.texture		=	0;
            }
            if (panels->rowbox1.texture) {
                SDL_DestroyTexture(panels->rowbox1.texture);
                panels->rowbox1.texture		=	0;
            }
            if (panels->rowbox2.texture) {
                SDL_DestroyTexture(panels->rowbox2.texture);
                panels->rowbox2.texture		=	0;
            }

            if (panels->rowbox3.texture) {
                SDL_DestroyTexture(panels->rowbox3.texture);
                panels->rowbox3.texture		=	0;
            }
            if (panels->rowbox4.texture) {
                SDL_DestroyTexture(panels->rowbox4.texture);
                panels->rowbox4.texture		=	0;
            }
            SDL_DestroyRenderer(surface);
        }

        // create a new renderer
        surface					=	SDL_CreateRenderer(window, NULL);
        if (!surface) {
            SDL_Log("Failed to create renderer: %s", SDL_GetError());
            exit(1);
        } else {
            SDL_Log("created new renderer: %s", SDL_GetError());
        }
        SDL_GetCurrentRenderOutputSize(surface, &win_x, &win_y);
        printf ("Resizing Window to %i X %i\n", win_x, win_y);
        SDL_SetRenderDrawColor(surface, 0,0,0,0);
        SDL_RenderClear(surface);
        SDL_RenderPresent(surface);


        //Rebuilding Callsign panel
        panels->callsign.dims.x			=	0;
        panels->callsign.dims.y			=	0;
        panels->callsign.dims.w			=	( win_x / 6 ) * 2;
        panels->callsign.dims.h			=	  win_y / 8;

        panels->callsign.texture		=	SDL_CreateTexture( surface, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET,
                                                                           panels->callsign.dims.w, panels->callsign.dims.h );
        if (!panels->callsign.texture) {
            printf("Error Creating callsign tex: %s\n", SDL_GetError());
        }

        //Rebuilding Clock panel
        panels->clock.dims.x			=	0;
        panels->clock.dims.y			=	  win_y / 8;
        panels->clock.dims.w			=	( win_x / 6 ) * 2;
        panels->clock.dims.h			=	  win_y / 8;

        panels->clock.texture			=	SDL_CreateTexture( surface, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET,
                                                                           panels->clock.dims.w, panels->clock.dims.h );
        if (!panels->clock.texture) {
            printf("Error Creating Clock: %s\n", SDL_GetError());
        }

        //Rebuilding Map panel
        panels->map.dims.x			=	  win_x / 6;
        panels->map.dims.y			=	  win_y / 4;
        panels->map.dims.w			= 	( win_x / 6 ) * 5;
        panels->map.dims.h			=	( win_y / 4 ) * 3;

        panels->map.texture			=	SDL_CreateTexture( surface, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET,
                                                                           panels->map.dims.w, panels->map.dims.h);
        if (!panels->map.texture) {
            printf("Error Creating MAP: %s\n", SDL_GetError());
        }

        //Rebuilding DE panel
        panels->de.dims.x			=	0;
        panels->de.dims.y			=	win_y / 4;
        panels->de.dims.w			=	win_x / 6;
        panels->de.dims.h			=	win_y / 4;

        panels->de.texture			=	SDL_CreateTexture( surface, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET,
                                                                           panels->de.dims.w, panels->de.dims.h);
        if (!panels->de.texture) {
            printf("Error Creating DE: %s\n", SDL_GetError());
        }

        //Rebuilding DX panel");
        panels->dx.dims.x			=	0;
        panels->dx.dims.y			=	win_y / 2;
        panels->dx.dims.w			=	win_x / 6;
        panels->dx.dims.h			=	win_y / 4;
        panels->dx.texture			=	SDL_CreateTexture( surface, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET,
                                                                           panels->dx.dims.w, panels->dx.dims.h);
        if (!panels->dx.texture) {
            printf("Error Creating DX: %s\n", SDL_GetError());
        }
        //Rebuilding Rowbox1 panel"
        panels->rowbox1.dims.x			=	( win_x / 6 ) * 2;
        panels->rowbox1.dims.y			=	0;
        panels->rowbox1.dims.w			=	( win_x / 6 );
        panels->rowbox1.dims.h			=	  win_y / 4;
        panels->rowbox1.texture			=	SDL_CreateTexture( surface, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET,
                                                                           panels->rowbox1.dims.w, panels->rowbox1.dims.h);
        if (!panels->rowbox1.texture) {
            printf("Error Creating RowBox1: %s\n", SDL_GetError());
        }

        //Rebuilding Rowbox2 panel
        panels->rowbox2.dims.x			=	( win_x / 6 ) * 3;
        panels->rowbox2.dims.y			=	0;
        panels->rowbox2.dims.w			=	( win_x / 6 );
        panels->rowbox2.dims.h			= 	win_y / 4;
        panels->rowbox2.texture			=	SDL_CreateTexture( surface, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET,
                                                                           panels->rowbox2.dims.w, panels->rowbox2.dims.h);
        if (!panels->rowbox2.texture) {
            printf("Error Creating RowBox2: %s\n", SDL_GetError());
        }

        //Rebuilding Rowbox3 panel
        panels->rowbox3.dims.x			=	( win_x / 6 ) * 4;
        panels->rowbox3.dims.y			= 	0;
        panels->rowbox3.dims.w			=	( win_x / 6 );
        panels->rowbox3.dims.h			=	  win_y / 4;
        panels->rowbox3.texture			=	SDL_CreateTexture( surface, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET,
                                                                           panels->rowbox3.dims.w, panels->rowbox3.dims.h);
        if (!panels->rowbox3.texture) {
            printf("Error Creating RowBox3: %s\n", SDL_GetError());
        }

        //"Rebuilding Rowbox4 panel
        panels->rowbox4.dims.x			=	( win_x / 6 ) * 5;
        panels->rowbox4.dims.y			=	0;
        panels->rowbox4.dims.w			=	( win_x / 6 );
        panels->rowbox4.dims.h			=	  win_y / 4;
        panels->rowbox4.texture			=	SDL_CreateTexture( surface, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET,
                                                                           panels->rowbox4.dims.w, panels->rowbox4.dims.h);
        if (!panels->rowbox4.texture) {
            printf("Error Creating RowBox4: %s\n", SDL_GetError());
        }
        // recreate the map textures as well so they don't get lost
        load_maps();
        SDL_Log("Done resizing panels");
        return;
}

int window_init() {
    if (!window) {
        // create the main window
        window = SDL_CreateWindow("Aaediwen Ham Clock", 800, 480, 0);
        SDL_SetWindowResizable(window, 1);
        resize_panels(&winboxes);
        if (!window || !surface) {
            printf("Window creation error\n");
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
        draw_panel_border(winboxes.callsign);
        draw_callsign(winboxes.callsign, Sans, "KQ4SIZ");

        draw_panel_border(winboxes.clock);
        draw_clock(winboxes.clock, Sans);

        draw_panel_border(winboxes.map);
        draw_map(winboxes.map);

        draw_panel_border(winboxes.de);

        draw_panel_border(winboxes.dx);

        draw_panel_border(winboxes.rowbox1);

        draw_panel_border(winboxes.rowbox2);

        draw_panel_border(winboxes.rowbox3);

        draw_panel_border(winboxes.rowbox4);

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

    // init globals
    map_pins			=	0;
    data_cache			=	0;
    winboxes.callsign.texture	=	0;
    winboxes.map.texture	=	0;
    winboxes.dx.texture		=	0;
    winboxes.de.texture		=	0;
    winboxes.clock.texture	=	0;
    winboxes.rowbox1.texture	=	0;
    winboxes.rowbox2.texture	=	0;
    winboxes.rowbox3.texture	=	0;
    winboxes.rowbox4.texture	=	0;
    DayMap.texture		=	0;
    NightMap.texture		=	0;
    CountriesMap.texture	=	0;

    // create the main window
    window_init();
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
            draw_clock(winboxes.clock, Sans);
            draw_map(winboxes.map);
            draw_panel_border(winboxes.map);
            draw_de_dx(winboxes.de, Sans, 37.978, -84.495, 1);
            draw_panel_border(winboxes.de);
            draw_de_dx(winboxes.dx, Sans, 0, 0, 0);
            draw_panel_border(winboxes.dx);


        draw_panel_border(winboxes.de);

        draw_panel_border(winboxes.dx);


        draw_panel_border(winboxes.rowbox1);
        pota_spots(winboxes.rowbox1, Sans);

        draw_panel_border(winboxes.rowbox2);

        draw_panel_border(winboxes.rowbox3);

        draw_panel_border(winboxes.rowbox4);
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
            resize_panels(&winboxes);
            draw_callsign(winboxes.callsign, Sans, "KQ4SIZ");
            resizing=0;
        }
    }
    return SDL_APP_CONTINUE;
}

