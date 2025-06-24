#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include "aaediclock.h"
#include "modules.h"
#include "utils.h"

void draw_de_dx(struct ScreenFrame panel, TTF_Font* font, double lat, double lon, int de_dx) {

    if (!surface) {
        SDL_Log("Missing Surface!");
        return ;
    }
    if (!panel.texture) {
        SDL_Log("Missing PANEL!");
        return ;
    }
    char *tempstr;
    tempstr = (char*)malloc(128);
    SDL_FRect TextRect;
    SDL_Surface* textsurface;
    SDL_Texture *TextTexture;

    SDL_SetRenderTarget(surface, panel.texture);
    SDL_Color fontcolor;
    if (de_dx) {
        fontcolor.r=128;
        fontcolor.g=255;
        fontcolor.b=128;
        fontcolor.a=0;
    } else {
        fontcolor.r=255;
        fontcolor.g=128;
        fontcolor.b=128;
        fontcolor.a=0;
    }
    if (!font) {
        printf("No font defined\n");
        return;
    }
    float oldsize = TTF_GetFontSize(font);
    TTF_SetFontSize(font,72);
    SDL_Log("Set Font");

    // blank the box
    SDL_SetRenderTarget(surface, panel.texture);
    SDL_SetRenderDrawBlendMode(surface, SDL_BLENDMODE_NONE);  // Clear solid
    SDL_SetRenderDrawColor(surface, 0, 0, 0, 255);
    SDL_RenderClear(surface);  // Fills the entire target with the draw color
    SDL_SetRenderDrawBlendMode(surface, SDL_BLENDMODE_BLEND);  // Clear solid

    // fet sunrise and sunset times
    tm* utc = gmtime(&currenttime);
    time_t sunrise;
    time_t sunset;
    double solar_decl = 23.45 * (sin( (2 * M_PI/365) * (284+(utc->tm_yday+1)) ));
    double solar_alt = solar_altitude(lat, lon, utc, solar_decl);
    // find the next zero crossing for sunrise if current alt <0

    double test_alt;
    test_alt = solar_alt;
    tm* test_time;
    sunrise = currenttime;
    sunset = currenttime;
    if (test_alt <0) {
        while (test_alt <0) {
            sunrise +=5;
            sunset += 5;
            test_time = gmtime(&sunrise);
            test_alt = solar_altitude(lat, lon, test_time, solar_decl);
        }
        while (test_alt >0) {
            sunset += 5;
            test_time = gmtime(&sunset);
            test_alt = solar_altitude(lat, lon, test_time, solar_decl);
        }
    } else {
       while (test_alt >0) {
            sunrise +=5;
            sunset += 5;
            test_time = gmtime(&sunset);
            test_alt = solar_altitude(lat, lon, test_time, solar_decl);
        }
        while (test_alt <0) {
            sunrise += 5;
            test_time = gmtime(&sunrise);
            test_alt = solar_altitude(lat, lon, test_time, solar_decl);
        }
    }


    // render the header
    TextRect.x=2;
    TextRect.y=2;
    TextRect.h=(panel.dims.h)/4;
    TextRect.w=(panel.dims.w)-4;

    if (de_dx) {
        textsurface = TTF_RenderText_Shaded(font, "DE:", 3, fontcolor, fontcolor);
    } else {
        textsurface = TTF_RenderText_Shaded(font, "DX:", 3, fontcolor, fontcolor);
    }
    TextTexture = SDL_CreateTextureFromSurface(surface, textsurface);
    SDL_SetRenderTarget(surface, panel.texture);
    SDL_RenderTexture(surface, TextTexture, NULL, &TextRect);
    SDL_DestroyTexture(TextTexture);
    SDL_DestroySurface(textsurface);

    // generate maidenhead grid square

    char latstr[2];
    char lonstr[2];
    char maiden[7];
    double madlon, madlat;
    madlon = lon + 180;
    madlat = lat + 90;
    maiden[6]=0;
    maiden[0]=((int)(madlon/20))+65;
    maiden[1]=((int)(madlat/10))+65;
    maiden[2]=(int)(((int)madlon % 20)/2)+48;
    maiden[3]=(int)(((int)madlat + 90) % 10)+48;
    maiden[4] = (int)(((fmod(madlon,2.0))/2.0)*24)+97;
    maiden[5] = (int)((fmod(madlat,1.0))*24)+97;

    // lat/lon string generation
    if (lat < 0) {
        lat *= -1;
        latstr[0]='S';
        latstr[1]=0;
    } else {
        latstr[0]='N';
        latstr[1]=0;
    }

    if (lon < 0) {
        lon *= -1;
        lonstr[0]='W';
        lonstr[1]=0;
    } else {
        lonstr[0]='E';
        lonstr[1]=0;
    }
    sprintf(tempstr, "%2.2f%s %2.2f%s", lat, latstr, lon, lonstr);
    // render lat/lon
    TextRect.x=2;
    TextRect.y=(panel.dims.h)/4;
    TextRect.h=(panel.dims.h)/4;
    TextRect.w=panel.dims.w-4;
    textsurface = TTF_RenderText_Shaded(font, tempstr, strlen(tempstr), fontcolor, fontcolor);
    TextTexture = SDL_CreateTextureFromSurface(surface, textsurface);
    SDL_SetRenderTarget(surface, panel.texture);
    SDL_RenderTexture(surface, TextTexture, NULL, &TextRect);
    SDL_DestroyTexture(TextTexture);
    SDL_DestroySurface(textsurface);

    // render maidenhead
    TextRect.x=2;
    TextRect.y=(panel.dims.h)/2;
    TextRect.h=(panel.dims.h)/4;
    TextRect.w=panel.dims.w-4;
    sprintf(tempstr, "%s", maiden);
    textsurface = TTF_RenderText_Shaded(font, tempstr, strlen(tempstr), fontcolor, fontcolor);
    TextTexture = SDL_CreateTextureFromSurface(surface, textsurface);
    SDL_SetRenderTarget(surface, panel.texture);
    SDL_RenderTexture(surface, TextTexture, NULL, &TextRect);
    SDL_DestroyTexture(TextTexture);
    SDL_DestroySurface(textsurface);

    // render sunrise time
    TextRect.x=2;
    TextRect.y=((panel.dims.h)/4)*3;
    TextRect.h=(panel.dims.h)/8;
    TextRect.w=(panel.dims.w/3)-4;
    test_time = localtime(&sunrise);
    strftime(tempstr, 12, "R%H:%M", test_time);
    textsurface = TTF_RenderText_Shaded(font, tempstr, strlen(tempstr), fontcolor, fontcolor);
    TextTexture = SDL_CreateTextureFromSurface(surface, textsurface);
    SDL_SetRenderTarget(surface, panel.texture);
    SDL_RenderTexture(surface, TextTexture, NULL, &TextRect);
    SDL_DestroyTexture(TextTexture);
    SDL_DestroySurface(textsurface);

    // render solar angle
    TextRect.x=(panel.dims.w/3)+4;
    TextRect.y=((panel.dims.h)/4)*3;
    TextRect.h=(panel.dims.h)/8;
    TextRect.w=(panel.dims.w/3)-8;
    test_time = localtime(&sunset);
    sprintf (tempstr, "%2.2f", solar_alt);
    textsurface = TTF_RenderText_Shaded(font, tempstr, strlen(tempstr), fontcolor, fontcolor);
    TextTexture = SDL_CreateTextureFromSurface(surface, textsurface);
    SDL_SetRenderTarget(surface, panel.texture);
    SDL_RenderTexture(surface, TextTexture, NULL, &TextRect);
    SDL_DestroyTexture(TextTexture);
    SDL_DestroySurface(textsurface);

    // render sunset time
    TextRect.x=(panel.dims.w/3)*2;
    TextRect.y=((panel.dims.h)/4)*3;
    TextRect.h=(panel.dims.h)/8;
    TextRect.w=(panel.dims.w/3)-4;
    test_time = localtime(&sunset);
    strftime(tempstr, 12, "S%H:%M", test_time);
    textsurface = TTF_RenderText_Shaded(font, tempstr, strlen(tempstr), fontcolor, fontcolor);
    TextTexture = SDL_CreateTextureFromSurface(surface, textsurface);
    SDL_SetRenderTarget(surface, panel.texture);
    SDL_RenderTexture(surface, TextTexture, NULL, &TextRect);
    SDL_DestroyTexture(TextTexture);
    SDL_DestroySurface(textsurface);

    // clean up
    free(tempstr);
    SDL_SetRenderTarget(surface, NULL);
    SDL_RenderTexture(surface, panel.texture, NULL, &(panel.dims));
    TTF_SetFontSize(font,oldsize);


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
    SDL_SetRenderTarget(surface, panel.texture);
    SDL_RenderTexture(surface, TextTexture, NULL, &TextRect);
    SDL_DestroyTexture(TextTexture);
    SDL_DestroySurface(textsurface);
    SDL_SetRenderTarget(surface, NULL);
    SDL_RenderTexture(surface, panel.texture, NULL, &(panel.dims));
    TTF_SetFontSize(font,oldsize);
    SDL_Log("Rendering Callsign Done");
}

