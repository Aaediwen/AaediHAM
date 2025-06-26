#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include "aaediclock.h"
#include "modules.h"
#include "utils.h"
#include <curl/curl.h>
#include "json.hpp"
using json = nlohmann::json;
uint pota_http_callback( char* in, uint size, uint nmemb, void* out) {
//    SDL_Log("Got %i bytes from HTTP", nmemb);
    std::string* buffer = static_cast<std::string*>(out);
    buffer->append(in, (size*nmemb));
    return (size*nmemb);
}
void pota_spots(struct ScreenFrame panel, TTF_Font* font) {

    Uint32 cache_size;
    time_t cache_age;
    char* json_spots = 0 ;
    delete_owner_pins(MOD_POTA);
    if (fetch_data_cache(MOD_POTA, &cache_age, &cache_size, NULL)) {
//        SDL_Log("Fetching %i Bytes from cache", cache_size);
        json_spots = (char*)malloc(cache_size+1);
        fetch_data_cache(MOD_POTA, &cache_age, &cache_size, json_spots);
//        SDL_Log("Got from Cache %i Bytes", strlen(json_spots));
    }

    if ( (!json_spots)|| ( (time(NULL) - cache_age > 300) ) ) {
        std::string spotbuffer;
        SDL_Log("Fetching new POTA SPOTS");
        CURL *curl = curl_easy_init();
        if (curl) {
            //  https://api.pota.app/spot/activator
    //        curl_easy_setopt(curl, CURLOPT_URL, "https://api.pota.app/spot/activator");
            curl_easy_setopt(curl, CURLOPT_URL, "https://aaediwen.theaudioauthority.net/morse/activator");
            curl_easy_setopt(curl, CURLOPT_HTTP_VERSION,
                             (long)CURL_HTTP_VERSION_3);
            curl_easy_setopt(curl, CURLOPT_USERAGENT, "aaediwens-pota-module-agent/1.0");
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, pota_http_callback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&spotbuffer);
            CURLcode res = curl_easy_perform(curl);
            curl_easy_cleanup(curl);
            add_data_cache(MOD_POTA, spotbuffer.size(), (void*)spotbuffer.c_str());
        } else {
            SDL_Log("Failed ot init Curl!");
        }
        json_spots = 0 ;
        if (fetch_data_cache(MOD_POTA, &cache_age, &cache_size, NULL)) {
            json_spots = (char*)malloc(cache_size);
            fetch_data_cache(MOD_POTA, &cache_age, &cache_size, json_spots);
        }
        if (!json_spots) {
            SDL_Log("Unable to retrieve POTA SPOTS!");
            return;
        }
        bool goodread=true;
    } else {
        SDL_Log("Using cached POTA SPOTS %i Bytes", strlen(json_spots));
    }

    int goodread;
    goodread = 1;
    json spot_list;
    try {
        spot_list=json::parse(json_spots);
    } catch (const json::parse_error &e) {
        SDL_Log("POTA Json Parse Error %i bytes\n", strlen(json_spots));
        goodread=0;
    }

    SDL_SetRenderTarget(surface, panel.texture);
    SDL_SetRenderDrawBlendMode(surface, SDL_BLENDMODE_NONE);  // Clear solid
    SDL_SetRenderDrawColor(surface, 0, 0, 0, 255);
    SDL_RenderClear(surface);  // Fills the entire target with the draw color
    SDL_SetRenderDrawBlendMode(surface, SDL_BLENDMODE_BLEND);  // Clear solid


    SDL_Color pota_color;
    pota_color.r = 0;
    pota_color.g = 128;
    pota_color.b = 0;
    pota_color.a = 200;

    char tempstr[64];
    SDL_FRect TextRect;
    TextRect.w=panel.dims.w-10;
    TextRect.h=panel.dims.h/11;
    TextRect.x=5;
    TextRect.y=2;
    pota_color.a = 0;
    sprintf(tempstr, "POTA ACTIVATORS");
    render_text(surface, panel.texture, &TextRect, font, pota_color, tempstr);
    pota_color.a = 200;
    TextRect.w=(panel.dims.w/4)-10;
    TextRect.h=panel.dims.h/11;
    TextRect.x=5;
    TextRect.y=((panel.dims.h/11)+(panel.dims.h/150));;
    int c;
    c=0;
    std::string tempstdstring;
    if (goodread) {
        for (auto spot : spot_list) {
            if (spot.contains("spotId") ) {
                // submit the map pin
                int id = spot.value("spotId", -1);
//          	SDL_Log("%i\n",id);
                struct map_pin pota_pin;
//                pota_pin.lat=spot.value("latitude", -1);;
                pota_pin.lat = spot["latitude"].template get<double>();
                pota_pin.owner=MOD_POTA;
                tempstdstring=spot["activator"].template get<std::string>();
                sprintf(pota_pin.label, "%s", tempstdstring.c_str());
//                sprintf(pota_pin.label, spot.value("activator");
                pota_pin.lon=spot.value("longitude", -1);;
                pota_pin.icon=0;
                pota_pin.color=pota_color;
                pota_pin.tooltip[0]=0;
                add_pin(&pota_pin);

                if (c < 9) {
                    std::string mode = spot["mode"].template get<std::string>();
                    std::string strfreq = spot["frequency"].template get<std::string>();
                    double freq = stod(strfreq)/1000;
                    std::string park = spot["reference"].template get<std::string>();





                    pota_color.a = 0;
                    sprintf(tempstr, "%s", pota_pin.label);
                    render_text(surface, panel.texture, &TextRect, font, pota_color, tempstr);
                    TextRect.x += (panel.dims.w/4)+2;
                    sprintf(tempstr, "%4.3f", (freq));
                    render_text(surface, panel.texture, &TextRect, font, pota_color, tempstr);
                    TextRect.x += (panel.dims.w/4)+2;
                    sprintf(tempstr, "%s", mode.c_str());
                    render_text(surface, panel.texture, &TextRect, font, pota_color, tempstr);
                    TextRect.x += (panel.dims.w/4);
                    sprintf(tempstr, "%s", park.c_str());
                    render_text(surface, panel.texture, &TextRect, font, pota_color, tempstr);
                    TextRect.x = 5;
                    TextRect.y += ((panel.dims.h/11)+(panel.dims.h/150));
                    pota_color.a = 200;
                    c++;
                }

            }
        }
    // clean up
    SDL_SetRenderTarget(surface, NULL);
    SDL_RenderTexture(surface, panel.texture, NULL, &(panel.dims));
    }
}


