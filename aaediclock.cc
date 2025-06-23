#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

SDL_Window* window =NULL;
SDL_Renderer* surface =NULL;
TTF_Font* Sans = NULL;

time_t currenttime;
struct ScreenFrame {
    SDL_Surface*	surface;
    SDL_Texture*	texture;
    SDL_FRect		dims;
};
struct ScreenFrame DayMap;
struct ScreenFrame NightMap;
struct ScreenFrame CountriesMap;

struct surfaces {
    struct ScreenFrame	map;
    struct ScreenFrame	callsign;
    struct ScreenFrame	de;
    struct ScreenFrame	dx;
    struct ScreenFrame  clock;
    struct ScreenFrame  rowbox1;
    struct ScreenFrame  rowbox2;
    struct ScreenFrame  rowbox3;
    struct ScreenFrame  rowbox4;
    struct ScreenFrame  ticker;

} winboxes;


void draw_panel_border(struct ScreenFrame panel) {
    SDL_SetRenderDrawColor(surface, 128, 128, 128, 255);
    SDL_FRect border;
    border.x=0;
    border.y=0;
    border.w=panel.dims.w;
    border.h=panel.dims.h;
    SDL_SetRenderTarget(surface, panel.texture);
    SDL_RenderRect(surface, &(border));
    SDL_SetRenderTarget(surface, NULL);
    SDL_RenderTexture(surface, panel.texture, NULL, &(panel.dims));
    return;
}

void draw_callsign(struct ScreenFrame panel, TTF_Font* font, const char* callsign) {
    if (!surface) {
        SDL_Log("Missing Surface!");
        return ;
    }
    if (!panel.texture) {
        SDL_Log("Missing PANEL!");
        return ;
    }
    SDL_Log("Rendering Callsign");
    SDL_SetRenderTarget(surface, panel.texture);
    SDL_Log("Setting color");
    SDL_Color fontcolor;
    fontcolor.r=128;
    fontcolor.g=128;
    fontcolor.b=255;
    fontcolor.a=0;
    if (!font) {
        printf("No font defined\n");
        return;
    }
    float oldsize = TTF_GetFontSize(font);
    TTF_SetFontSize(font,72);
    SDL_Log("Set Font");

    SDL_FRect TextRect;
    SDL_Surface* textsurface;
    SDL_Texture *TextTexture;
    TextRect.x=2;
    TextRect.y=2;
    TextRect.h=(panel.dims.h)-4;
    TextRect.w=(panel.dims.w)-4;

    textsurface = TTF_RenderText_Shaded(font, callsign, strlen(callsign), fontcolor, fontcolor);
    TextTexture = SDL_CreateTextureFromSurface(surface, textsurface);
    SDL_Log("Plotting");
    SDL_SetRenderTarget(surface, panel.texture);
    SDL_RenderTexture(surface, TextTexture, NULL, &TextRect);
    SDL_SetRenderTarget(surface, NULL);
    SDL_RenderTexture(surface, panel.texture, NULL, &(panel.dims));
    TTF_SetFontSize(font,oldsize);
    SDL_Log("Rendering Callsign Done");
}
double solar_altitude(double lat_deg, double lon_deg, struct tm *utc, double decl_deg) {
    //Converts latitude and solar declination from degrees to radians
    double lat = lat_deg * M_PI / 180.0;
    double decl = decl_deg * M_PI / 180.0;

    double utc_hours = utc->tm_hour + utc->tm_min / 60.0 + utc->tm_sec / 3600.0;
    double solar_time = utc_hours + (lon_deg / 15.0);  // Local solar time for pixel
    double hour_angle = (15.0 * (solar_time - 12.0)) * M_PI / 180.0;
    double sin_alt = sin(lat) * sin(decl) + cos(lat) * cos(decl) * cos(hour_angle);
    return asin(sin_alt) * 180.0 / M_PI;
}
void load_maps() {
//    DayMap.surface = SDL_LoadBMP("/home/lunatic/.hamclock/map-D-660x330-Countries.bmp");
    DayMap.surface = SDL_LoadBMP("images/Blue_Marble_2002.bmp");
    if (DayMap.surface) {
        DayMap.texture = SDL_CreateTextureFromSurface(surface,DayMap.surface);
        if (!DayMap.texture) {
        SDL_Log("Unable to load DayMaps");
        }
    }
//    NightMap.surface = SDL_LoadBMP("/home/lunatic/.hamclock/map-N-660x330-Countries.bmp");
    NightMap.surface = SDL_LoadBMP("images/Black_Marble_2016.bmp");
    if (NightMap.surface) {
        NightMap.texture = SDL_CreateTextureFromSurface(surface,NightMap.surface);
        if (!NightMap.texture) {
        SDL_Log("Unable to load NightMaps: %s\n", SDL_GetError());
        }
    }

    if ((!DayMap.texture) || (!NightMap.texture)) {
        SDL_Log("Unable to load Maps");
//        return (SDL_APP_FAILURE);
    }
    SDL_SetTextureBlendMode(DayMap.texture, SDL_BLENDMODE_NONE);
    SDL_SetTextureBlendMode(NightMap.texture, SDL_BLENDMODE_NONE);


    CountriesMap.surface = SDL_LoadBMP("images/outline.bmp");
    int x, y;
    Uint8 cg, cr, cb;
    Uint8* country_pixels = (Uint8*)CountriesMap.surface->pixels;
    const Uint8 bpp = SDL_GetPixelFormatDetails(CountriesMap.surface->format)->bytes_per_pixel;

    for ( y = 0; y < CountriesMap.surface->h ; y++) {
     for (x=0 ; x < CountriesMap.surface->w ; x++) {
       int pixel_index = ( CountriesMap.surface->w * bpp * y ) + ( bpp * x );

       Uint32 *pixel_val=(Uint32*)(pixel_index+country_pixels);

       SDL_GetRGBA( *pixel_val, SDL_GetPixelFormatDetails(CountriesMap.surface->format), NULL, &cr, &cg, &cb, NULL);
            Uint32 pixel_val_out = SDL_MapRGBA(SDL_GetPixelFormatDetails(CountriesMap.surface->format), NULL, 0, 0, 0, (255-cg));
            memcpy((country_pixels + pixel_index), &pixel_val_out, bpp);
     }
    }
    CountriesMap.texture = SDL_CreateTextureFromSurface(surface,CountriesMap.surface);


return;

}



