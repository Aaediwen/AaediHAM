#include "map.h"
#include <SDL3_image/SDL_image.h>
#include <mutex>
#include "utils/celestials.h"

aaediclock_host_api* host_api = nullptr;
std::mutex night_mask_mutex;
struct map_layer day_map;
struct map_layer night_map;
struct map_layer countries_map;
aaediclock_image night_mask = {0,0,nullptr};
uint32_t night_mask_texture;
bool night_mask_refresh = false;
SDL_TimerID night_mask_timer = 0;
aaediclock_FRect old_dims = {0,0,0,0};

void load_day_map (const aaediclock_FRect& dims) {
    //reset everything
    if (day_map.surface) {
        SDL_DestroySurface(day_map.surface);
        day_map.surface = nullptr;
   }
   if (host_api->AaediHAM_TextureCheck(day_map.texture)) {
       host_api->AaediHAM_TextureDelete(day_map.texture);
       day_map.texture = 0;
   }
   SDL_Surface* temp_surface = IMG_Load("images/Blue_Marble_2002.bmp");

   if (temp_surface) {
       day_map.surface = SDL_CreateSurface(static_cast<int>(floor(dims.w)), static_cast<int>(floor(dims.h)), SDL_PIXELFORMAT_RGBA32);
       if (day_map.surface) {
           if (SDL_BlitSurfaceScaled(temp_surface, NULL, day_map.surface, NULL, SDL_SCALEMODE_LINEAR)) {
           aaediclock_image new_image;
           new_image.width = static_cast<uint16_t>(floor(dims.w));
           new_image.height = static_cast<uint16_t>(floor(dims.h));
           new_image.pixels = static_cast<uint8_t*>(day_map.surface->pixels);
           day_map.texture = host_api->AaediHAM_TextureCreate(new_image);
           if (!host_api->AaediHAM_TextureCheck(day_map.texture)) {
               *(host_api->AaediHAM_LogDebug) << "Error Creating DayMap Texture "<< SDL_GetError() <<"\n";
               SDL_DestroySurface(day_map.surface);
               day_map.surface = nullptr;
           }
       } else {
           *(host_api->AaediHAM_LogDebug) << "Error Scaling DayMap: "<< SDL_GetError() <<"\n";
       }
   } else {
           *(host_api->AaediHAM_LogDebug) << "Unable to load Day Map texture from disk\n";
   }
   SDL_DestroySurface(temp_surface);
   temp_surface = nullptr;
   } else {
        *(host_api->AaediHAM_LogDebug) << "Unable to load Day Map texture from disk\n";
   }
    return;
}

void load_night_map (const aaediclock_FRect& dims) {
    if (night_map.surface) {
        SDL_DestroySurface(night_map.surface);
        night_map.surface = nullptr;
    }
    if (host_api->AaediHAM_TextureCheck(night_map.texture)) {
        host_api->AaediHAM_TextureDelete(night_map.texture);
        night_map.texture = 0;
    }
    SDL_Surface* temp_surface = IMG_Load("images/Black_Marble_2016.bmp");
    if (temp_surface) {
        night_map.surface = SDL_CreateSurface(static_cast<int>(floor(dims.w)), static_cast<int>(floor(dims.h)), SDL_PIXELFORMAT_RGBA32);
        if (night_map.surface) {
            if (SDL_BlitSurfaceScaled(temp_surface, NULL, night_map.surface, NULL, SDL_SCALEMODE_LINEAR)) {
                *(host_api->AaediHAM_LogDebug) << "Scaled Night Map to NightMap surface!\n";
            } else {
                *(host_api->AaediHAM_LogDebug) << "Error scaling Night Map to NightMap surface!\n";
                SDL_DestroySurface(night_map.surface);
                night_map.surface = nullptr;
            }
        }
        SDL_DestroySurface(temp_surface);
        temp_surface = nullptr;
    } else {
        *(host_api->AaediHAM_LogDebug) << "Unable to load Night Map texture from disk\n";
    }
    return;
}