void draw_de_dx(struct ScreenFrame panel, TTF_Font* font, double lat, double lon, int de_dx) {

    if (!surface) {
        SDL_Log("Missing Surface!");
        return ;
    }
    if (!panel.texture) {
        SDL_Log("Missing PANEL!");
        return ;
    }
    char tempstr[64];
    SDL_FRect TextRect;
//    SDL_Surface* textsurface;
//    SDL_Texture *TextTexture;

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
//    SDL_Log("Set Font");

    // blank the box
    SDL_SetRenderTarget(surface, panel.texture);
    SDL_SetRenderDrawBlendMode(surface, SDL_BLENDMODE_NONE);  // Clear solid
    SDL_SetRenderDrawColor(surface, 0, 0, 0, 255);
    SDL_RenderClear(surface);  // Fills the entire target with the draw color
    SDL_SetRenderDrawBlendMode(surface, SDL_BLENDMODE_BLEND);  // Clear solid

    // fet sunrise and sunset times
//    tm* utc = gmtime(&currenttime);
    time_t sunrise;
    time_t sunset;
    double solar_alt;
    // find the next zero crossing for sunrise if current alt <0

    sun_times(lat, lon, &sunrise, &sunset, &solar_alt, currenttime);
    // render the header
    TextRect.x=2;
    TextRect.y=2;
    TextRect.h=(panel.dims.h)/4;
    TextRect.w=(panel.dims.w)-4;
    struct map_pin de_dx_pin;
    if (de_dx) {
        render_text(surface, panel.texture, &TextRect, font, fontcolor, "DE:");
        de_dx_pin.owner=MOD_DE;
        sprintf(de_dx_pin.label, "DE");

    } else {
        render_text(surface, panel.texture, &TextRect, font, fontcolor, "DX:");
        de_dx_pin.owner=MOD_DX;
        sprintf(de_dx_pin.label, "DX");
    }
    de_dx_pin.lat=lat;
    de_dx_pin.lon=lon;
    de_dx_pin.icon=0;
    de_dx_pin.color=fontcolor;
    de_dx_pin.color.a=255;
    de_dx_pin.tooltip[0]=0;
    delete_owner_pins(de_dx_pin.owner);
    add_pin(&de_dx_pin);

    // generate maidenhead grid square

    char latstr[2];
    char lonstr[2];
    char maiden[7];
    maidenhead(lat, lon, maiden);
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

    render_text(surface, panel.texture, &TextRect, font, fontcolor, tempstr);

    // render maidenhead
    TextRect.x=2;
    TextRect.y=(panel.dims.h)/2;
    TextRect.h=(panel.dims.h)/4;
    TextRect.w=panel.dims.w-4;
    sprintf(tempstr, "%s", maiden);

    render_text(surface, panel.texture, &TextRect, font, fontcolor, tempstr);

    // render sunrise time
    TextRect.x=2;
    TextRect.y=((panel.dims.h)/4)*3;
    TextRect.h=(panel.dims.h)/8;
    TextRect.w=(panel.dims.w/3)-4;
    tm* test_time = localtime(&sunrise);
    strftime(tempstr, 12, "R%H:%M", test_time);
    render_text(surface, panel.texture, &TextRect, font, fontcolor, tempstr);

    // render solar angle
    TextRect.x=(panel.dims.w/3)+4;
    TextRect.y=((panel.dims.h)/4)*3;
    TextRect.h=(panel.dims.h)/8;
    TextRect.w=(panel.dims.w/3)-8;
    test_time = localtime(&sunset);
    sprintf (tempstr, "%2.2f", solar_alt);
    render_text(surface, panel.texture, &TextRect, font, fontcolor, tempstr);

    // render sunset time
    TextRect.x=(panel.dims.w/3)*2;
    TextRect.y=((panel.dims.h)/4)*3;
    TextRect.h=(panel.dims.h)/8;
    TextRect.w=(panel.dims.w/3)-4;
    test_time = localtime(&sunset);
    strftime(tempstr, 12, "S%H:%M", test_time);
    render_text(surface, panel.texture, &TextRect, font, fontcolor, tempstr);

    // clean up
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
//    SDL_Log("Rendering Callsign");
    SDL_SetRenderTarget(surface, panel.texture);
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

    SDL_FRect TextRect;
    SDL_Surface* textsurface;
    SDL_Texture *TextTexture;
    TextRect.x=2;
    TextRect.y=2;
    TextRect.h=(panel.dims.h)-4;
    TextRect.w=(panel.dims.w)-4;
    render_text(surface, panel.texture, &TextRect, font, fontcolor, callsign);
    SDL_SetRenderTarget(surface, NULL);
    SDL_RenderTexture(surface, panel.texture, NULL, &(panel.dims));
    TTF_SetFontSize(font,oldsize);
//    SDL_Log("Rendering Callsign Done");
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

void render_pin(struct ScreenFrame *panel, struct map_pin *current_pin) {

    SDL_Texture* icon_tex = SDL_CreateTexture(surface, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, 16, 16);
    if (!icon_tex) {
        SDL_Log("Failed to create icon texture: %s", SDL_GetError());
        return ;
    }
//    SDL_SetTextureBlendMode(icon_tex, SDL_BLENDMODE_BLEND);
    // render the icon
     SDL_SetRenderTarget(surface, icon_tex);
    if (current_pin->icon) {
        SDL_RenderTexture(surface, current_pin->icon, NULL, NULL);
    } else {
        SDL_FRect temprect;

         SDL_SetRenderDrawColor(surface, current_pin->color.r, current_pin->color.g, current_pin->color.b, current_pin->color.a);
         SDL_RenderFillRect(surface, NULL);
         SDL_SetRenderDrawColor(surface, 16, 16, 16, 255);
         SDL_RenderRect(surface, NULL );
         temprect={2,2,15,15};
         SDL_RenderRect(surface, &temprect );
         temprect={3,3,14,14};
         SDL_RenderRect(surface, &temprect );
         temprect={4,4,13,13};
         SDL_RenderRect(surface, &temprect );
    }
    SDL_FRect target_rect;
    target_rect.h=8;
    target_rect.w=8;
    target_rect.x=(current_pin->lon/180.0)*(panel->texture->w/2)+(panel->texture->w/2);
    target_rect.x -= (target_rect.w/2);
    target_rect.y=((-1*current_pin->lat)/90.0)*(panel->texture->h/2)+(panel->texture->h/2);
    target_rect.y -= (target_rect.h/2);
    SDL_SetTextureBlendMode(icon_tex, SDL_BLENDMODE_BLEND);
    SDL_SetRenderTarget(surface, panel->texture);
    SDL_RenderTexture(surface, icon_tex, NULL, &target_rect);
    SDL_SetRenderTarget(surface, NULL);
    SDL_DestroyTexture(icon_tex);
//    SDL_Log("DONE RENDERING PIN");
    return;
}

int draw_map(struct ScreenFrame panel) {


    tm* utc = gmtime(&currenttime);
    double softness = 10.0;
    SDL_Rect panel_cords, source_cords, dest_cords;
    double solar_decl = 23.45 * (sin( (2 * M_PI/365) * (284+(utc->tm_yday+1)) ));
//    SDL_Log("Drawing Map ");
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
//    SDL_Log("Rendered Daymap to texture");

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
    // draw map pins
    if (map_pins) {
        struct map_pin* current_pin;
        current_pin=map_pins;
        while (current_pin) {
//            SDL_Log("rendering pin %i, %i, %i, %i for %i", current_pin->color.r, current_pin->color.g, current_pin->color.b, current_pin->color.a, current_pin->owner);
            render_pin(&panel, current_pin);
            current_pin=current_pin->next;
        }
    }


    // map the result to the window
    SDL_SetRenderTarget(surface, NULL);
    SDL_RenderTexture(surface, panel.texture, NULL, &(panel.dims));
    SDL_Log("Drawing Map Complete\n\n");
    return 0;
}


int draw_clock(struct ScreenFrame panel, TTF_Font* font) {

//    SDL_Log("Drawing Clock");
    SDL_Color fontcolor;
    fontcolor.r=128;
    fontcolor.g=128;
    fontcolor.b=255;
    fontcolor.a=0;
    char timestr[64];
    SDL_FRect TextRect;

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

     // utc
    TextRect.x=2;
    TextRect.y=2;
    TextRect.h=(panel.dims.h/5)*2;
    TextRect.w=((panel.dims.w/5)*2)-4;
    struct tm* utc = gmtime(&currenttime);
    strftime(timestr, sizeof(timestr), "%Y-%m-%d", utc);
    render_text(surface, panel.texture, &TextRect, font, fontcolor, timestr);
    TextRect.x=((panel.dims.w/5)*3)-4;;
    strftime(timestr, sizeof(timestr), "%H:%M:%S %Z", utc);
    render_text(surface, panel.texture, &TextRect, font, fontcolor, timestr);


     // local
    TextRect.x=2;
    TextRect.y=(panel.dims.h/5)*2;
    struct tm* local = localtime(&currenttime);
    strftime(timestr, sizeof(timestr), "%Y-%m-%d", utc);
    render_text(surface, panel.texture, &TextRect, font, fontcolor, timestr);
    TextRect.x=((panel.dims.w/5)*3)-4;;
    strftime(timestr, sizeof(timestr), "%H:%M:%S %Z", utc);
    render_text(surface, panel.texture, &TextRect, font, fontcolor, timestr);
    SDL_SetRenderTarget(surface, NULL);
    SDL_RenderTexture(surface, panel.texture, NULL, &(panel.dims));
    TTF_SetFontSize(font,oldsize);

    return 0;
}