void load_maps() {
//    DayMap.surface = SDL_LoadBMP("/home/lunatic/.hamclock/map-D-660x330-Countries.bmp");
    DayMap.surface = SDL_LoadBMP("images/Blue_Marble_2002.bmp");
    if (DayMap.surface) {
        DayMap.texture = SDL_CreateTextureFromSurface(surface,DayMap.surface);
        if (!DayMap.texture) {
            SDL_Log("Unable to load DayMap Texture: %s\n", SDL_GetError());
            exit(1);
        }
    } else {
        SDL_Log("Unable to load DayMap Surface: %s\n", SDL_GetError());
        exit(1);
    }
//    NightMap.surface = SDL_LoadBMP("/home/lunatic/.hamclock/map-N-660x330-Countries.bmp");
    NightMap.surface = SDL_LoadBMP("images/Black_Marble_2016.bmp");
    if (NightMap.surface) {
        NightMap.texture = SDL_CreateTextureFromSurface(surface,NightMap.surface);
        if (!NightMap.texture) {
        SDL_Log("Unable to load NightMap Texture: %s\n", SDL_GetError());
        }
    } else {
        SDL_Log("Unable to load NightMap Surface: %s\n", SDL_GetError());
        exit(1);
    }

    if ((!DayMap.texture) || (!NightMap.texture)) {
        SDL_Log("Unable to load Maps");
//        return (SDL_APP_FAILURE);
    }

    SDL_SetTextureBlendMode(DayMap.texture, SDL_BLENDMODE_NONE);
    SDL_SetTextureBlendMode(NightMap.texture, SDL_BLENDMODE_NONE);


    CountriesMap.surface = SDL_LoadBMP("images/outline.bmp");
    if (CountriesMap.surface) {
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
        if (!CountriesMap.texture) {
                SDL_Log("Unable to load Country texture: %s\n", SDL_GetError());
                exit(1);
        }
    } else {
        SDL_Log("Unable to load Country Surface: %s\n", SDL_GetError());
        exit(1);
    }
    SDL_Log("ALL MAPS LOADED %s\n", SDL_GetError());
    return;

}

//https://celestrak.org/NORAD/elements/gp.php?GROUP=amateur&FORMAT=tle
// https://api.pota.app/spot/activator
// https://retrieve.pskreporter.info/query?senderCallsign=YOUR_CALLSIGN
// VOACAP (or VOACAPL)
//HamQSL.com / NOAA API


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
    SDL_Log("Rendered Daymap to texture");

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