void load_countries_map (const aaediclock_FRect& dims) {
    if (countries_map.surface) {
        SDL_DestroySurface(countries_map.surface);
        countries_map.surface = nullptr;
    }
    if (countries_map.texture) {
        host_api->AaediHAM_TextureDelete(countries_map.texture);
        countries_map.texture = 0;
    }
    SDL_Surface* temp_surface = IMG_Load("images/outline.bmp");
    if (temp_surface) {
        countries_map.surface = SDL_CreateSurface(static_cast<int>(floor(dims.w)), static_cast<int>(floor(dims.h)), SDL_PIXELFORMAT_RGBA32);
        if (countries_map.surface) {
            if (SDL_BlitSurfaceScaled(temp_surface, NULL, countries_map.surface, NULL, SDL_SCALEMODE_LINEAR)) {
                // alpha mask the political map
                int x, y;
                x=0;
                y=0;
                Uint8 cg, cr, cb;
                Uint8* country_pixels = (Uint8*)countries_map.surface->pixels;
                const SDL_PixelFormatDetails* FormatDetails = SDL_GetPixelFormatDetails(countries_map.surface->format);
                const Uint8 bpp = FormatDetails->bytes_per_pixel;
//                const Uint8 bpp = SDL_GetPixelFormatDetails(countries_map.surface->format)->bytes_per_pixel;
            /*
            about the following FOR loop:
            --------------------------------

            I am specifically leaving this manual code in here and NOT subsequently cleaning up CountriesMap.surface

            SDL_SetSurfaceColorKey() can do this kind of Alpha keying
            However, it does not obey anti-aliasing which exists on the country borders
            This code DOES work with the anti-aliasing.

            After CountriesMap.texture is created, no other code actually cares about CountriesMap.surface
            However, Other code does check for its existance as a sanity check that this routine ran correctly
            I *could* nuke the surface and create a stub 1x1 surface, but I don't think it's worth the savings in system RAM
            */

                for ( y = 0; y < countries_map.surface->h ; y++) {
                    for (x = 0 ; x < countries_map.surface->w ; x++) {
                        // get where the pixel lives
                        int pixel_index = countries_map.surface->pitch*y + ( bpp * x );
                        // read its color values
                        Uint32 *pixel_val=(Uint32*)(pixel_index+country_pixels);
                        SDL_GetRGBA( *pixel_val, FormatDetails, NULL, &cr, &cg, &cb, NULL);
//                        SDL_GetRGBA( *pixel_val, SDL_GetPixelFormatDetails(countries_map.surface->format), NULL, &cr, &cg, &cb, NULL);
                        // write the new value back
                        Uint32 pixel_val_out = SDL_MapRGBA(FormatDetails, NULL, 0, 0, 0, (255-cg));
//                        Uint32 pixel_val_out = SDL_MapRGBA(SDL_GetPixelFormatDetails(countries_map.surface->format), NULL, 0, 0, 0, (255-cg));
                        memcpy((country_pixels + pixel_index), &pixel_val_out, bpp);
                    }
                }
                aaediclock_image new_image;
                new_image.width = static_cast<uint16_t>(floor(dims.w));
                new_image.height = static_cast<uint16_t>(floor(dims.h));
                new_image.pixels = static_cast<uint8_t*>(countries_map.surface->pixels);
                countries_map.texture = host_api->AaediHAM_TextureCreate(new_image);
                if (!host_api->AaediHAM_TextureCheck(countries_map.texture)) {
                                                                                                                                                    *(host_api->AaediHAM_LogDebug) << "Error Creating Countries Texture "<< SDL_GetError() <<"\n";
                    SDL_DestroySurface(countries_map.surface);
                    countries_map.surface = nullptr;
                }
            } else {
                *(host_api->AaediHAM_LogDebug) << "Error scaling Night Map to NightMap surface!\n";
                SDL_DestroySurface(countries_map.surface);
                countries_map.surface = nullptr;
            }
        }
        SDL_DestroySurface(temp_surface);
        temp_surface = nullptr;
    } else {
        *(host_api->AaediHAM_LogDebug) << "Unable to load Countries texture from disk\n";
    }
    return;
}