int draw_map(struct ScreenFrame panel) {


    tm* utc = gmtime(&currenttime);
    double softness = 10.0;
    SDL_Rect panel_cords, source_cords, dest_cords;
    double solar_decl = 23.45 * (sin( (2 * M_PI/365) * (284+(utc->tm_yday+1)) ));
    SDL_Log("Drawing Map ");
    if (!surface) {
        SDL_Log("Missing Surface!");
        return 1;
    }
    if (!panel.texture) {
        SDL_Log("Missing PANEL!");
        return 1;
    }

    // blank the box
    SDL_SetRenderTarget(surface, panel.texture);
    SDL_SetRenderDrawBlendMode(surface, SDL_BLENDMODE_NONE);  // Clear solid
    SDL_SetRenderDrawColor(surface, 0, 0, 0, 255);  // Red, fully opaque
    SDL_RenderClear(surface);  // Fills the entire target with the draw color
     SDL_SetRenderDrawBlendMode(surface, SDL_BLENDMODE_BLEND);  // Clear solid

    // start with the day map
    SDL_SetRenderTarget(surface, panel.texture);
    SDL_RenderTexture(surface, DayMap.texture, NULL, NULL);


    // init the night map alpha mask
    SDL_Surface* night_mask = SDL_CreateSurface(panel.dims.w, panel.dims.h, SDL_PIXELFORMAT_RGBA32);
    Uint8* alpha_pixels = (Uint8*)night_mask->pixels;
    Uint8* source_pixels = (Uint8*)NightMap.surface->pixels;
    const Uint8 dest_bpp = SDL_GetPixelFormatDetails(night_mask->format)->bytes_per_pixel;
    const Uint8 source_bpp = SDL_GetPixelFormatDetails(NightMap.surface->format)->bytes_per_pixel;
    if (!night_mask) {
        SDL_Log("Failed to create mask surface: %s", SDL_GetError());
        return 1;
    }


    // calculate the NightMap Alpha mask
    for (panel_cords.y=0 ; panel_cords.y < panel.dims.h ; panel_cords.y++) {
        double lat = 90.0 - (180.0 * panel_cords.y / (double)panel.dims.h);
        for (panel_cords.x=0 ; panel_cords.x < panel.dims.w ; panel_cords.x++) {
            Uint8 r, g, b;
            Uint8 bpp;
            double lon = -180.0 + (360.0 * panel_cords.x / (double)panel.dims.w);
            double alt = solar_altitude(lat, lon, utc, solar_decl);
            // calculate per pixel alpha
            Uint8 alpha;
            if (alt > softness) {
                alpha = 255;
            } else if (alt < -softness) {
                alpha = 0;
            } else {
                alpha = (Uint8)(255.0 * (alt + softness) / (2.0 * softness));
            }
            // Write a pixel with the computed alpha
            source_cords.y = (panel_cords.y/panel.dims.h)*NightMap.surface->h;
            source_cords.x = (panel_cords.x/panel.dims.w)*NightMap.surface->w;
            int source_pixel_index = ( NightMap.surface->w * source_bpp * source_cords.y ) + ( source_bpp * source_cords.x );
            int dest_pixel_index =   ( night_mask->w * dest_bpp * panel_cords.y ) + ( dest_bpp * panel_cords.x );

            Uint32 *source_pixel_val=(Uint32*)(source_pixel_index+source_pixels);
            SDL_GetRGBA( *source_pixel_val, SDL_GetPixelFormatDetails(NightMap.surface->format), NULL, &r, &g, &b, NULL);
            Uint32 dst_pixel_val = SDL_MapRGBA(SDL_GetPixelFormatDetails(night_mask->format), NULL, r, g, b, (255 - alpha));
            memcpy((alpha_pixels + dest_pixel_index), &dst_pixel_val, dest_bpp);

        }
    }
    // render the masked NightMap to the panel

    SDL_Texture* mask_tex = SDL_CreateTextureFromSurface(surface, night_mask);
    SDL_DestroySurface(night_mask);
    if (!mask_tex) {
        SDL_Log("Failed to create mask texture: %s", SDL_GetError());
        return 1;
    }
    //set the blend mode for the alpha overlay of Night Map
    SDL_SetTextureBlendMode(mask_tex, SDL_BLENDMODE_BLEND);
    SDL_SetRenderTarget(surface, panel.texture);
    SDL_RenderTexture(surface, mask_tex, NULL, NULL);
    SDL_DestroyTexture(mask_tex);
    SDL_RenderTexture(surface, CountriesMap.texture, NULL, NULL);
    // draw equator and tropics
    SDL_SetRenderDrawColor(surface, 128,128,128,64);
    SDL_RenderLine(surface, 0,(panel.dims.h/2), panel.dims.w, (panel.dims.h/2));
    SDL_SetRenderDrawColor(surface, 128,0,0,64);
    int tropic;
    tropic = ((-23.4+90) * panel.dims.h)/180;
    SDL_RenderLine(surface, 0,tropic, panel.dims.w, tropic);
    tropic = ((23.4+90) * panel.dims.h)/180;
    SDL_RenderLine(surface, 0,tropic, panel.dims.w, tropic);

    // map the result to the window
    SDL_SetRenderTarget(surface, NULL);
    SDL_RenderTexture(surface, panel.texture, NULL, &(panel.dims));
    SDL_Log("Drawing Map Complete\n\n");
    return 0;
}