int SDLCALL regen_night_mask(void* userdata) {
    (void) userdata;
    aaediclock_FRect dims = host_api->AaediHAM_GetMapSize();
    if ((dims.h < 10) || (dims.w < 10)) {
        return 0;
    }
    const std::lock_guard<std::mutex>night_mask_lock(night_mask_mutex);
    if (!night_map.surface) {
        load_night_map(dims);
    }
    if (!night_map.surface) {
        *(host_api->AaediHAM_LogDebug) << "No Night Map when generating mask\n";
        return 0;
    }
//    std::cout << "night_mask_pixels "<< night_mask.pixels <<"\n";
    // recreate a new surface
    if (night_mask.pixels != nullptr) {
        night_mask.height = 0;
        night_mask.width = 0;
        free(night_mask.pixels);
        night_mask.pixels = nullptr;
    }
    night_mask.height = static_cast<uint16_t>(floor(dims.h));
    night_mask.width = static_cast<uint16_t>(floor(dims.w));
    night_mask.pixels = (uint8_t*)malloc(night_mask.height*night_mask.width*4);
    if (!night_mask.pixels) {
        *(host_api->AaediHAM_LogDebug) << "Failed to create mask surface\n";
        night_mask.pixels = nullptr;
        return 0;
    }
    const time_t nowtime = time(nullptr);
    *(host_api->AaediHAM_LogDebug) << "Regenerating Night Mask at "<< nowtime << "\n";
        struct tm utc;
#ifdef _WIN32
    gmtime_s(&utc, &nowtime);
#else
    gmtime_r(&nowtime, &utc);
#endif
    // variables
    aaediclock_FRect panel_cords, source_cords;
    const double softness = 10.0;
    const Uint8* source_pixels = (Uint8*)(night_map.surface->pixels);
    const SDL_PixelFormatDetails* source_details = SDL_GetPixelFormatDetails(night_map.surface->format);
    const Uint8 source_bpp = source_details->bytes_per_pixel;
    const double solar_decl = 23.45 * (sin( (2 * M_PI/365) * (284+(utc.tm_yday+1)) ));
    for (panel_cords.y=0.0 ; panel_cords.y < floor(dims.h) ; panel_cords.y++) {
        source_cords.y = static_cast<uint16_t>((panel_cords.y/dims.h)*night_map.surface->h);
        double lat = 90.0 - (180.0 * panel_cords.y / (double)dims.h);
        for (panel_cords.x=0 ; panel_cords.x < floor(dims.w) ; panel_cords.x++) {
            uint8_t r, g, b, alpha;
            double lon = -180.0 + (360.0 * panel_cords.x / (double)dims.w);
            double alt = solar_altitude(lat, lon, &utc, solar_decl);
            // calculate per pixel alpha
            if (alt > softness) {
                alpha = 255;
            } else if (alt < -softness) {
                alpha = 0;
            } else {
                alpha = (Uint8)(255.0 * (alt + softness) / (2.0 * softness));
            }
            // Write a pixel with the computed alpha
            source_cords.x = static_cast<int>((panel_cords.x/dims.w)*night_map.surface->w);
            int source_pixel_index = ( night_map.surface->pitch * (int)source_cords.y ) + ( source_bpp * (int)source_cords.x );
            int dest_pixel_index =   ( (int)dims.w * 4 * (int)panel_cords.y ) + ( 4 * (int)panel_cords.x );
//            *(host_api->AaediHAM_LogDebug) << "src: " << source_cords.x << ", " << source_cords.y << "\tdst: "
//                                           << panel_cords.x << ", " << panel_cords.y << "\t"
//                                           << "indices " << source_pixel_index << ", " << dest_pixel_index << "\n";
            Uint32 *source_pixel_val=(Uint32*)(source_pixel_index+(uint8_t*)source_pixels);
            SDL_GetRGBA( *source_pixel_val, source_details, NULL, &r, &g, &b, NULL);
            night_mask.pixels[dest_pixel_index]=r;
            night_mask.pixels[dest_pixel_index+1]=g;
            night_mask.pixels[dest_pixel_index+2]=b;
            night_mask.pixels[dest_pixel_index+3]=255-alpha;
        }
    }
    *(host_api->AaediHAM_LogDebug) << "MAP_REGEN: Night Mask Regen Complete\n";
    night_mask_refresh = true;
    return 0;
}

Uint32 SDLCALL night_mask_spawn(void* userdata, SDL_TimerID timerID, Uint32 interval) {
    (void)userdata;
    (void)interval;
    if (timerID) {
        SDL_Thread* thread = SDL_CreateThread(regen_night_mask, "Map Night Mask Regen", nullptr);
        if (thread) {
            SDL_DetachThread(thread);
        } else {
            *(host_api->AaediHAM_LogDebug) << "Failed to Create Night Mask Regen Thread\n";
        }
        return(30000);
    } else {
        return(0);
    }
}

extern "C" DllExport aaediclock_plugin_api* createPlugin() {
    return new map_plugin();
}
extern "C" DllExport void destroyPlugin(aaediclock_plugin_api* target) {
    if (target) {
        delete target;
    }
}

void map_plugin::plugin_init() const {
     night_mask.pixels = nullptr;
     if (!night_mask_timer) {
          night_mask_timer = SDL_AddTimer(30, night_mask_spawn, NULL);
     }

    return;
}

void map_plugin::plugin_exit() const {
    std::cout << "Freeing MAP resources\n";
    if (night_mask_timer) {
        SDL_RemoveTimer(night_mask_timer);
    }
    const std::lock_guard<std::mutex>night_mask_lock(night_mask_mutex);
    if (night_mask.pixels != nullptr) {
        night_mask.height = 0;
        night_mask.width = 0;
        free(night_mask.pixels);
        night_mask.pixels = nullptr;
    }

    if (day_map.surface) {
        SDL_DestroySurface(day_map.surface);
        day_map.surface = nullptr;
   }
   if (host_api->AaediHAM_TextureCheck(day_map.texture)) {
       host_api->AaediHAM_TextureDelete(day_map.texture);
       day_map.texture = 0;
   }
   if (night_map.surface) {
        SDL_DestroySurface(night_map.surface);
        night_map.surface = nullptr;
   }
   if (host_api->AaediHAM_TextureCheck(night_map.texture)) {
       host_api->AaediHAM_TextureDelete(night_map.texture);
       night_map.texture = 0;
   }
   if (countries_map.surface) {
        SDL_DestroySurface(countries_map.surface);
        countries_map.surface = nullptr;
   }
   if (host_api->AaediHAM_TextureCheck(countries_map.texture)) {
       host_api->AaediHAM_TextureDelete(countries_map.texture);
       countries_map.texture = 0;
   }
   if (host_api->AaediHAM_TextureCheck(night_mask_texture)) {
       host_api->AaediHAM_TextureDelete(night_mask_texture);
   }

    return;
}