int draw_clock(struct ScreenFrame panel, TTF_Font* font) {

    SDL_Log("Drawing Clock");
    SDL_SetRenderTarget(surface, panel.texture);
    SDL_Color fontcolor;
    fontcolor.r=128;
    fontcolor.g=128;
    fontcolor.b=255;
    fontcolor.a=0;
    char timestr[64];

    SDL_FRect TextRect;
    SDL_Surface *datesurface, *utcsurface, *localsurface;
    SDL_Texture *TextTexture;

    float oldsize = TTF_GetFontSize(font);
    TTF_SetFontSize(font,72);


    if (!font) {
        printf("No font defined\n");
        return 1;
    }

    // blank the box
    SDL_SetRenderTarget(surface, panel.texture);
    SDL_SetRenderDrawColor(surface, 0, 0, 0, 255);  // Red, fully opaque
    SDL_RenderClear(surface);  // Fills the entire target with the draw color

    // generate the time strings
//    strftime(timestr, sizeof(timestr), "%Y-%m-%d", utc);
//    datesurface = TTF_RenderText_Shaded(font, timestr, strlen(timestr), fontcolor, fontcolor);
    struct tm* utc = gmtime(&currenttime);

    strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S %Z", utc);
    utcsurface = TTF_RenderText_Shaded(font, timestr, strlen(timestr), fontcolor, fontcolor);
    struct tm* local = localtime(&currenttime);
    strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S %Z", local);
    localsurface = TTF_RenderText_Shaded(font, timestr, strlen(timestr), fontcolor, fontcolor);

    // render
    SDL_SetRenderTarget(surface, panel.texture);
    TextTexture = SDL_CreateTextureFromSurface(surface, datesurface);
     // date
 //   TextRect.x=2;
 //   TextRect.y=panel.dims.h/4;
//    TextRect.h=(panel.dims.h/2);
//    TextRect.w=(panel.dims.w/2)-4;
//    SDL_RenderTexture(surface, TextTexture, NULL, &TextRect);
//    SDL_DestroyTexture(TextTexture);
//    SDL_DestroySurface(datesurface);


     // utc
    TextRect.x=2;
    TextRect.y=panel.dims.h/4;
    TextRect.h=panel.dims.h/4;
    TextRect.w=(panel.dims.w)-4;
    TextTexture = SDL_CreateTextureFromSurface(surface, utcsurface);
    SDL_RenderTexture(surface, TextTexture, NULL, &TextRect);
    SDL_DestroyTexture(TextTexture);
    SDL_DestroySurface(utcsurface);

     // local
    TextRect.x=2;
    TextRect.y=panel.dims.h/2;
    TextRect.h=panel.dims.h/4;
    TextRect.w=(panel.dims.w)-4;
    TextTexture = SDL_CreateTextureFromSurface(surface, localsurface);
    SDL_RenderTexture(surface, TextTexture, NULL, &TextRect);
    SDL_DestroyTexture(TextTexture);
    SDL_DestroySurface(localsurface);

    SDL_SetRenderTarget(surface, NULL);
    SDL_RenderTexture(surface, panel.texture, NULL, &(panel.dims));
    TTF_SetFontSize(font,oldsize);
        SDL_Log("Drawing Clock Complete");

  return 0;
}
void resize_panels(struct surfaces* panels) {
        int win_x;
        int win_y;


        if (surface) {
            if (panels->callsign.texture) {
                SDL_DestroyTexture(panels->callsign.texture);
                panels->callsign.texture=0;
            }
            if (panels->clock.texture) {
                SDL_DestroyTexture(panels->clock.texture);
                panels->clock.texture=0;
            }
            if (panels->map.texture) {
                SDL_DestroyTexture(panels->map.texture);
                panels->map.texture=0;
            }
            if (panels->de.texture) {
                SDL_DestroyTexture(panels->de.texture);
                panels->de.texture=0;
            }
            if (panels->dx.texture) {
                SDL_DestroyTexture(panels->dx.texture);
                panels->dx.texture=0;
            }
            if (panels->rowbox1.texture) {
                SDL_DestroyTexture(panels->rowbox1.texture);
                panels->rowbox1.texture=0;
            }
            if (panels->rowbox2.texture) {
                SDL_DestroyTexture(panels->rowbox2.texture);
                panels->rowbox2.texture=0;
            }

            if (panels->rowbox3.texture) {
                SDL_DestroyTexture(panels->rowbox3.texture);
                panels->rowbox3.texture=0;
            }
            if (panels->rowbox4.texture) {
                SDL_DestroyTexture(panels->rowbox4.texture);
                panels->rowbox4.texture=0;
            }
            SDL_DestroyRenderer(surface);
        }
        surface = SDL_CreateRenderer(window, NULL);
        SDL_GetCurrentRenderOutputSize(surface, &win_x, &win_y);
        printf ("Resizing Window to %i X %i\n", win_x, win_y);
        SDL_SetRenderDrawColor(surface, 0,0,0,0);
        SDL_RenderClear(surface);
        SDL_RenderPresent(surface);
        SDL_Log("Rebuilding Callsign tex");
        panels->callsign.dims.x	=	0;
        panels->callsign.dims.y	=	0;
        panels->callsign.dims.w	=	(win_x/6)*2;
        panels->callsign.dims.h	=	win_y/8;
        panels->callsign.texture=SDL_CreateTexture(surface, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, panels->callsign.dims.w, panels->callsign.dims.h);
        if (!panels->callsign.texture) {
            printf("Error Creating callsign tex: %s\n", SDL_GetError());
        }
        SDL_Log("Rebuilding Clock tex");
        panels->clock.dims.x=0;
        panels->clock.dims.y=win_y/8;
        panels->clock.dims.w=(win_x/6)*2;
        panels->clock.dims.h=win_y/8;

        panels->clock.texture=SDL_CreateTexture(surface, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, panels->clock.dims.w, panels->clock.dims.h);
        if (!panels->clock.texture) {
            printf("Error Creating Clock: %s\n", SDL_GetError());
        }

        SDL_Log("Rebuilding Map tex");
        panels->map.dims.x=win_x/6;
        panels->map.dims.y=win_y/4;
        panels->map.dims.w=(win_x/6)*5;
        panels->map.dims.h=(win_y/4)*3;

        panels->map.texture=SDL_CreateTexture(surface, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, panels->map.dims.w, panels->map.dims.h);
        if (!panels->map.texture) {
            printf("Error Creating MAP: %s\n", SDL_GetError());
        }
        SDL_Log("Rebuilding DE tex");
        panels->de.dims.x=0;
        panels->de.dims.y=win_y/4;
        panels->de.dims.w=win_x/6;
        panels->de.dims.h=win_y/4;

        panels->de.texture=SDL_CreateTexture(surface, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, panels->de.dims.w, panels->de.dims.h);
        if (!panels->de.texture) {
            printf("Error Creating DE: %s\n", SDL_GetError());
        }
        SDL_Log("Rebuilding DX tex");
        panels->dx.dims.x=0;
        panels->dx.dims.y=win_y/2;
        panels->dx.dims.w=win_x/6;
        panels->dx.dims.h=win_y/4;
        panels->dx.texture=SDL_CreateTexture(surface, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, panels->dx.dims.w, panels->dx.dims.h);
        if (!panels->dx.texture) {
            printf("Error Creating DX: %s\n", SDL_GetError());
        }
        SDL_Log("Rebuilding Rwobox1 tex");
        panels->rowbox1.dims.x=(win_x/6)*2;;
        panels->rowbox1.dims.y=0;
        panels->rowbox1.dims.w=(win_x/6);
        panels->rowbox1.dims.h=win_y/4;
        panels->rowbox1.texture=SDL_CreateTexture(surface, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, panels->rowbox1.dims.w, panels->rowbox1.dims.h);
        if (!panels->rowbox1.texture) {
            printf("Error Creating RowBox1: %s\n", SDL_GetError());
        }
        SDL_Log("Rebuilding Rowbox2 tex");
        panels->rowbox2.dims.x=(win_x/6)*3;
        panels->rowbox2.dims.y=0;
        panels->rowbox2.dims.w=(win_x/6);
        panels->rowbox2.dims.h=win_y/4;
        panels->rowbox2.texture=SDL_CreateTexture(surface, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, panels->rowbox2.dims.w, panels->rowbox2.dims.h);
        if (!panels->rowbox2.texture) {
            printf("Error Creating RowBox2: %s\n", SDL_GetError());
        }
        SDL_Log("Rebuilding Rowbox3 tex");
        panels->rowbox3.dims.x=(win_x/6)*4;
        panels->rowbox3.dims.y=0;
        panels->rowbox3.dims.w=(win_x/6);
        panels->rowbox3.dims.h=win_y/4;
        panels->rowbox3.texture=SDL_CreateTexture(surface, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, panels->rowbox3.dims.w, panels->rowbox3.dims.h);
        if (!panels->rowbox3.texture) {
            printf("Error Creating RowBox3: %s\n", SDL_GetError());
        }
        SDL_Log("Rebuilding Rowbox4 tex");
        panels->rowbox4.dims.x=(win_x/6)*5;
        panels->rowbox4.dims.y=0;
        panels->rowbox4.dims.w=(win_x/6);
        panels->rowbox4.dims.h=win_y/4;
        panels->rowbox4.texture=SDL_CreateTexture(surface, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, panels->rowbox4.dims.w, panels->rowbox4.dims.h);
        if (!panels->rowbox4.texture) {
            printf("Error Creating RowBox4: %s\n", SDL_GetError());
        }
        load_maps();
        SDL_Log("Done resizing panels");
        return;
}