void map_plugin::plugin_main(const aaediclock_FRect& dims) const {
    // input sanitization
    if ((dims.w < 10) || (dims.h < 10)) {
        return;
    }
    // resize handler
    if ((dims.h != old_dims.h) || (dims.w != old_dims.w)) {
        old_dims.h = dims.h;
        old_dims.w = dims.w;
        const std::lock_guard<std::mutex>night_mask_lock(night_mask_mutex);
        if (day_map.surface) {
            SDL_DestroySurface(day_map.surface);
            day_map.surface = nullptr;
        }
        if (day_map.texture) {
            host_api->AaediHAM_TextureDelete(day_map.texture);
            day_map.texture = 0;
        }
        if (night_map.surface) {
            SDL_DestroySurface(night_map.surface);
            night_map.surface = nullptr;
        }
        if (night_map.texture) {
            host_api->AaediHAM_TextureDelete(night_map.texture);
            night_map.texture = 0;
        }
        if (countries_map.surface) {
            SDL_DestroySurface(countries_map.surface);
            countries_map.surface = nullptr;
        }
        if (countries_map.texture) {
            host_api->AaediHAM_TextureDelete(countries_map.texture);
            countries_map.texture = 0;
        }
    }

    // load and render the base maps
    if (!host_api->AaediHAM_TextureCheck(day_map.texture)) {
        load_day_map(dims);
    }
    if (!host_api->AaediHAM_TextureCheck(countries_map.texture)) {
        load_countries_map(dims);
    }
    host_api->AaediHAM_OverlaySet(dims,OVERLAY_BACKGROUND);
    host_api->AaediHAM_OverlayClear({0,0,0,255});
    if (host_api->AaediHAM_TextureCheck(day_map.texture)) {
        host_api->AaediHAM_GraphicsDrawImage(day_map.texture);
    }
    if (night_mask_refresh) {
        if (host_api->AaediHAM_TextureCheck(night_mask_texture)) {
            host_api->AaediHAM_TextureDelete(night_mask_texture);
        }
        night_mask_texture = host_api->AaediHAM_TextureCreate(night_mask);
        night_mask_refresh = false;
    }
    if (host_api->AaediHAM_TextureCheck(night_mask_texture)) {
        host_api->AaediHAM_GraphicsDrawImage(night_mask_texture);
    }
    if (host_api->AaediHAM_TextureCheck(countries_map.texture)) {
        host_api->AaediHAM_GraphicsDrawImage(countries_map.texture);
    }

    // draw equator and tropics
    aaediclock_FRect tropic;
    tropic.x = 0.0;
    tropic.y = dims.h/2.0;
    tropic.w = dims.w;
    tropic.h = dims.h/2.0;
    host_api->AaediHAM_GraphicsDrawLine(aaediclock_Color{128, 128, 128, 64}, tropic);
//    *(host_api->AaediHAM_LogDebug) << "Equatorial dims: " << tropic.x << ", " << tropic.y << " " << tropic.w << ", " << tropic.h << "\n";

//    SDL_SetRenderDrawColor(panel.GetRenderer(), 128,128,128,64);
//    SDL_RenderLine(panel.GetRenderer(), 0,(panel.dims.h/2), panel.dims.w, (panel.dims.h/2));
//    SDL_SetRenderDrawColor(panel.GetRenderer(), 128,0,0,64);
    tropic.y = ((-23.4f+90.0f) * dims.h)/180.0f;
    tropic.h = tropic.y;
    host_api->AaediHAM_GraphicsDrawLine(aaediclock_Color{128, 0, 0, 64}, tropic);
//    *(host_api->AaediHAM_LogDebug) << "Tropical dims: " << tropic.x << ", " << tropic.y << " " << tropic.w << ", " << tropic.h << "\n";
//    tropic = ((23.4f+90.0f) * panel.dims.h)/180.0f;
//    SDL_RenderLine(panel.GetRenderer(), 0,tropic, panel.dims.w, tropic);
    tropic.y = ((23.4f+90.0f) * dims.h)/180.0f;
    tropic.h = tropic.y;
    host_api->AaediHAM_GraphicsDrawLine(aaediclock_Color{128, 0, 0, 64}, tropic);
//    *(host_api->AaediHAM_LogDebug) << "Tropical dims: " << tropic.x << ", " << tropic.y << " " << tropic.w << ", " << tropic.h << "\n";
    return;
}

const char* map_plugin::getName() const {
    return "Map Module";
}

void map_plugin::set_host(aaediclock_host_api* host) {
    host_api = host;
}