int window_init() {
    if (!window) {
        window = SDL_CreateWindow("Aaediwen Ham Clock", 800, 480, 0);
        SDL_SetWindowResizable(window, 1);
        resize_panels(&winboxes);
        if (!window || !surface) {
            printf("Window creation error\n");
            return(1);
        }

        currenttime=time(NULL);
        Sans = TTF_OpenFont("arial.ttf", 24);
        if (!Sans) {
            printf("Error opening font: %s\n", SDL_GetError());
            return(1);
        }
        currenttime=time(NULL);

        draw_panel_border(winboxes.callsign);
        draw_callsign(winboxes.callsign, Sans, "KQ4SIZ");


        draw_panel_border(winboxes.clock);
        draw_clock(winboxes.clock, Sans);


        draw_panel_border(winboxes.map);
        load_maps();
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
    if (!(SDL_InitSubSystem(SDL_INIT_VIDEO))) {
        SDL_Log("Unable to initialize SDL: %s", SDL_GetError());
        return (SDL_APP_FAILURE);
    }
    if(TTF_Init()==-1) {
        printf("TTF_Init: %s\n", SDL_GetError());
        return(SDL_APP_FAILURE);
    }
    window=0;
    surface=0;
    winboxes.callsign.texture=0;
    winboxes.map.texture=0;
    winboxes.dx.texture=0;
    winboxes.de.texture=0;
    winboxes.clock.texture=0;
    winboxes.rowbox1.texture=0;
    winboxes.rowbox2.texture=0;
    winboxes.rowbox3.texture=0;
    winboxes.rowbox4.texture=0;

    window_init();
    return(SDL_APP_CONTINUE);

}

time_t oldtime;
int resizing = 0;
SDL_AppResult SDL_AppIterate(void *appstate) {
    SDL_Delay(10);
    if (!resizing) {
    currenttime=time(NULL);
    if (currenttime != oldtime) {
    draw_clock(winboxes.clock, Sans);
    draw_panel_border(winboxes.clock);

    draw_map(winboxes.map);
    draw_panel_border(winboxes.map);
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

